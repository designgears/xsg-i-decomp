/*
 * The settings schema of the PC port: the one list of every setting that
 * settings.ini holds (port/docs/MENU.md, "settings.ini reference").
 *
 * Included by both processes as an X-macro, so the key names, defaults and
 * value lists cannot drift:
 *   - xsg (32-bit game, C):     port/runtime/settings.c
 *   - xsg-gpu (64-bit, C++):    port/gpu/menu/cfg.cpp (the menu's table)
 *
 * XSG_SETTING(key, legacy_env, default, values, live, description)
 *   key          "section.name": [section] name = value in settings.ini
 *   legacy_env   the XSG_* variable this replaced (only for the deprecation
 *                warning: the environment no longer sets anything), or 0
 *   default      the value when settings.ini has none ("" = off / unset)
 *   values       "a|b|c", a range "lo..hi", or 0 (free text / number)
 *   live         1: applies at once (from the menu); 0: after a restart
 *   description  one line for the menu and the docs
 *
 * Sections [display] [renderer] [game] [audio] [gamepad] [overlay] are user
 * settings; [debug] holds developer switches (traces, captures, test hooks).
 * [keyboard] and [gamepad] also carry free-form binding entries
 * (STAGE10_INPUT.md), which are not listed here.
 */

/* ---- display (xsg-gpu, live) */
XSG_SETTING("display.fullscreen", "XSG_GPU_FULLSCREEN", "0", "0|1", 1, "Start fullscreen (F11 toggles).")
XSG_SETTING("display.window_scale", "XSG_GPU_SCALE", "2", "1..4", 1, "Window size as a multiple of 512x448.")
XSG_SETTING("display.present", "XSG_GPU_PRESENT", "mailbox", "mailbox|fifo|immediate|auto", 1,
            "Swapchain present mode (fifo = vsync; replaces XSG_GPU_VSYNC=1).")
XSG_SETTING("display.aspect", "XSG_GPU_ASPECT", "4:3", "4:3|16:9|stretch", 1, "Display aspect ratio.")
XSG_SETTING("display.scale_mode", "XSG_GPU_SCALEMODE", "fit", "fit|integer", 1, "Fit the window or integer scaling.")
XSG_SETTING("display.filter", "XSG_GPU_FILTER", "linear", "linear|nearest", 1, "Blit filter.")
XSG_SETTING("display.ui", "XSG_GPU_UI", "original", "original|rebuilt", 1, "Game UI: the original PS2 2D, or rebuilt in Dear ImGui (sharp at any size).")
XSG_SETTING("display.prompts", 0, "auto", "auto|xbox|playstation|nintendo|keyboard", 1, "Button prompts in the rebuilt UI (auto: the connected controller, or the keyboard).")
XSG_SETTING("display.prompt_style", 0, "default", "default|light|retro|alt|alt2|drawn", 1, "Prompt icon style (JulioCacko Free Input Prompts, CC0); drawn: simple vector glyphs.")
XSG_SETTING("display.ui_layout", 0, "game", "game|wide", 1, "Rebuilt UI layout: on the picture (game) or anchored to the window edges (wide).")
XSG_SETTING("display.widescreen", 0, "0", "0|1", 1, "Native 16:9 widescreen (native renderer): the 3D scene drawn wider, the PS2 2D pillarboxed in the centre.")
XSG_SETTING("display.crt", "XSG_GPU_CRT", "off", "off|scanlines|consumer|trinitron|pvm|shadow", 1, "CRT filter style (the picture as on a tube TV).")
XSG_SETTING("display.crt_strength", "XSG_GPU_CRT_STRENGTH", "100", "0..100", 1, "CRT filter strength (percent).")
XSG_SETTING("display.crt_interlace", "XSG_GPU_CRT_INTERLACE", "1", "0|1", 1, "CRT: one beam per field line, blending odd and even lines as an interlaced TV.")
XSG_SETTING("display.crt_scanlines", "XSG_GPU_CRT_SCANLINES", "-1", "-1..100", 1, "CRT scanline darkness (percent; -1: the style's).")
XSG_SETTING("display.crt_mask", "XSG_GPU_CRT_MASK", "-1", "-1..100", 1, "CRT phosphor mask strength (percent; -1: the style's).")
XSG_SETTING("display.crt_halation", "XSG_GPU_CRT_HALATION", "-1", "-1..100", 1, "CRT halation, the wide glow (percent; -1: the style's).")
XSG_SETTING("display.crt_bloom", "XSG_GPU_CRT_BLOOM", "-1", "-1..100", 1, "CRT bloom, bright areas spreading (percent; -1: the style's).")
XSG_SETTING("display.crt_curvature", "XSG_GPU_CRT_CURVATURE", "-1", "-1..100", 1, "CRT screen curvature (percent; -1: the style's).")

/* ---- renderer (both processes, at start) */
XSG_SETTING("renderer.gs", "XSG_GS", "gpu", "gpu|soft|both", 0,
            "GS backend: gpu (xsg-gpu, the menu), soft (software GS, no window/menu), both (compare).")
XSG_SETTING("renderer.native", "XSG_GPU_NATIVE", "0", "0|1", 1, "Native Vulkan renderer (NATIVE_RENDERER.md).")
XSG_SETTING("renderer.native_scale", "XSG_GPU_NATIVE_SCALE", "2", "1..8", 1, "Native renderer internal scale.")
XSG_SETTING("renderer.native_edge_fx", "XSG_GPU_NATIVE_EDGE_FX", "1", "0|1", 1, "Native renderer: the characters' soft-edge pass at native resolution (0: on the PS2 GS, blocky character edges but its contact shadows).")
XSG_SETTING("renderer.lighting", "XSG_GPU_LIGHTING", "ps2", "ps2|perpixel", 1, "Character lighting: the PS2's per-vertex, or the same lights per pixel (native renderer).")
XSG_SETTING("renderer.shadows", "XSG_GPU_SHADOWS", "0", "0..100", 1, "Real shadows from the scene's key light (native renderer, shadow map): strength in percent, 0 = off.")
XSG_SETTING("renderer.shadow_angle", "XSG_GPU_SHADOW_ANGLE", "40", "0..85", 1, "Shadow light lean from the game's overhead light (degrees).")
XSG_SETTING("renderer.shadow_azimuth", "XSG_GPU_SHADOW_AZIMUTH", "135", "0..359", 1, "Shadow light direction around the vertical (degrees, world space): one sun for the whole scene.")
XSG_SETTING("renderer.neural", "XSG_GPU_NEURAL", "0", "0|1", 0, "DLSS 5 Neural Rendering via DLSS5VKLayer (off by default; needs the layer + nvngx_dlssnr.dll; on takes a restart).")
XSG_SETTING("renderer.neural_helper", "XSG_GPU_NEURAL_HELPER", "", 0, 0, "Windows: path of dlssnr_helper.exe (default: dlssnr\\ next to xsg-gpu.exe, then %LOCALAPPDATA%\\xsg\\dlssnr).")
XSG_SETTING("renderer.neural_bins", "XSG_GPU_NEURAL_BINS", "", 0, 0, "Windows: folder holding nvngx_dlssnr.dll (default: the helper's folder or its binaries\\).")
XSG_SETTING("renderer.ssaa", "XSG_GPU_SSAA", "1", "1|2|4|8|16", 1, "paraLLEl-GS super-sampling.")
XSG_SETTING("renderer.ssaa_textures", "XSG_GPU_SSAA_TEX", "0", "0|1", 1, "Super-sampled textures (SSAA >= 2).")
XSG_SETTING("renderer.hires_scanout", "XSG_GPU_HIRES", "0", "0|1", 1, "High-resolution scanout (SSAA >= 4).")

/* ---- game */
XSG_SETTING("game.fps_limit", "XSG_FAST", "60", "60|30|120|uncapped", 1,
            "Vblank rate = game speed; uncapped replaces XSG_FAST=1.")
XSG_SETTING("game.movies", "XSG_MPEG", "1", "0|1", 1, "Play the FMV movies.")
XSG_SETTING("game.uncensored", 0, "0", "0|1", 1, "Uncensored 3039C scene: plays uncensored/3039C_un.pss in place of the disc's 3039C.pss.")
XSG_SETTING("game.uncensored_subs", 0, "1", "0|1", 1, "English subtitles at the top of the screen in the uncensored 3039C (its picture has the Japanese release's subtitles).")
XSG_SETTING("game.uncensored_dir", 0, "", 0, 1, "Folder holding 3039C_un.pss (empty: the uncensored folder in the install root, else <game_dir>/uncensored).")
XSG_SETTING("game.memcard", "XSG_MC", "1", "0|1", 1, "Memory card in slot 1.")
XSG_SETTING("game.memcard_dir", "XSG_MC_DIR", "", 0, 1, "Memory card folder (empty: ~/.local/share/xsg/mc0).")
XSG_SETTING("game.game_dir", "XSG_GAME_DIR", "", 0, 0, "Game data folder with iso/ (and optionally extracted-iso/) (empty: build default).")
XSG_SETTING("game.iso", "XSG_ISO", "", 0, 0, "Disc image path (empty: the first image in <game_dir>/iso). Set by the first-run setup screen (SETUP.md).")
XSG_SETTING("game.elf", "XSG_ELF", "", 0, 0, "SLUS_204.69 path (empty: <game_dir>/extracted-iso/SLUS_204.69, else read from the disc image).")
XSG_SETTING("game.screenshot_dir", "XSG_SCREENSHOT_DIR", "", 0, 1, "Folder for menu screenshots (empty: ~/Pictures/xsg).")

/* ---- audio */
XSG_SETTING("audio.enabled", "XSG_AUDIO", "1", "0|1", 1,
            "Sound output (0 at start: no audio device is opened; live: mutes the mix).")
XSG_SETTING("audio.sound_driver", "XSG_SOUND", "1", "0|1", 0, "Run the game's sound driver (RSSD) at all.")
XSG_SETTING("audio.master", 0, "100", "0..100", 1, "Master volume.")
XSG_SETTING("audio.music", 0, "100", "0..100", 1, "Music volume.")
XSG_SETTING("audio.effects", 0, "100", "0..100", 1, "Effects volume.")
XSG_SETTING("audio.voice", 0, "100", "0..100", 1, "Voice and movie volume.")

/* ---- controls */
XSG_SETTING("gamepad.deadzone", 0, "8000", "0..32000", 1, "Stick dead zone.")
XSG_SETTING("gamepad.trigger_threshold", 0, "8000", "0..32000", 1, "Trigger press threshold.")
XSG_SETTING("gamepad.layout", 0, "xbox", "xbox|playstation|nintendo", 1, "Button labels in the menu.")
/* ---- mouse (port/docs/MOUSE.md; xsg-gpu publishes them live to the game) */
XSG_SETTING("mouse.enabled", 0, "1", "0|1", 1, "Play with the mouse: menus follow the pointer, clicks select.")
XSG_SETTING("mouse.hover", 0, "1", "0|1", 1, "Hovering a menu entry selects it (off: only a click does).")
XSG_SETTING("mouse.field", 0, "hold", "off|hold|click", 1,
            "Walking: hold the left button to walk toward the pointer, or click a spot to walk there.")
XSG_SETTING("mouse.run_radius", 0, "96", "16..400", 1, "Pointer distance from the player (game px) at which walking becomes running.")
XSG_SETTING("mouse.click_advances", 0, "1", "0|1", 1, "A left click where nothing is clickable confirms (advances text, talks).")
XSG_SETTING("mouse.hide_after", 0, "3", "0..30", 1, "Hide the pointer after this many idle seconds (0: never).")

/* ---- overlay */
XSG_SETTING("overlay.mode", "XSG_GPU_OVERLAY", "off", "off|fps|full", 1, "Performance overlay (F10 cycles).")

/* ---- debug: process / logging */
XSG_SETTING("debug.log", "XSG_LOG", "", 0, 0, "Log level (number; empty: default).")
XSG_SETTING("debug.log_file", "XSG_LOG_FILE", "", 0, 0, "Write the game log to this file instead of stderr.")
XSG_SETTING("debug.timeout", "XSG_TIMEOUT", "", 0, 0, "Exit after N seconds.")
XSG_SETTING("debug.gpu_bin", "XSG_GPU_BIN", "", 0, 0, "xsg-gpu binary (empty: gpu/xsg-gpu next to xsg).")
XSG_SETTING("debug.headless", "XSG_GPU_HEADLESS", "0", "0|1", 0, "xsg-gpu without a window.")
XSG_SETTING("debug.vulkan_validation", "XSG_GPU_VVL", "0", "0|1", 0, "Allow the Vulkan validation layer.")
XSG_SETTING("debug.pmode_raw", "XSG_GPU_PMODE_RAW", "0", "0|1", 0, "Scan out PMODE as the game wrote it.")
XSG_SETTING("debug.stream", "XSG_STREAM", "", 0, 0, "Raw 512x448 RGB24 live stream FIFO path.")
XSG_SETTING("debug.jump", 0, "", 0, 0, "Test room: once the first field runs, jump to this scene (cf1234, ev0123; port/runtime/rooms.c).")
XSG_SETTING("debug.room", 0, "", 0, 0, "Test room by name: a scene plus pad steps (battle, battle_kosmos, menu, menu_skills, ...; port/runtime/rooms.c).")
XSG_SETTING("debug.give_all", 0, "0", "0|1", 0, "At the first field: whole party, A.G.W.S., money and every item, weapon, ammo and accessory (rooms.c).")
XSG_SETTING("debug.scene_list", 0, "0", "0|1", 0, "Log every scene file on the disc at the first field.")
XSG_SETTING("debug.skip_events", "XSG_SKIP_EVENTS", "0", "0|1|jump", 0, "Skip story events (test runs; jump: until the debug.jump / room jump).")
XSG_SETTING("debug.frame_us", "XSG_FRAME_US", "", 0, 0, "Fixed vblank period in us (overrides game.fps_limit; 0 = unthrottled).")
XSG_SETTING("debug.fast_until", "XSG_FAST_UNTIL", "", 0, 0, "Unthrottled until this vblank.")
XSG_SETTING("debug.fps", "XSG_FPS", "", 0, 0, "Log [fps] every N frames.")
XSG_SETTING("debug.dma_thread", "XSG_DMA_THREAD", "0", "0|1", 0, "Run VIF1/GIF DMA on the DMAC thread.")
/* ---- debug: pad scripts */
XSG_SETTING("debug.pad", "XSG_PAD", "", 0, 0, "Scripted pad input, e.g. 300:START,1100:START.")
XSG_SETTING("debug.pad_log", "XSG_PAD_LOG", "", 0, 0, "Log pad reads.")
XSG_SETTING("debug.pad_live", "XSG_PAD_LIVE", "", 0, 0, "Live pad command file.")
XSG_SETTING("debug.pad_stick", "XSG_PAD_STICK", "", 0, 0, "Scripted analog stick input.")
XSG_SETTING("debug.remote_pad", 0, "", 0, 0, "Remote pad state file (32-byte record, STATUS_SITE.md), read each vblank.")
XSG_SETTING("debug.twin_log", "XSG_TWIN_LOG", "", 0, 0, "Twin (pad lock-step) log.")
/* ---- debug: screenshots / captures */
XSG_SETTING("debug.shot", "XSG_SHOT", "", 0, 0, "Frames to screenshot (list).")
XSG_SETTING("debug.shot_every", "XSG_SHOT_EVERY", "", 0, 0, "Screenshot every N frames.")
XSG_SETTING("debug.shot_vram", "XSG_SHOT_VRAM", "0", "0|1", 0, "Also dump VRAM with each shot.")
XSG_SETTING("debug.shot_dir", "XSG_SHOT_DIR", "", 0, 0, "Screenshot folder (empty: port/build/shots).")
XSG_SETTING("debug.shot_fbp", "XSG_SHOT_FBP", "", 0, 0, "Force the screenshot frame buffer page.")
XSG_SETTING("debug.vu1_capture", "XSG_VU1_CAPTURE", "", 0, 0, "VU1 capture folder.")
XSG_SETTING("debug.vu1_capture_from", "XSG_VU1_CAPTURE_FROM", "", 0, 0, "VU1 capture first frame.")
XSG_SETTING("debug.vu1_capture_frames", "XSG_VU1_CAPTURE_FRAMES", "", 0, 0, "VU1 capture frame count.")
XSG_SETTING("debug.vu1_capture_every", "XSG_VU1_CAPTURE_EVERY", "", 0, 0, "VU1 capture interval.")
/* ---- debug: traces */
XSG_SETTING("debug.gs_trace", "XSG_GS_TRACE", "", 0, 0, "GS trace of frame N.")
XSG_SETTING("debug.gs_compare", "XSG_GS_COMPARE", "", 0, 0, "renderer.gs=both: compare every N frames (empty: 60).")
XSG_SETTING("debug.dma_trace", "XSG_DMA_TRACE", "0", "0|1", 0, "Trace DMA kicks.")
XSG_SETTING("debug.mpg_trace", "XSG_MPG_TRACE", "0", "0|1", 0, "Trace MPEG uploads.")
XSG_SETTING("debug.ipu_log", "XSG_IPU_LOG", "", 0, 0, "IPU log level.")
XSG_SETTING("debug.vu1", "XSG_VU1", "", 0, 0, "0 disables VU1 microprograms.")
XSG_SETTING("debug.vu_fast", "XSG_VU_FAST", "", 0, 0, "0: reference VU1 interpreter instead of the fast path.")
XSG_SETTING("debug.vu_flaglat", "XSG_VU_FLAGLAT", "", 0, 0, "VU flag latency model.")
XSG_SETTING("debug.vu_hist", "XSG_VU_HIST", "", 0, 0, "VU1 opcode histogram.")
XSG_SETTING("debug.vu_trace", "XSG_VU_TRACE", "", 0, 0, "Trace N VU1 runs.")
XSG_SETTING("debug.vu_dump", "XSG_VU_DUMP", "", 0, 0, "Dump VU1 state at frame N.")
XSG_SETTING("debug.vu_log", "XSG_VU_LOG", "", 0, 0, "VU1 log level.")
XSG_SETTING("debug.vu0_stats", "XSG_VU0_STATS", "", 0, 0, "VU0 statistics.")
XSG_SETTING("debug.prof", "XSG_PROF", "", 0, 0, "Sampling profiler output file.")
XSG_SETTING("debug.prof_start", "XSG_PROF_START", "", 0, 0, "Profiler start frame.")
XSG_SETTING("debug.prof_hz", "XSG_PROF_HZ", "", 0, 0, "Profiler rate (empty: 1000).")
/* ---- debug: sound */
XSG_SETTING("debug.sound_log", "XSG_SOUND_LOG", "", 0, 0, "Sound driver log level.")
XSG_SETTING("debug.spu_trace", "XSG_SPU_TRACE", "", 0, 0, "SPU2 trace folder.")
XSG_SETTING("debug.sound_clock", "XSG_SOUND_CLOCK", "auto", "auto|game|real", 0,
            "IOP sound clock: auto = real time, held back only to keep a stream fed; game = vblanks; real = wall clock.")
XSG_SETTING("debug.wav", "XSG_WAV", "", 0, 0, "Record the mix to this WAV file.")
/* ---- debug: native renderer (xsg-gpu) */
XSG_SETTING("debug.native_vu1", "XSG_GPU_NATIVE_VU1", "0", "0|1", 0, "Native renderer: interpret VU1 programs.")
XSG_SETTING("debug.native_skin", "XSG_GPU_NATIVE_SKIN", "1", "0|1", 0, "Native renderer: GPU skinning.")
XSG_SETTING("debug.native_set0_lit", "XSG_GPU_NATIVE_SET0_LIT", "1", "0|1", 0, "Native renderer: set-0 lit loops (0x1458, 0x1fa0).")
XSG_SETTING("debug.native_live", "XSG_GPU_NATIVE_LIVE", "", 0, 0, "0: native renderer shots only.")
XSG_SETTING("debug.native_thr", "XSG_GPU_NATIVE_THR", "", 0, 0, "Native renderer threshold.")
XSG_SETTING("debug.native_dir", "XSG_GPU_NATIVE_DIR", "", 0, 0, "Native renderer shot folder.")
XSG_SETTING("debug.native_shot", "XSG_GPU_NATIVE_SHOT", "", 0, 0, "Native renderer shot vsyncs (list).")
XSG_SETTING("debug.dump_tex", "XSG_GPU_DUMP_TEX", "", 0, 0, "Native shots: also decode these TEX0 values (hex list) to PNGs.")
XSG_SETTING("debug.glyph_log", "XSG_GPU_GLYPH_LOG", "0", "0|1", 0, "Write every unique small texture upload to debug.glyph_dir (button glyph catalogue).")
XSG_SETTING("debug.glyph_dir", "XSG_GPU_GLYPH_DIR", "", 0, 0, "Glyph catalogue folder (empty: /tmp/xsg_glyphs).")
XSG_SETTING("debug.native_mv_dump", "XSG_GPU_NATIVE_MV_DUMP", "0", "0|1", 0, "Native shots: also write the raw motion vectors (.f32, ~30 MB each).")
XSG_SETTING("debug.native_owner", "XSG_GPU_NATIVE_OWNER", "", 0, 0, "Native FX owner tracking.")
XSG_SETTING("debug.shadow_bias", "XSG_GPU_SHADOW_BIAS", "50", 0, 1, "Shadow map depth bias (1e-5).")
XSG_SETTING("debug.shadow_margin", "XSG_GPU_SHADOW_MARGIN", "30", 0, 1, "Shadow map margin around the characters (0.1 world units).")
XSG_SETTING("debug.shadow_char_radius", "XSG_GPU_SHADOW_CHAR_RADIUS", "90", 0, 1, "Characters receive no shadows within this radius of their origin (0.01 world units).")
XSG_SETTING("debug.owner_thread", "XSG_GPU_OWNER_THREAD", "", 0, 0, "Native FX owner thread.")
XSG_SETTING("debug.fx_trace", "XSG_GPU_FX_TRACE", "", 0, 0, "Native FX trace.")
XSG_SETTING("debug.mv_view", "XSG_GPU_MV_VIEW", "", 0, 0, "Motion vector view.")
XSG_SETTING("debug.gpu_prof", "XSG_GPU_PROF", "", 0, 0, "xsg-gpu profiler.")
XSG_SETTING("debug.sample", "XSG_SAMPLE", "", 0, 0, "Windows: log where the game thread spends its time.")
XSG_SETTING("debug.vu0_ref", "XSG_VU0_REF", "", 0, 0, "0: run the VU0 sin/cos program on the VU interpreter instead of its C version.")
XSG_SETTING("debug.vu0_ref_check", "XSG_VU0_REF_CHECK", "", 0, 0, "1: run the VU0 C program and the interpreter, log differences.")
XSG_SETTING("debug.vu1_ref_check", "XSG_VU1_REF_CHECK", "", 0, 0, "1: run the VU1 C programs and the interpreter on the same input, log differences.")
XSG_SETTING("debug.vu0_runs_check", "XSG_VU0_RUNS_CHECK", "", 0, 0, "1: run each compiled VU0 op run and its ops, log differences.")
/* ---- debug: test hooks (xsg-gpu) */
XSG_SETTING("debug.input_test", "XSG_GPU_INPUT_TEST", "", 0, 0, "Synthetic SDL input events (STAGE10_INPUT.md).")
XSG_SETTING("debug.menu_test", "XSG_GPU_MENU_TEST", "", 0, 0, "Menu screenshot tour: at:per:WxH.")
XSG_SETTING("debug.menu_test_dir", "XSG_GPU_MENU_TEST_DIR", "", 0, 0, "Menu tour output folder.")
XSG_SETTING("debug.menu_test_set", "XSG_GPU_MENU_TEST_SET", "", 0, 0, "Timed settings changes: vsync:key=v,...;... (also MOVE=x/y, CLICK / PRESS / RELEASE=x/y[/button], WHEEL=n: the mouse at game x/y, 512 x 448)")
