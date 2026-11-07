# Stage 12: the first battle (ST0210 Virtual Tutorial)

The user reported that starting a battle from the web controller crashed the game (SIGSEGV,
exit -11) twice, at vblanks ~15,361 and ~9,099. The logs were lost. This stage reproduces the crash
headless, fixes it, plays the tutorial battle for many turns, and makes future crashes leave
evidence.

## Route (headless, scripted pad)

`port/docs/examples/battle-run.ini` has the whole route. It uses `fast_until`, so vblanks run
at about 80-280/s.

| vblank | input | what happens |
|---|---|---|
| 300, 1100, 1200 | START, START, CROSS | title, then New Game |
| 1500.. | CIRCLE every 40 | tutorial text; the field is up by about 3900 |
| 4000 | DOWN+RIGHT held 40 | Shion walks to KOS-MOS (lower right of the start) |
| 4100 | CIRCLE | talk: "Target input complete." |
| 4460 | CIRCLE | dismisses it; a menu opens at about 4490 |
| 4540/4570/4600, 4640 | DOWN ×3, CIRCLE | **Switch to Real Combat** (the default, *Normal Attacks*, loops back to "Changing targets") |
| 4760.. | CIRCLE | "The battle program has been set…"; the battle loads at about 5040 (`SCE010B00`, `ST9001`) |
| 5400.. | CIRCLE / SQUARE every 90 | tutorial pages, then **SQUARE = Knuckle** (attack) |

The soldier is not the target, and talking to him does nothing. The battle code is overlay
`ov01`. The same overlay is already active from the title screen on.

## Root causes

There were two crashes, both host-only stack overflows. The matched C declares a scalar local and
passes its address to a callee that fills a larger record. On the PS2 the extra bytes land in frame
padding. On i386 they overwrite saved callee-saved registers, so the crash happens later in the
*caller's caller*.

1. **`menuStatObjDraw` (src/ov01/menu.c)**, and also `menuTexTrans`/`menuStatTexTrans`, declare `int
   packet;` and call `grGpInit(&packet)`. `grGpInit` fills a two-word `GrPacket {data, count}`.
   The second word overwrote the saved `%ebx` of `objExec2`'s caller `xglRenderFinalPacket`, whose
   loop index lives in `%ebx`. The index became 0xc5, and it called
   `sRender.final_packet_callback[0xc5]`, which is ASCII from `SoundDataPath`. The result was
   `signal 11 at address 0x5c646573` ("sed\"), right as the battle HUD first drew (vblank ≈ 5095 in
   the scripted route). I found it with gdb breakpoints around each indirect call that compared
   `%ebx/%esi/%edi` before and after the call.
2. **`dataSndSeLoadSub` (src/ov01/snd.c)** declares `char name;` and calls
   `dataSndSeNameGet(&name, …)`, which `strcpy`s a whole sound-bank file name (`…ons0`) into it.
   That overwrote `dataSndSeRegLoad`'s saved `%esi`/`%ebx`, so `record->id = charaId` stored
   through `0x8003530` during `unitEnCreate`, at vblank ≈ 5113.

Fixes, all under `#ifdef PORT_HOST`: `int packet[2]` and `char name_buf[64]` (behind a
`#define name (*name_buf)`, so `name`/`&name` keep their meaning).

There was also one scanner finding in ov01, fixed in the same way. `battleMain` is `void`, but
`entryPhase50` tests `battleMain() == 0` to decide whether the battle has ended. On the PS2 the
tail call left `battleEndChk`'s v0. The host version returns `battleEndChk()` explicitly.

New scanner: `port/tools/scan_scalar_buffers.py [dirs]` lists scalar locals whose address goes
to a buffer-filling callee (`gr*`, `str*`, `*Name*Get`, …). On src/ov01 + src/main it reports the
cases above, which are fixed now. It does not see `#ifdef`, so they are still listed. It also
reports `grDBIdxGet`/`grRotTransPers` with real scalar out-parameters (harmless). The other
scanners: `check_missing_args.py` gives 40 findings, 0 not allowed. `check_call_types.py` reports
only main-side VOID entries that were there before. In ov01, `scan_void_results.py` reports
`battleMain` (fixed). The other ov01 hits (`grOpenF4`, `grVramCopyFBtoFB`, `objHatoVec`, …)
declare an int return, but no caller uses the result.

## How far it gets

The battle runs from vblank 5040 to the 560 s timeout, at vblank 43,229 (~38,000 battle
vblanks), with no crash or hang. The run covered Shion's Knuckle / Power Kick and KOS-MOS's
S-SAULT / SWORD turns, enemy Grenade E/F and Energy Bullet E/B turns, HI-CRITICAL hits, damage
numbers, the boost gauge, ether effects, and the tutorial message pages. The scripted
SQUARE-only input does not finish the tutorial battle: it keeps cycling turns, and the drone
does not die within the run. Ending it probably needs the tutorial's specific commands (tech
attacks / Firecracker). That was not pursued.

Screenshots (gpu renderer, 512×448), copied to the main checkout's `port/docs/screenshots/`
(git-ignored): `battle_tutorial_start.png`, `battle_overview.png`, `battle_command_menu.png`,
`battle_grenade_crit.png`, `battle_grenade_explosion.png`, `battle_ether_hit.png`,
`battle_kosmos_attack.png`.

## Crash evidence for next time

* **Game:** on SIGSEGV/SIGBUS/SIGILL/SIGFPE, `on_fault` (port/runtime/host_main.c) writes the
  following with `write(2)`: `eip`, `esp`, the word at `[esp]` (the caller's return address
  when eip is a bad call target, as in crash 1), and up to 40 stack words that point into `.text`.
  Feed them to `addr2line -f -e port/build/xsg ADDR`. `backtrace_symbols_fd` comes after that.
  It can fault on the makecontext thread stacks, which is why it runs last. The unwinder is primed
  in `main()`.
* **Status site:** each run's full stdout/stderr goes to `LIVE_DIR/runs/run-N.log`, and only the
  last 10 are kept. The log ends with `[status-site] run N exited with RC`. The *Recent runs* table
  links each run to `/runs/run-N.log`.

## Environment note

During this work the user's /tmp quota was full: 13 GB of other sessions' data (`/tmp/xsg-live`,
`/tmp/stream.raw`, …). Compiles failed with EDQUOT until I set `TMPDIR` to a directory under the
build tree.
