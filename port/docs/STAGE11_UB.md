# PC port, Stage 11: undefined-behaviour sweep, -O3 status

Branch `pc-port` (agent worktree), 2026-11-07. Follows [STAGE10_PERF.md](STAGE10_PERF.md) §1a
(open points 5b and 5c and the -O3 note).

Check recipe for every item below: `XSG_GS=gpu XSG_GPU_HEADLESS=1 XSG_AUDIO=0 XSG_MPEG=0
XSG_SKIP_EVENTS=1 XSG_FAST=1 XSG_PAD='300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60'`,
screenshots at 140/1000/1400/1550/1700/6000 compared with `compare -metric AE` against the -O2
build of `pc-port` before this stage. Every fix here leaves those frames at 0 differing pixels.

## 1. 5c: opcodes that do not advance in `xglFontFlushCore`

Opcodes 20, 22, 26 and 28–31 `break` without moving `text`, so a string that holds one of
them loops forever. **This is the original code.** In the PS2 binary the jump table
`jtbl_004D23B0` sends all seven to `.L0021AB34` (`daddu $4, $11, $0`, then the `*text != 0`
loop test). `$11` is the unchanged text pointer, so the PS2 loops the same way. The bug is not
specific to the host, so the C is left as it is.

The game never sends those opcodes. A temporary counter on that `case` (not committed) never
fired over the recipe run (title, cutscene, field to vblank 7500+). The message data was not
scanned statically, because a byte in 0x14..0x1f is only an opcode at a command boundary, and
parsing that needs the full command grammar. A hang there would point to corrupt text, not
to this switch.

## 2. 5b: narrow results read as int

`xglFontAscii2EucWord` (an `__asm__` alias of `xglFontAscii2Euc`) declared an `unsigned int`
result for a function that returns `unsigned short`. On the PS2 the callee leaves the halfword
zero-extended in v0. On i386 only `%ax` is defined, and the upper half of `%eax` is whatever
the callee left there. On PORT_HOST the alias now returns `unsigned short`.

**`port/tools/check_call_types.py <build-dir>`** checks for the same pattern everywhere. It
recompiles every game file with `-flto` (same flags as the build) and LTO-links main plus each
overlay with `-Wlto-type-mismatch`. GCC then compares every declaration (prototype, `__asm__`
alias, implicit `int f()`) with the definition. The script prints return-type conflicts where
the callee is narrower than the caller's type (`NARROW`), where the widths match below int but
the signedness differs (`SIGN`), and where the callee is void (`VOID`, see
`scan_void_results.py`). `--all` also lists the harmless ones.

Fixed (PORT_HOST declarations now carry the callee's type):

| callee returns | callers that read `int` |
|---|---|
| `unsigned short xglSRand` | xgl_font.c (`xglSRandWord` alias) |
| `unsigned short xglFontAscii2Euc` | xgl_font.c (`xglFontAscii2EucWord`) |
| `unsigned short xglFontGetFlags` | game_init_camera.c |
| `unsigned char xglFontGetSPcodeSize` | e_message.c, yajima_test.c, card_fread.c (implicit) |
| `unsigned short dataPadRead` | ov01 battle_init.c, menu.c, … |
| `short WindowSPSelect` | menu_shop.c, menu_skill.h, menu_agws_para_set.c, menu_para_pt_rate_get.c, … |
| `short srsEftNo2WeaponID`, `srsGetBossWaitEftNo` | set_path.c, sef.c |
| `short dataCidGet` | party.h, ov01/unit_cmd.h, … |
| `u8 cfEventGet`, `cfEncountGet` | ov01 calc.c, cmd.h, … |
| `unsigned char DataBuffer_getUByteAt` | init_class_db.c (implicit) |

Two VOID results that a caller really uses were also fixed in the same way as STAGE10 cause 3:
`MenuShopListChange00` (its last statement is `WindowSPSelect(...)`, and the result is stored in
`MenuShopWork->listIndex`) and `HitCheckMapUnitPos` (a forwarder to `HitCheckMapUnitPosSize`
whose result get.c compares with -1). The remaining VOID entries (`tracePrint*`,
`xglCdSifLoadModule`, `MDL_setVisible`, `xglFontPrintExtFunc`,
`xglSoundStreamOpenVagStereoParam`) are declared `int`, but no caller uses the value.

The opposite direction (the callee returns int and the caller declares short/char:
`CardMakeDispList`, `cmdNum`, `thinkRegGet`, `MenuSelectMove`, `MenuFaceEpidGet`,
`nmlModelCalcClipMat2AllCam`) is safe: the i386 caller truncates the result itself. The checker
now reports nothing in the NARROW or SIGN classes.

## 3. -O3

On the current tree (with the drop-shadow fix) **-O3 is pixel-identical to -O2**. The 6
recipe frames match, and so do 41 frames taken every 250 vblanks (to about vblank 10000). This
holds both with and without the fixes above: the ~625-pixel difference from vblank 1550 and the
missing "translucent box" were the drop-shadow bug, and -O2 was the side that drew it wrongly.
The box does not appear in either build now. No file-level bisection was needed.

Speed (XSG_FAST, 90 s, two runs each): -O2 7885 / 7521 vblanks, -O3 7931 / 7911, so -O3 is
0.6–5% faster, which is within the run-to-run noise. VU1 (built separately at -O2) is still the
limit. **-O2 stays the default.** `-DXSG_OPT=-O3` is safe to use.

## 4. UBSan sweep

`cmake -S port -B port/build-ubsan -DXSG_SANITIZE=undefined,float-cast-overflow` instruments
the game C and links libubsan. Without the link flag, link_xsg.py would turn the
`__ubsan_handle_*` references into logging stubs. 32-bit libubsan (`/usr/lib32/libubsan.so`)
works with `gcc -m32`. The run used recover mode (`UBSAN_OPTIONS=log_path=...`) rather than
`-fno-sanitize-recover`, so that one run collects every site. The frames match -O2.

The recipe run (300 s, past vblank 6000) reported 64 distinct sites:

* **Fixed:**
  * `xgl_timer0.c:51` `getbits`: `value >> (32 - bc)` with bc == 0 shifts by 32. PS2
    `srlv` uses the low 5 bits, so the host now writes `& 31`. This is the same value on both,
    without the UB.
  * `ssd_init.c:195`: `*(void **)&w->_unmodeled_038[0x6c]` indexes 0x6c into a 0x48-byte
    member. It means +0xa4 (`server_buffer`) and is now written as a byte offset from `w`.
* **Benign, original layout (about 55 sites, "insufficient space for an object"):** a TU
  declares a global smaller than it is (`extern float HokanNow[4]` viewed as a `HokanState`,
  `GameLoopState`/`PadData`/`sRender`/`batDatBuf`/`orgData` accessed through partial struct
  views at large offsets, `groupStatus`, `D_0093DC70`). Every address falls inside the object's
  real PS2 extent: the host data keeps the PS2 sizes and neighbours. This is the
  "labels inside an object" family from STAGE10 §1a. It is only a problem if the same TU also
  reaches the storage under a second name, and find_alias_views.py covers that case.
* **Benign on x86:** `sef.c:3156–3545`, member access through `SefKey3/5Table` pointers that are
  only 2-byte aligned (the PS2 data is packed and the fields are read as halfwords). Game C is
  built with `-mno-sse`, so GCC emits no alignment-sensitive loads.
* **Not reported:** signed overflow (`-fwrapv` makes it defined), float-to-int overflow, and
  divide by zero did not occur on this path.

Coverage is limited to the recipe path (title, opening cutscene, field). Battles, menus and
shops were not exercised.
