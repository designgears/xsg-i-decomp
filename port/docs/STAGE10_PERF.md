# PC port, Stage 10: optimised build, profiling, VU1 speed

Branch `pc-port` (agent worktree), 2026-11-07. Builds on [STAGE9_GPU.md](STAGE9_GPU.md).

**Result:** the default build is now `Release` (-O2 -g) and it runs through the title, the
cutscene and into the field. The Release crash had several causes, all of them undefined
behaviour that -O0 happened to survive. A sampling profiler shows that the VU1 interpreter
takes about 75% of the game thread in 3D. The interpreter is now about 20% faster per pair,
and its output is bit-identical. The cutscene runs at about 36 vblanks/s in real time,
up from 30. 60/s is **not** reached yet: VU1 is still the limit (see §4).

**Update (§6):** with a pre-decoded VU1 interpreter, flag-liveness elision and bulk VIF1
UNPACK, the cutscene and the field now hold **60 vblanks/s in real time** (75/s with
XSG_FAST). The output is still bit-identical. VU1 was not moved off the game thread: that
was not needed.

## 1. The Release crash: root causes

| # | symptom | cause | fix |
|---|---|---|---|
| 1 | SIGSEGV at 0x3, frame 16 | Data tables name file-static functions (`_BallVsBall`, `_IsAction*`, …). link_xsg.py globalises those functions. At -O1 and above, GCC drops or inlines the unreferenced statics, so the link resolved the table entries to raw PS2 addresses (`--defsym …=0x2..`). | `-fkeep-static-functions -fkeep-static-consts` for game C. autostubs.txt is now the same as in the -O0 build. |
| 2 | smashed return path / jump into the middle of an instruction (frame ~1194). `-fstack-protector-all` reports it in `xglCdStreamOpen`. | `CdFilePositionView` (0x2c bytes) receives `xglCdGetFilePos`'s 0x30-byte record. The write of `size_bytes` overran the frame. The PS2 frame padding absorbed it. | xgl_cd.c: add the field (`PORT_HOST`). |
| 3 | `Java_xeno_Effect_setRotate__` writes to 0x90 (frame 1166; this is also the old -O3 crash in `virtualMachine`) | `void sefCreateEffectCf(…) { sefCreateEffectCf2(…); }`, but `getPeer_Effect` uses the result. On the PS2 the tail call leaves v0 in place. On the host %eax survived at -O0 only by luck. | Return the value (`PORT_HOST`). `tools/scan_void_results.py` lists every void definition whose result a caller uses. The forwarders with real callers are fixed the same way: `objEntry`, `dataAccBoxInc`, `dataEvtBoxInc`, `RgBxxGetHeader`, `RgBxxGetXtx`, `RgColiDataVsBall`, `RgBattleInitCreateBg`, `CardEnemyCommandRequest`, `CardEnemy1stAnswer`. |
| 4 | endless loop in `xglFontFlushCore` (frame 1106) | Several C names for one symbol: `extern FontStateViewStorage FS_view __asm__("FS")` next to `extern unsigned char FS[]`, `D_queue` = `D_008811A0`. GCC treats distinct declarations as distinct objects and reorders or forwards loads and stores between them. `-fno-strict-aliasing` does not cover this. Minimal repro: `A[0]=1; B.x=2; return A[0];` with `B __asm__("A")` returns 1 at -O2. | `scripts/find_alias_views.py` finds the 33 game files that declare an object through an `__asm__` label, or that use a link alias (`D_426F48 = batDatBuf+8`) together with its base symbol. CMake builds those at -O0 and the rest at -O2. |

How they were found: per-file -O0 bisection (`-DXSG_O0_REGEX=…`, `tools/o0_bisect_try.sh`)
showed that the failures moved with layout. `-ftrivial-auto-var-init=zero` and
`-DXSG_SSP=-fstack-protector-all` turned the heisenbugs into deterministic faults (#2, #3).
Fibers, volatile and the DMA flags were not involved.

Open points: the alias-view files stay at -O0. Rewriting their views (for example a
laundered pointer instead of a second declaration) would let them be optimised.
`CardEnemyCommandPlay` → `CC_Kara_CommPlay` is void at both levels and was left alone.
-O3 was not retested.

## 2. Build

```
cmake -S port -B port/build -G Ninja     # Release (-O2 -g) by default
cmake -S port -B port/build-dbg -DCMAKE_BUILD_TYPE=Debug   # all -O0
#   -DXSG_ALIAS_VIEWS_O0=OFF   optimise the alias-view files too (unsafe)
#   -DXSG_SSP=-fstack-protector-all   find frame overruns
#   -DXSG_O0_REGEX=<regex>     build more game files at -O0 (bisection)
```

The alias-view list is computed at configure time. Re-run cmake after adding such a declaration.

## 3. Profiling

`XSG_PROF=<file>` (port/runtime/prof.c) samples the game thread through a CLOCK_MONOTONIC
timer (`XSG_PROF_HZ`, default 1000; `XSG_PROF_START=<vblank>`). Each sample stores the pc
and a backtrace(). Unwinding is skipped when the pc is outside our binary, because libgcc
faults in libc's memcpy. The sample file is written at exit or at XSG_TIMEOUT:

```
XSG_PROF=/tmp/p.txt XSG_PROF_START=1400 XSG_TIMEOUT=70 <field recipe> port/build/xsg
python3 port/tools/prof_report.py port/build/xsg /tmp/p.txt -n 30
```

Cutscene (vblanks 1400–2400, real time, Release, old interpreter), 40k samples:

| self | function |
|---|---|
| 22% | vu1 `Arith` |
| 21% | vu1 `Run` loop |
| 14% | vu1 `Upper` |
| 10% | vu1 `Commit` (+ flags) |
| 5% | vu1 `LandFlags` |
| 12% | libc, idle (vblank wait) |
| 8% | gs_bridge `VifData` (UNPACK) |
| ~3% | GIF ring (`xsggpu::Gif/GifBlock/Room`) |
| <1% each | VU0 model, game C (`JNT_*`, `ACT_*`) |

Inclusive: `xsgvu::Run` 77%, everything under `xglPacketSend` → DMA → VIF 85%. The software
GS does no work in gpu mode. Logging, the frame capture and the stream do not show up.

## 4. VU1 interpreter

* FMAC ops (`Arith`) now work on all four lanes with SSE4.1. The In/Out clamping and the MAC
  bits are computed by vector masks, and only the dest lanes keep their bits. These are the
  same single-precision ops under the same MXCSR, so the results are identical.
* Commits blend the dest lanes in one 128-bit store.
* MAC/status/clip are landed on demand: before a lower op that reads or writes them
  (op7 0x10–0x1c), before DIV's status write, and at program end. Previously they were
  landed every pair. With a latency below 16 this gives the same values.
* **Check:** `test_vu1_replay` (`cmake --build port/build --target test_vu1_replay`) replays
  `XSG_VU1_CAPTURE` files and compares, for every run, the pair count, the stop pc and every
  XGKICK packet byte for byte. On 2 cutscene frames (1400, 1550; 20,885 runs; 9.0 M pairs)
  captured with the previous interpreter, there are **0 differences**.
* Replay speed (same files): 60 → 70 M pairs/s.

A capture frame holds 9–11k runs and 3.6–5.4 M pairs, which is 50–80 ms of interpreter time
at 70 M pairs/s. To reach 60 vblanks/s (a 30 fps game with a 33 ms budget) this needs
another 3–4×. The next steps, in order:

1. A pre-decoded threaded interpreter: a per-pc cache of {handler, operands}, validated
   against the raw words, with specialised handlers per opcode and dest mask.
2. Skip the flag work for FMAC results that no FSAND/FCAND can observe before they are
   overwritten. A static pass over the microcode can show which.
3. Run VU1 off the game thread. The DMA kick would have to return before the run, with
   ordering kept against PATH2/PATH3.

## 5. Speed (XSG_FPS, gpu headless, field recipe)

Measured with `port/tools/fps_run.sh` and given as vblanks/s / game frames/s. "Before" is
the Debug build (-O0) with the old interpreter. "After" is this Release build. The recipe
stays in the 3D cutscene/Virtual Tutorial scenes from about vblank 1500 through 8000+, and
the field part is in the same range (vblank 4500+).

| scene | before, real time | after, real time | after, XSG_FAST |
|---|---|---|---|
| title (250–1000) | 60 / 30 | 60 / 30 | 545–1745 / 270–870 |
| cutscene (1750–3000) | 30.5 / 15.2 | 36.3 / 18.2 | 40.4 / 20.2 |
| field (4500–7000) | 30.3 / 15.1 | 36.4 / 18.2 | 40.3 / 20.2 |

That is +20% in 3D, and still short of 60.

## 6. Decoded VU1 interpreter, flag liveness, bulk UNPACK (Stage 10b)

### Decode cache (port/runtime/vu1.cpp, `RunFast`)

* The 16 KB of micro memory is decoded into one 64-byte `DP` record per pair. Each record
  holds the upper and lower handler ids, the operands already extracted (VF/VI register
  pointers, dest masks as blend vectors, the dest mask repeated over the 4 MAC groups,
  immediates, precomputed branch targets), and the raw words.
* **Cache:** a map from a 64-bit hash of the micro memory to the decoded program. Each executed
  pair compares its two raw words with micro memory. On a mismatch (MPG upload, a different
  set) the program is looked up again by hash (memcmp guards against collisions) or decoded
  afresh. No hook in the VIF is needed. There are 4 lookups across the 4 capture frames,
  because the seven microcode sets are re-uploaded with the same contents.
* **Dispatch:** two `switch`es over always-inline handlers in one loop. They replaced two
  indirect calls per pair, which on i386 pass the record on the stack.
* **Specialised:** the FMAC ops are templates per op (ADD SUB MUL MADD MSUB MAX MINI, and
  the ACC forms) and per operand form (vector, bc, q, i). They use one dest blend. The MAC
  bits come from a single pack + pshufb + movemask. Also specialised: ITOF/FTOI (cvtdq2ps /
  cvttps2dq with saturation), CLIP, LQ/SQ/LQI/SQI, ILW/ISW, IADDIU/ISUBIU/IADD/ISUB/IADDI/
  IAND/IOR, MOVE/MR32/MFIR/MTIR/MFP, all branches, XGKICK/XTOP/XITOP and the flag readers.
  The rare ops (EFU, LQD/SQD, R*, OPMULA/OPMSUB, ABS, DIV) call the reference
  `Upper`/`Lower` from the fast loop.
* **Pair ordering:** the fast path runs the upper op first and commits it straight away.
  That is equivalent unless the lower op reads or writes the upper's target VF register, or
  is FSSET/FCSET (they write the flags that the upper's write would snapshot). Such pairs,
  2% of those executed, run the reference Upper → Lower → Commit sequence. DIV/SQRT/RSQRT
  and the flag readers can stay in direct order, for two reasons: queued entries' I/D bits
  are masked when they land, and an entry pushed in the same pair is never due yet
  (latency ≥ 1).

### Flag liveness

A flag write `p` (FMAC MAC/status, CLIP) is put on the latency queue only when something
could observe it. It is skipped when both of these hold:

* the next flag write `q` follows in straight-line code, with no branch delay slot and no
  end-of-program pair in `[p, q+lat-2]`;
* no lower reader lands the queue in `[p+lat, q+lat-1]`. The readers are op7 0x10–0x1c and
  DIV/SQRT/RSQRT.

In that case no reader can see `p` as the newest landed entry, and neither can the end of
the program. `mac_l`/`status_l`/`clip_l`, which carry the sticky bits, are always updated,
so every queued snapshot keeps the same contents. 74% of the queue pushes are skipped. The
4-pair latency model is unchanged: the fast path is used for `XSG_VU_FLAGLAT` 1–15, and
latency 0 or ≥ 16 runs the reference loop.

### Found on the way

`std::sqrt` in `Lower` was inlined as `sqrtss` only by luck of inlining. Once `Lower` had
more callers, GCC emitted a call to i386 libm `sqrtf` instead. That is x87 `fsqrt`, which
rounds to nearest and not with MXCSR round-toward-zero, so ERLENG's P changed in the last
bit. It is now `_mm_sqrt_ss` explicitly (`SqrtF`).

### VIF1 UNPACK

`VifData` fed UNPACK data to `UnpackElement` one byte at a time, which was 15% of the game
thread. `VifWords` now passes whole runs of elements to `VifBulk`. A run starts on an element
boundary at a word boundary; a partial word at the end is carried into `upk_buf`. Plain
V4-32 (no mask, mode 0) is a 16-byte copy. MPG data is copied in bulk as well.

### Checks

* `test_vu1_replay` now also checks the **full register state**: the registers after run i
  must equal the entry registers of run i+1 (vf, acc, vi, I/Q/P/R, MAC/status/clip). On 4
  frames captured with the old interpreter (vblanks 1400, 2600, 3800, 5000; 43,805 runs;
  16.2 M pairs) there are **0 differences** in pairs, stop pc, XGKICK packets and state. This
  holds for the fast path and for `XSG_VU_FAST=0` (the reference loop).
  `--bisect` runs both paths per run and reports the first pair that differs.
* **In-game end to end:** two cutscene frames captured with the old and the new binary
  (18,850 runs) have identical run headers, entry registers and XGKICK packets
  (`XSG_VU1_CAPTURE`, compared record by record). The only bytes that differ in the data
  memory are host pointers inside DMA tags, which move with the binary layout.
* `test_vu1_ref` on the new captures: every supported program is exact, 0 fail.
* Replay speed: 70 → 115 (cutscene ~135) → ~127 M pairs/s.

### Speed (field recipe, XSG_FPS=600, gpu headless, Release)

| scene | before (§5 build), real time | after, real time | before, XSG_FAST | after, XSG_FAST |
|---|---|---|---|---|
| title | 60 | 60 | 310–490 | 310–730 |
| cutscene (1800–3600) | 30.0–33.3 | **60.0** | 33.5–38.1 | 74.5–75.7 |
| field (4200–9600) | 28.1–34.8 | **60.0** (59.5 once) | 32.6–38.2 | 73.2–75.9 |

The real-time window that contains the scene load (1200–1800) shows 54.5.

### Profile after (XSG_FAST, vblank 2000+, 85k samples)

| self | function |
|---|---|
| 72% | `RunFast` (VU1, handlers inlined) |
| 7% | `VifWords` (UNPACK, inlined) + 1.6% `UnpackElement` |
| 5.6% | GIF ring (`xsggpu::GifBlock`/`Gif`/`Room`) |
| 1.7% | `LandFlags` |
| ~3% | reference `Upper`/`Lower`/`Arith`/`Commit` (slow pairs, rare ops) |
| 1.2% | `vu0_arith` |

Inclusive: `xsgvu::Run` 83%. VU1 is still most of the frame, but at 75 vblanks/s there is
about 25% headroom over 60. What remains:

1. More of the FMAC cost (InV on both operands, the MAC pack) for MADDbc/MULbc, the hottest
   forms. The FMAC handlers are about half of `RunFast`.
2. The non-UNPACK VIF paths (DIRECT goes through `GsQword` one qword at a time).
3. Off-thread VU1 is still possible if more headroom is needed. A worker would have to own
   the whole VIF1 stream (UNPACK writes race with a running program), PATH3/ch2 would go
   through the same queue to keep the GS order, and there would be a sync at vsync. It was
   not needed for 60.

`XSG_VU_FAST=0` selects the reference loop (for checks). `XSG_VU_TRACE`/`XSG_VU_HIST` also
use it.
