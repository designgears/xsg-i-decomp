# PC port, Stage 7: the event VM runs the opening scene

Branch `pc-port`, 2026-11-07. Builds on [STAGE6_VU1.md](STAGE6_VU1.md). The IPU/movie and VU0
work done at the same time is in [STAGE7_IPU.md](STAGE7_IPU.md) and [STAGE7_VU0.md](STAGE7_VU0.md).

**Result:** after New Game, the event-script VM now runs the whole opening cutscene
`SCE01001` (the KOS-MOS / Shion lab conversation, 46 dialogue windows, with motions, camera
splines, lights and the voice stream started). It then loads `cf0210` (the Woglinde field) and
the `ST0210` "Virtual Tutorial" event, and waits there on `waitkey(64)` (CROSS). There are no
crashes up to that point, about 11,000 vblanks. The screen is still black: the 3D renderer
draws (VU1 geometry programs run, about 1.4 M prims in 2,400 frames), but its matrices come
from stubbed VU0 asm. Section 5 covers what happens with scripted CROSS presses.

## 1. Root causes fixed

All of these are host-only changes (`#ifdef PORT_HOST` or port scripts). `ninja gate` passes.

| # | symptom | cause | fix (commit) |
|---|---|---|---|
| 1 | vblank 372: `JTHREAD_waitFor` faults at `0x16f` | `JNI_callMethod` calls `JTHREAD_waitFor()` with **no argument** (jni.h declares it `(void)`). On the EE the callee reads its thread from `$a0`, which still holds `JNI_callMethod`'s `vm`. On x86 it read a stale stack word (0x162 + 0xd = 0x16f). The bad "wait target" was really a bad *node* pointer. | pass `vm` (`8fb7b0d`) |
| 2 | vblank 960: `xglVectorScaleXYZ` faults at `0xbf800000` (= -1.0f) | Two `__asm__("xglVectorScaleXYZ")` aliases in nml_model_set.c, and `drawCircle2RadiusFirst` in db_light_write.c, declare the float argument in a **different position** from the definition. MIPS passes floats in `$f12` whatever their position, so the PS2 build is unaffected. On x86 the arguments shift. | host-only macros that reorder the call (`38073de`) |
| 3 | `data\obj\aobj000\acc_<garbage>.lex` not found (scene props missing) | **Converted data.** splat had symbolised 4-byte string tails such as `"006\0"` (= 0x00363030) as `.word CfCameraDefine + 0x1B0`, because the value falls inside RAM. On the PS2 these are the same bytes; on the host they relocate. That broke the `AID0`/model-name tables in look_at_player.sdata (resource names `004`…`199`, `_04`…`_09`) and a few format strings (`%2d`, `%3d`, `.3f`, `EVA`, `HDD`, `msg`, `mot`…). | gen_host_data.py emits such words as raw bytes: 166 words (`4f1b8d7`) |
| 4 | about 11,000 vblanks, after the opening: `RES_loadFileSub` faults writing `files->first_file` | `command_dummy` calls `((int (*)())GameResourceRealloc)(address)` with one argument. On the EE, `$a1` is 0 (set in the branch delay slot). On x86 the `size` came from the stack, `GameResource[1].size` became a host stack address, and the next resource got `address = 0xf7d6bd00`. | pass 0 (`87bd4a2`) |

Causes 1 and 4 are the same class of bug, and so is the Stage 6 font callback: **the MIPS
calling convention lets a call inherit argument registers** (an unprototyped or `(void)`
call, or a call with too few arguments). Other sites of this class that are not on the
current path are listed in §6.

### What it was *not*

* **VU0 asm stubs:** the JTHREAD fault does not depend on any VU0 site. No VU0 code was
  touched here.
* **Struct layout / PS2_LONG / 24-bit pointers:** `JThreadNode` is fine on -m32. The 0x16f
  was a wrong argument, not a truncated field.
* **SDK thread/semaphore semantics:** the event VM's "threads" are cooperative script
  threads driven from `JTHREAD_cntl` on the game thread. They need no kernel behaviour.

## 2. The "missing" opening files are expected misses

Dumping the three archive indexes (`XENOSAGA.00` 8573 entries, `.10` 349, `.20` 266; format
in STAGE4_BOOT.md §2) shows that the files reported as `NOT FOUND` **are not on the disc at
all**. The game probes for them and falls back, exactly as on the PS2:

| request | in index? | what happens |
|---|---|---|
| `scene\ev1000.txt` | yes (.00) | loaded |
| `scene\ev1000.a`, `ev1010.a` | no (`scene\` has only `cf*.a`) | `xglCdGetFileSize` probe, falls through |
| `movie\mpeg2\prologue.cap`, `10op.cap`, `1001_1.cap` | no (only `.pss`) | caption files: the US disc has none |
| `scene\SCE010OP.evt` | yes (.20, layer 1) | loaded |
| `mtnpack\SCE010OP.fpk/.arc` | no | probe only. `SCE01001.fpk/.arc` (.10) are found and loaded |
| `obj\dummy.jnt` | no (only `.lex`/`.xtx`) | probe only |

Notes on how names resolve: `xglCdGetFilePosSub` compares **case-sensitively** against
`root + index path`. The index is mostly lower case, but the event packs are mixed case
(`scene\SCE01001.evt`, `mtnpack\SCE01001.fpk`), and the game asks with that exact case, so
they match. `\DATA\...` upper-case names in the `[disc]` log are only the loose-ISO fallback
(`sceCdLayerSearchFile`), which the game reaches after all four archive indexes have missed.
The host file layer needed no change.

The only real file-name bug was the `acc_<garbage>` name (cause 3): a conversion bug, as the
brief suspected.

## 3. How far it gets (`XSG_PAD=300:0x0800 XSG_FAST=1`, no other input)

```
[file] P data\scene\ev1000.txt
[file] S data\scene\ev1000.a                     (probe, absent on disc)
[file] P data\movie\mpeg2\prologue.cap           (probe, absent)
[file] P data\scene\SCE010OP.evt
[file] P data\scene\ev1010.txt
[file] P data\map\MC_VOK23.lex / .xtx / .id      (the lab map)
[file] P data\obj\aobj000\acc_006.lex/.xtx/.jnt  (was acc_\320\366=\b.lex)
[file] P data\char\pc\kosmos1.*, kosmos_face.*, kosmos_h1.*, shion4_h.*, shion4.*, ...
[file] P data\simajiri\esd\eve003/116/119/172.esd
[file] P data\scene\SCE01001.evt
[file] P data\mtnpack\SCE01001.fpk / .arc
[stream] Sound.streamPlay -> sceCdStStart lsn=709078 (voice/BGM VAG stream)
[msg] Morning, KOS-MOS.
[msg] Good morning, Shion.
[msg] I am an Anti-Gnosis Humanoid
[msg] Development name KP-X.
[msg] Abbreviated name KOS-MOS.
  ... 46 windows ...
[msg] Placing target drones in the Encephalon.
[msg] How about a test run before
[msg] Mmm, sure, let's do that.
[msg] Affirmative.
[file] P data\scene\cf0210.txt, cf0210.a
[file] P data\sound\sed\SO_VOK05.SWD/.SED
[file] P data\motion\shion_cf.fpk
[file] P data\scene\ST0210.evt
[msg]       "Virtual Tutorial"
[msg] /[waitkey(64)]/[clear()]                    <- waits for CROSS
```

The natives called while the scene runs (`rbreak ^Java_` sampled after the stream starts):
`System.sleep` 51, `Chr.start` 29, `Chr.mtn` 24+5, `Window.print/clear` 9/8,
`Chr.setVisible` 13, `Light.setDirection2/setColor` 6/6, `Camera.transSPL/rotateSPL/setFov`,
`Runtime.setDefocus*`, `Stage.setColor`, `Thread.create/start/stop/setTarget`. The script
paces itself with `System.sleep` (frame counts), so it does not block on the voice stream.
The stream is opened but never read (`sceCdStRead` is not called for it), so it is silent.

Every frame after New Game is black (`XSG_SHOT_EVERY=300`: all identical). The GS does
receive geometry: `[gs] frame 2400: ... MSCAL 13475, VIF1 kicks 1199 (119 M qw)`.

## 4. Debugging recipe used

The game objects have symbols but no `-g`. `dprintf` after the prologue reads arguments at
`$ebp+8`, `$ebp+12`, …:

```
gdb -batch -ex 'set debuginfod enabled off' \
  -ex 'dprintf xglCdReadFilePart,"[file] P %s\n", *(char**)($ebp+8)' \
  -ex 'dprintf xglCdGetFileSize,"[file] S %s\n", *(char**)($ebp+8)' \
  -ex 'dprintf MSG_print2,"[msg] %s\n", *(char**)($ebp+12)' \
  -ex run -ex bt port/build-evm/xsg
```

`xglCdGetFilePos` is static, and gcc gives it a register calling convention, so hook the
public `xglCdReadFilePart`/`xglCdGetFileSize` instead. A conditional break such as
`break xglCdGetFileSize if *(int*)(*(char**)($ebp+8)+16)==0x6363615c` stops on one name.
Do not put `run` inside a `-x` script *and* on the command line: when the first run exits,
gdb starts the game again.

Archive index dump (front-coded tree, STAGE4 §2): a 25-line Python reader of sector 0 of
`XENOSAGA.00/.10/.20` (LSN 2674, 700326, 2077583) gives the full 9188-entry list. It is not
committed, because the format is already documented.

## 5. With CROSS presses into the tutorial

Run: `XSG_PAD=300:0x0800,11000:0x0040,11090:0x0040,…,22000:0x0040` (CROSS every 90
vblanks), `XSG_FAST=1`, under gdb, 900 s. The game reaches **vblank 23,400 with no crash**,
but the `ST0210` window stays at `/[waitkey(64)]`. No further message or file load follows,
and every shot (1 per 1000 vblanks) is black. Either `waitkey(64)` is not SCE_PAD CROSS
(0x40) in the game's own button mapping, or the message window's key wait reads pad state
that the scripted `XSG_PAD` presses do not reach (edge, repeat or "trigger" fields of the
pad work area). This is the next thing to trace: `MSG_*` waitkey handling → pad read.

## 6. Next blockers

0. **`waitkey(64)` in ST0210** does not accept the scripted CROSS presses (§5).
1. **Rendering is black:** VU0 matrices (the VU0 agent's work), then validate VU1 geometry
   output.
2. **Voice/BGM stream:** `xglSoundStreamOpenVagStereoParam` → `sceCdStStart` happens, but
   nothing reads the stream (IOP-side RSSD streaming is not emulated). This is harmless for
   timing so far, because the scripts pace with `System.sleep`.
3. **Inherited-register calls still in the tree** (not on the path reached so far, but each
   one will misbehave on x86 when reached):
   * `nmlModelFlush()` called with no argument (xgl_render.c, endou_test.c, menu_shop.c,
     menu_top_command_check.c); the definition takes `int *arg0`, which is only used when
     `s_nRenderCancel != 0` (`AlphaGroupSort(arg0)`).
   * `srsGetWeaponEffectIdx()` (srs.c ×2) with no argument; the definition takes `int id`.
   * `menuStatSprite` (ov01 menu.c) called with 7 of 8 arguments; `_InitDashVRStatus`
     (ov12 rg_robot.c) with 3 of 4+; `_LoadSeq()` (ov12 xrg_sound.c) with none of 1;
     `RgFontStr` (ov12 rg_announce.c) is worth checking too.
   * `MenuSortAddrGet()` (menu_shop.c), `CharactorAllRecovery()`/`AgwsAllRecovery()`
     (runtime.c), `UmnDataBaseMonsterSet()`/`unitActdraw()` (ov01 battle_init.c),
     `MenuModelUnitBreak()` (ov02): `(void)` declarations of functions that take arguments.
     Check each one against the PS2 asm for what `$a0..` hold at the call.
   A checker that compares each call's argument count with the definition, including
   `(T (*)())f` casts and `(void)` externs, would find the rest.
4. **IPU movies** (STAGE7_IPU.md): `1001_1.pss` / `prologue.pss` exist on disc. Whether the
   opening plays them depends on that work.
