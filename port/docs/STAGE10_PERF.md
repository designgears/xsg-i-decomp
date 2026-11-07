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
| 4 | endless loop in `xglFontFlushCore` (frame 1106) | Several C names for one symbol: `extern FontStateViewStorage FS_view __asm__("FS")` next to `extern unsigned char FS[]`, `D_queue` = `D_008811A0`. GCC treats distinct declarations as distinct objects and reorders or forwards loads and stores between them. `-fno-strict-aliasing` does not cover this. Minimal repro: `A[0]=1; B.x=2; return A[0];` with `B __asm__("A")` returns 1 at -O2. | PORT_HOST side of every view: `extern T xsg_view_X __asm__("sym"); #define X XSG_VIEW(xsg_view_X)` (port/include/ps2_host_compat.h). XSG_VIEW goes through a pointer laundered by an empty asm, which GCC must assume aliases every global. Link aliases become offsets into the base (`#define UmnDataBaseStateData (SaveData + 66140)`); labels inside FS become `FS + off` (see §1a). `scripts/find_alias_views.py` fails the configure if an unguarded view is left. All game files are -O2. |

How they were found: per-file -O0 bisection (`-DXSG_O0_REGEX=…`, `tools/o0_bisect_try.sh`)
showed that the failures moved with layout. `-ftrivial-auto-var-init=zero` and
`-DXSG_SSP=-fstack-protector-all` turned the heisenbugs into deterministic faults (#2, #3).
Fibers, volatile and the DMA flags were not involved.

Open points: `CardEnemyCommandPlay` → `CC_Kara_CommPlay` is void at both levels and was left alone.

### 1a. Alias views at -O2 (2026-11-07)

The 33 files that were built at -O0 (about 120 declarations: arrays, structs,
pointers, block-scope externs, `__asm__("D_28-0x28")` and `"D_00A57DF0+2"`
labels with offsets) are rewritten mechanically on the PORT_HOST side:

```
#ifdef PORT_HOST
extern FontStateViewStorage xsg_view_FS_view __asm__("FS");
#define FS_view XSG_VIEW(xsg_view_FS_view)   /* (*(__typeof__(v) *)xsg_launder(&v)) */
#else
extern FontStateViewStorage FS_view __asm__("FS");
#endif
```

`__typeof__` keeps the declared type, so `sizeof`, array indexing, `&X` and
assignment to pointer views behave as before. The PS2 side is the original
text. The minimal repro (`A[0]=1; B.x=2; return A[0];`) returns 2 at -O3 with it.
Function aliases (`int f(void) __asm__("g")`) are left alone: they name code,
not storage, so GCC caches nothing across them.

`scripts/find_alias_views.py src` is now a checker (CMake runs it at configure
time, `FATAL_ERROR` on failure). On the host side of each file (PS2 branches of
`#ifdef/#ifndef PORT_HOST` are dropped) it reports any `extern` object with an
asm label that is not `xsg_view_*`/`xsg_sym_*`, a labelled definition whose
symbol has another C name in the file, an `xsg_view_*` used outside
`XSG_VIEW()`, and a link alias from `build/*/aliases.ld` used next to its base
(unless a host-side `#define` replaces it). It found three views the old list
missed (`lightColRows` in db_light_write.c, `D_00A57DF2` = `"D_00A57DF0+2"` in
ov10/cgp.c, the host-only `_srsMemResMap` in sef.c).

**A second form of the same bug: labels inside an object.** With every view
laundered, -O2 still hung in `xglFontFlushCore` (frame 1211). xgl_font.c also
declares `D_00881188`, `D_0088119C` and `D_008811A0`. These are separate
symbols from the PS2 symbol list, but they sit at FS+0x08, +0x1c and +0x20. The
queue cursor is written as `*D_0088119C` in one place and as `*(FS + 0x1c)` in
another, and GCC assumes that distinct objects never overlap. On the host they
are now `#define D_0088119C ((unsigned char **)(FS + 0x1C))` and so on. No
checker covers this form. `nm` shows 258 game objects that use two data symbols
less than 0x40 bytes apart. Most are separate arrays, but any one of them that
indexes past its own symbol into the next one has this bug.

Verification (field recipe, `XSG_FAST=1`, frames 140/1000/1400/1550/1700/6000,
`compare -metric AE` against the pre-change Release build): -O2 has 0 differing
pixels in every frame. A long run (1500 s, 58953 vblanks, field idle) has no hang or crash.
Speed is unchanged within noise: 4485 against 4507 vblanks in 90 s with `XSG_FAST`,
because VU1 is still the limit (§4).

**-O3** (`-DXSG_OPT=-O3`) runs as well: frames 140–1400 are identical, and from 1550 on
about 625 pixels differ. The translucent grey box around Shion that the -O2 build
draws (field, 6000) is missing at -O3. A Debug (-O0) build has the box and is
within 31 px of -O2, so the -O3 difference is a deviation. It could be another
UB pattern that -O3 exposes, or a float or vectorisation change. It was not chased, so
-O3 stays off.

## 2. Build

```
cmake -S port -B port/build -G Ninja     # Release (-O2 -g) by default
cmake -S port -B port/build-dbg -DCMAKE_BUILD_TYPE=Debug   # all -O0
#   -DXSG_OPT=-O3              optimisation level for Release (default -O2)
#   -DXSG_SSP=-fstack-protector-all   find frame overruns
#   -DXSG_O0_REGEX=<regex>     build more game files at -O0 (bisection)
```

The alias-view check runs at configure time. Re-run cmake after adding such a declaration.

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
