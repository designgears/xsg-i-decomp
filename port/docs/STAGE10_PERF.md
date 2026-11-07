# PC port, Stage 10: optimised build, profiling, VU1 speed

Branch `pc-port` (agent worktree), 2026-11-07. Builds on [STAGE9_GPU.md](STAGE9_GPU.md).

**Result:** the default build is now `Release` (-O2 -g) and it runs through the title, the
cutscene and into the field. The Release crash had several causes, all of them undefined
behaviour that -O0 happened to survive. A sampling profiler shows that the VU1 interpreter
takes about 75% of the game thread in 3D. The interpreter is now about 20% faster per pair,
and its output is bit-identical. The cutscene runs at about 36 vblanks/s in real time,
up from 30. 60/s is **not** reached yet: VU1 is still the limit (see §4).

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
