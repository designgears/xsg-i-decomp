# Mouse

The game can be played with the mouse alone (keyboard and gamepad keep working alongside).
It is native: the game's own menus hit-test the pointer against their own layout, set their
cursor directly and confirm through their own code, and the field walks the player toward
the pointer.  Settings: F1 > Controls > Mouse (`[mouse]` in settings.ini).

| Where | Mouse |
|---|---|
| any menu list (camp menu and its pages, shops, file screen, choice boxes, battle Ether / Item lists, title), Tech roster, A.G.W.S. list and its segment windows (weapon / ammo / accessory lines), item segments, Ether party / target windows, file slot panels | pointing selects (`hover`), a left click selects and confirms |
| Ether tree (camp menu Ether: Set and Use E.Pts lists, Evolve's slots) | pointing at a node that is an entry of the list beside it (or one of the slots) selects that entry, a click confirms; once the tree pans to it, hover waits for the pointer to move |
| casino (minigame): menu and its submenus, coin exchange, prize shop, data viewer, poker (dialogs, cards to hold, Draw), Nine Reel (reels, Spin, Gets down) | pointing selects, a left click selects and confirms (a held card toggles, a reel stops) |
| Xenocard title menu and its submenus; deck / card list windows (load, save, make deck, view cards); A.G.W.S. Battle title menu, Select A.G.W.S. rows and EXIT, Select Weapons tabs, weapon list and Confirm / Cancel (minigames) | pointing selects, a left click selects and confirms; a click on the inactive deck-screen list switches to it |
| Xenocard table (minigame) | the board's cards (both hands, battle, standby and situation slots) are picked like battle units: the card under the pointer is outlined cyan, the current one gold, pointing selects it, a click on the current one confirms; the hand strip of the play / discard prompts (below the board, or above it when a 2P table is turned) is outlined the same way and walks its selection to the card pointed at, a click on the selected card confirms |
| Xenocard card packs (minigame) | a click on the pack opens it, a click on an arrow shows the next / previous pack; the opened cards are picked like the table's (pointing shows that card, a click on it is done) |
| Xenocard help (minigame) | index: pointing at a category or topic selects it, a click opens it (a click on the other list goes there); a page: a left click turns to the next page, the wheel turns pages, right click is back |
| battle command list | the same; on the rebuilt UI the rows as drawn |
| battle attack rows (both UIs), LB / RB footer, pause card (rebuilt UI) | a click presses that row's button |
| battle targets | the unit under the pointer is outlined; pointing selects it (`hover`, once the pointer moves); a click on another unit selects it, a click on the current target confirms |
| field, `field = hold` | hold the left button: walk toward the pointer (run beyond `run_radius`); click a person: walk up and talk; click elsewhere: CIRCLE (examine, whatever `click_advances`) |
| field, `field = click` | click a spot: walk there; click a person: walk up and talk; holding also walks |
| field, either | a person under the pointer is outlined; the d-pad, keys or the stick end a mouse walk |
| slot machine (minigame) | side buttons: bet up / down; a click on Spin! spins, a click on the reel that stops next (or its STOP lamp) stops it; the wheel picks the line |
| A.G.W.S. Battle fights (minigame) | the fallbacks only (left click CIRCLE, wheel, buttons): real-time play |
| dialogue, anything else | a left click is CIRCLE (`click_advances`) |
| everywhere | right click: back (CROSS); wheel: up / down; middle click: TRIANGLE (camp menu); side buttons: L1 / R1 |
| "Settings" hints | open the settings menu (also with `enabled = 0`) |
| settings menu | right click closes it, side buttons switch tabs, the wheel scrolls |

A click on empty space inside an open menu does nothing; it never confirms the selection.
The field camera is the map's own and takes no input, so the mouse has no camera control.

## Settings (`[mouse]`, all live)

| key | default | |
|---|---|---|
| enabled | 1 | mouse play (0: the game ignores the mouse; no hover, hand cursor or outlines) |
| hover | 1 | pointing selects (off: only clicks) |
| field | hold | off / hold / click (see above) |
| run_radius | 96 | game px from the player beyond which walking becomes running |
| click_advances | 1 | a left click on nothing clickable is CIRCLE |
| hide_after | 3 | idle seconds before the pointer hides (0: never); a key or gamepad press hides it at once, a move brings it back |

## How it works

- xsg-gpu (port/gpu/ui/pointer.cpp) publishes the pointer every frame in shared memory
  (gpu_ipc.h `GpuMouse`): its position in the game's 512 x 448 space under each rebuilt-UI
  layout anchor (left / centre / right, layout.cpp `Unmap`; in wide layouts edge UI moves),
  buttons and wheel as counters, the options, and `ui_kind` / `ui_index` where the rebuilt UI
  draws something in a layout of its own (title rows, battle command rows, prompts).
- The game (port/runtime/mouse.cpp, `xsg_mouse.h`) turns that into a per-frame state.  Hooks
  (`#ifdef PORT_HOST`, the matching build is untouched) claim clicks:
  - `WindowSPSelect` (e_battle_win_open.c): scrolling lists and grids, exact geometry.
  - `MenuSelectMove` / `MenuSelectMove2` (window_tex_load.c): command lists, by last frame's
    hand cursor (recorded in `eCursolMain`, open_close_main.c), rows 24 apart.  The hand is the
    brightest one on screen; of equal ones the frontmost (lowest sprite `work`), so a popup's
    hand wins over the list's parked one under it.
  - `TMENU_updateDefault` (twsys_init.c): choice boxes.
  - Pages with their own cursor code: menu_tec.c (roster), menu_agws_para_set.c (A.G.W.S.
    list), menu_item.c (segments), menu_ether.c (party / target windows; the Ether tree's nodes,
    recorded where ether_tree.c `EtherTreeObjectDraw` places them), file.c (slot panels), ov11
    mini_g.c (the casino's screens, slot machine bet), ov10 card_play_sound1.c `CardMainProc`
    (Xenocard title), ov10 card_fread.c (Xenocard list windows, the hand strips), ov10 cgp.c
    `CGPCursorMove2` (the table cursor, by object ids), ov10 card_view_proc.c (card packs, by
    object ids), ov10 card_disp.c `CardHelpProc` (help index and pages), ov12 rg_title.c / rg_select_agws.c
    (A.G.W.S. Battle title and selection; both 2D pictures), menu_agws_para_set.c state 0x40
    (segment windows).  Choices laid out freely go through `xsg_mouse_rects`.
  - title.c, ov01 menu.c `menuBatObjFree` (commands), battle_init.c `curSel` (units),
    `keyCmdGet` (the original UI's attack rows as drawn by `menuKeyGuideDisp`; no stray attacks).
  - look_at_player.c `GameCfPlayerMove`: the stick, from the pointer projected through the
    camera (`viewMatrix`, scale / offset) onto the ground at the player.  Not while a window or
    a menu list is live, nor while the pad walks or in the steering mode (`move_mode` 2).
- Objects under the pointer (battle units, talkable people): with the native renderer, by object
  ids.  act_2.c / nml_model_set.c tag each model slot with its actor, nml_packet_add.c puts a
  VIF MARK `0xCB00 + actor + 1` before its batches (gs_bridge.cpp -> `GPU_NATIVE_OBJECT`), and
  native.cpp writes the id into an R16 target; the pixel under the pointer is read back a few
  frames later (`GpuMouse.pick`), and outline.comp draws the outlines the game names
  (`g_outline`, selected / hovered) plus, at once, any `g_hoverable` id under the pointer
  (256 bits, ids 1..256).
  Without the native renderer: units by their projected body (as `menuActCurPut` places the
  reticle), people by their feet-to-head column on screen.
- Tagging your own 3D objects (a card, a part, a reel): ids `XSG_OBJ_FIRST`..`XSG_OBJ_LAST`
  (65..255, above the actors; the MARK decode in gs_bridge.cpp takes 0xCB00..0xCBFF).  Bracket
  the object's nml model entries (`nmlModelEntry`, `nmlModelEntryCard`) with
  `xsg_mouse_object_begin(id)` / `xsg_mouse_object_end()`, as act_2.c brackets actors; only
  opaque draws write the id.  Then `xsg_mouse_object_pick(ids, n, sel)` picks among them like
  battle units (cyan hover, gold current, hover selects once the pointer moves, a click on the
  current one confirms).  The Xenocard table does this: card_fread.c tags each board card with
  its cursor target (`XSG_CARD_TAG`, zone * 16 + index, as cgp.c `CGPCursor2Pos` reads them).
  It needs the native renderer; without it (`pick` -1) nothing is picked and the pad works.
- Confirming goes through the game's own path: a hook sets the cursor and taps CIRCLE through
  the pad (sdk.c `scePadRead`), so sounds, checks and transitions are the game's.  A press
  nobody claims is offered again for two more vblanks (the game's logic may run every other
  vblank), then becomes its pad button (right CROSS, middle TRIANGLE, ...).  A keyboard / pad
  cursor move (sdk.c) makes hover wait for the pointer to move again.

Not covered yet (pointing does not select; clicks confirm the current entry, the wheel and the
d-pad buttons still work): the A.G.W.S. Battle fights (real-time play).  Without the native
renderer (no object ids) the Xenocard table's cards, the turned hand strip and the card-pack
models are not picked either (the strip below the board is, by its drawn layout).  In a 2P
Xenocard game only player 1 (pad 1) is driven by the mouse.

## Testing

`[debug] menu_test_set` takes `MOVE=x/y`, `CLICK=x/y[/button]`, `PRESS` / `RELEASE` (same form) and `WHEEL=n` (game
coordinates) at a vsync; `XSG_MOUSE_LOG=1` logs presses, taps, list clicks and field walking.
