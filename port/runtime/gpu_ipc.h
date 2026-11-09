/*
 * Stage 9: shared-memory protocol between the 32-bit game (gs_bridge.cpp) and
 * the 64-bit renderer process xsg-gpu (port/gpu/xsg_gpu.cpp).
 *
 * One memfd, created by the game and inherited by xsg-gpu as fd XSG_GPU_FD
 * (3).  Both sides map it whole.  Only 32-bit fields and 32-bit atomics are
 * shared, so the layout is the same in an i386 and an x86-64 process.
 *
 *   [0, 4096)                 GpuShm header
 *   [4096, 4096+RING)         command ring (single producer = game,
 *                             single consumer = xsg-gpu)
 *   [.., +REPLY)              reply area (VRAM copy / transfer FIFO data)
 *
 * Ring: wpos/rpos are free-running byte counters (mod 2^32); RING is a power
 * of two, every message is a multiple of 16 bytes and starts on a 16-byte
 * boundary, so a 16-byte header always fits before the end.  A message that
 * does not fit before the end is preceded by GPU_MSG_PAD covering the rest.
 * The producer publishes wpos after the payload is written (release); the
 * consumer publishes rpos after it has finished with a message (release).
 * Blocking is by futex on the counters themselves (shared, not PRIVATE),
 * with a waiter flag so the fast path makes no system call.  The producer
 * blocks when the ring is full (back-pressure, never a drop).
 *
 * Synchronous requests (GPU_MSG_READ_VRAM, GPU_MSG_READ_FIFO) carry a
 * sequence number in `a`; the renderer writes the reply area, then stores
 * reply_seq = a and wakes it.
 */
#ifndef XSG_GPU_IPC_H
#define XSG_GPU_IPC_H

#include <stdint.h>

#define XSG_GPU_MAGIC 0x55504758u /* "XGPU" */
#define XSG_GPU_VERSION 8u
#define XSG_GPU_FD 3
#define XSG_GPU_HDR (256u << 10)
#define XSG_GPU_RING (64u << 20)
#define XSG_GPU_REPLY (4u << 20)
#define XSG_GPU_AUDIO (256u << 10)
#define XSG_GPU_AUDIO_OFF (XSG_GPU_HDR + XSG_GPU_RING + XSG_GPU_REPLY)
#define XSG_GPU_SHM_SIZE (XSG_GPU_HDR + XSG_GPU_RING + XSG_GPU_REPLY + XSG_GPU_AUDIO)

/* Audio (port/gpu/audio.cpp): the game's sound driver renders the SPU2 mix (48 kHz
   stereo s16) into this single-producer / single-consumer ring; xsg-gpu plays it
   through an SDL3 audio stream (PipeWire / Pulse / ALSA), pulled at the device's own
   pace.  wpos / rpos count frames (wrap at 2^32); the game keeps about `target_ms`
   queued.  When the game stalls and the ring runs dry, xsg-gpu holds the last sample
   decaying to silence and ramps the audio back in (no clicks); `underruns` counts those.
   on = 1 once the game writes (audio enabled); 0: xsg-gpu keeps the device closed. */
#define XSG_GPU_AUDIO_FRAMES 32768u /* power of two: 683 ms */
typedef struct GpuAudio {
    volatile uint32_t on, rate, wpos, rpos;
    volatile uint32_t underruns, played_lo, played_hi, pad[9];
    int16_t pcm[XSG_GPU_AUDIO_FRAMES * 2];
} GpuAudio;

enum {
    GPU_MSG_PAD = 0,       /* skip to the end of the ring */
    GPU_MSG_GIF = 1,       /* a = path (1..3), payload = GIF qwords */
    GPU_MSG_PRIV = 2,      /* a = register offset (0x0000..0x1FFF), payload = u64 */
    GPU_MSG_VSYNC = 3,     /* a = vblank number, b = field (0/1) */
    GPU_MSG_READ_VRAM = 4, /* a = seq: copy all 4 MB of local memory to the reply area */
    GPU_MSG_READ_FIFO = 5, /* a = seq, b = qwords: local->host transfer data (TRXDIR 1) */
    GPU_MSG_QUIT = 6,
    GPU_MSG_NATIVE = 7,    /* a = kind (GPU_NATIVE_*), b = vertex count; payload: GpuNativeBatch + vertices */
    GPU_MSG_UI_IMAGE = 9,  /* a = image id (1: the title picture), b = w | h << 16; payload: RGBA32 (PS2 alpha) */
    GPU_MSG_UI_TEX = 8,    /* a = texture-set key, b = entries; payload: GpuUiTexEntry[b] then their PSMCT32 words */
};

/* Native renderer (port/docs/NATIVE_RENDERER.md, XSG_GPU_NATIVE=1): the inputs
 * of one VU1 geometry batch, taken from VU1 data memory after the batch ran.
 * The VU1 output is still kicked to the GS as before; the renderer redraws
 * the batch natively from these inputs. */
enum {
    GPU_NATIVE_SET0_STRIP = 1,     /* set 0, loop 0x0c18 (entry 0x0bd8) */
    GPU_NATIVE_SET2 = 2,           /* set 2 (skinned/rigid/blend characters): GpuNativeSkin + vertices + ADC bits */
    GPU_NATIVE_SET2_PALETTE = 3,   /* b = palette seq; payload: qw 732..961 of VU1 data (the bone palette) */
    GPU_NATIVE_SET2_ENV = 4,       /* the env pass of the last set-2 batch starts (its A+D state is now set) */
    GPU_NATIVE_CAMERA = 5,         /* GpuNativeCamera: the world -> clip camera the following batches were built with */
    GPU_NATIVE_SET0_LIT = 6,       /* set 0 lit loops 0x1458 / 0x1fa0 (object-space P/N, no bones): as GPU_NATIVE_SET2,
                                      no palette; per-vertex fog from GpuNativeSkin.fog */
};
/* The scene camera (StudioCamera.viewMatrix: frustum x inverse camera transform),
 * queued by nmlPacketAddParts at packet build and sent when the VIF stream reaches
 * the packet's MARK 0xCA4E, so it arrives in order with the batches it applies to.
 * A batch's object -> world is inverse(vp) x its qw 962-965 matrix. */
typedef struct {
    float vp[4][4];     /* rows as VU0 holds them: vp[k] multiplies coordinate k */
    float scale[4], offset[4];   /* screenScale / screenOffset (vf26 / vf27 of the batches) */
} GpuNativeCamera;
typedef struct {
    float m[4][4];      /* qw 962-965: object -> clip (rows as VU1 holds them: m[k] multiplies coordinate k) */
    float scale[4];     /* qw 966 */
    float offset[4];    /* qw 967 */
    float cscale[4];    /* vf24 (qw 971): colour scale */
    float uv[4];        /* qw 972: .xy UV offset */
    float fog[4];       /* qw 1007: x min, y max, z bias, w scale */
    uint32_t tag[4];    /* qw 1011: GIF tag template (PRIM in bits 47..57) */
    uint32_t frame, seq;
    uint32_t key, occ;  /* draw identity for motion vectors: VIF address of the MSCNT, occurrence in this frame */
} GpuNativeBatch;
/* followed by `b` vertices of GpuNativeVertex */
typedef struct {
    float p[4];         /* xyz, w = S */
    float c[4];         /* RGBA (0..255 floats, after ITOF) */
    float t, pad[3];    /* T */
} GpuNativeVertex;

/* Set 2 (VU1_PROGRAMS.md §2): the constants of one batch, as the program used
 * them (registers after the run, data memory). */
#define GPU_SKIN_PAL_QW0 732u
#define GPU_SKIN_PAL_NQW 230u
typedef struct {
    uint32_t loop;      /* vi15*8: the program's vertex loop */
    uint32_t n, key, occ;
    uint32_t frame, seq, pal, fogw;   /* pal: palette seq (GPU_NATIVE_SET2_PALETTE); fogw: qw 998.w */
    float m[4][4];      /* vf28-31: object -> clip */
    float scale[4], offset[4];        /* vf26, vf27 */
    float lmat[4][4];   /* vf17-19 (row 3 unused): normal -> light; .w = eye (973-975.w) */
    float lcol[4][4];   /* vf20-22 light colours, vf23 ambient */
    float diffuse[4];   /* vf25 */
    float env[4];       /* vf24 (qw 969): env-map ST scale/offset */
    float eye[4];       /* qw 976 */
    float c970[4], uv[4], c1008[4], mirror[4];   /* 970, 972, 1008, 1017 */
    float e1004[4][4];  /* qw 1004-1006 (row 3 unused) */
    uint32_t c1009[4];  /* env-pass colour */
    uint32_t tag1010[4], tag1011[4], tag1012[4];
    uint32_t facing, ifirst, mirror_on, pad;   /* 995.y; I at entry; 1017.z != 0 (ILW) */
    float fog[4];       /* set 0 only: the loop's fog (vf9: x min, y max, z bias, w scale; fog = clamp(z + w q)) */
} GpuNativeSkin;
/* followed by n GpuNativeSkinVertex, then (n+31)/32 u32 of ADC bits (from the
   reference's XYZF2: bit set = the triangle ending at this vertex is not drawn) */
typedef struct {
    float p[4];         /* xyz, w = S */
    float n[4];         /* normal xyz, w = T */
    float c[4];         /* colour (float RGBA) */
    uint32_t b[4];      /* bone stream (qw addresses; "blend 8": x/y addresses, z/w float weights) */
    float w[4];         /* bone weights (skin_lit) */
} GpuNativeSkinVertex;

typedef struct {
    uint32_t type, len; /* len = payload bytes (multiple of 16) */
    uint32_t a, b;
} GpuMsg;

typedef struct {
    uint32_t magic, version, ring_size, reply_size;
    uint32_t pad0[12];
    /* producer-owned */
    volatile uint32_t wpos;
    volatile uint32_t wpos_waiting; /* consumer sleeps on wpos */
    uint32_t pad1[14];
    /* consumer-owned */
    volatile uint32_t rpos;
    volatile uint32_t rpos_waiting; /* producer sleeps on rpos */
    uint32_t pad2[14];
    /* replies (consumer-owned) */
    volatile uint32_t reply_seq;
    volatile uint32_t reply_waiting;
    volatile uint32_t ready;        /* 1 once xsg-gpu has its device up, 2 = failed */
    volatile uint32_t vsyncs_done;
    volatile uint32_t dropped_never; /* always 0; kept for the stats line */
    uint32_t pad3[11];
} GpuShm;

/* Stage 10: live controller state, written by xsg-gpu (SDL3 keyboard and
 * gamepad, port/docs/STAGE10_INPUT.md) and read by the game's scePadRead
 * once per call.  At XSG_GPU_PAD_OFF inside the 4096-byte header page.
 * Seqlock: the writer makes `seq` odd, writes, then makes it even again;
 * the reader retries while seq is odd or changed under it.  All zero (the
 * memfd's initial state, and what a headless xsg-gpu leaves it at) means
 * no live input: the game uses a neutral pad. */
#define XSG_GPU_PAD_OFF 1024u
typedef struct {
    volatile uint32_t seq;
    volatile uint32_t active;      /* 1 once xsg-gpu publishes input */
    volatile uint32_t buttons;     /* SCE_PAD mask, active high (0x0040 CROSS...) */
    volatile uint32_t stick[4];    /* rx, ry, lx, ly as the DS2 sends them, 0x80 = centre */
    volatile uint32_t press[12];   /* DS2 pressure order: R L U D tri O X sq L1 R1 L2 R2 */
    volatile uint32_t gamepads;    /* number of open SDL gamepads */
    volatile uint32_t updates;     /* bumped on every publish (diagnostics) */
    uint32_t reserved[11];
} GpuPad;

/* In-game menu (port/docs/MENU.md): the settings the game process applies
 * live, written by xsg-gpu's menu (port/gpu/menu/) and polled by the game
 * once per vblank (kernel.c -> gpu_client.cpp xsg_gpu_settings_poll).  Same
 * seqlock as GpuPad.  All zero (valid = 0) = no menu: the game keeps its
 * environment / built-in defaults. */
/* Rebuilt UI ([display] ui = rebuilt, port/gpu/ui/): the game's dialog window,
 * published by the game each vblank it draws one (twsys_init.c / EW text widget,
 * PORT_HOST) and drawn by xsg-gpu with Dear ImGui.  Text is the game's encoding
 * (EUC-style full width + its own 0xD1xx symbols and 0x19 control codes). */
#define XSG_GPU_UI_OFF 2560u
#define XSG_GPU_UI_LINES 3
#define XSG_GPU_UI_LINE 192
typedef struct {
    volatile uint32_t seq;          /* seqlock (odd while writing) */
    volatile uint32_t frame;        /* the game vblank of the last publish (0: never) */
    volatile int32_t x, y, w, h;    /* window rectangle, PS2 pixels (512 x 448) */
    volatile uint32_t state;        /* TWIN state (8, 0xB, 0x11: waiting for a key: page arrow) */
    volatile uint32_t nlines;
    char name[64];                  /* speaker (ASCII), "" = none */
    char line[XSG_GPU_UI_LINES][XSG_GPU_UI_LINE];   /* the lines typed out so far */
} GpuUiDialog;

/* Rebuilt UI: the battle party panels (ov01 menu.c menuPlStatPut, PORT_HOST). */
#define XSG_GPU_UI_BATTLE_OFF 3328u
#define XSG_GPU_UI_UNITS 4
typedef struct {
    volatile uint32_t frame;        /* game vblank of the last publish (0: unused) */
    int32_t key;                    /* the unit (address), to keep a slot per unit */
    int32_t x, y;                   /* the panel origin, PS2 pixels (the game's layout) */
    int32_t active;                 /* this unit's turn */
    int32_t hp, hp_max, ep, ep_max; /* displayed (animated) values */
    int32_t ap;                     /* 0..6 chevrons */
    int32_t bg, bp;                 /* boost count, boost gauge (0..60) */
    int32_t idol;                   /* hp shown in the warning colour */
} GpuUiUnit;
typedef struct {
    volatile uint32_t seq;
    GpuUiUnit unit[XSG_GPU_UI_UNITS];
} GpuUiBattle;

/* Rebuilt UI: the battle command menu (ov01 menu.c menuBatObjDraw, PORT_HOST). */
#define XSG_GPU_UI_CMD_OFF 3584u
typedef struct {
    int32_t label;                  /* 0 Ether, 1 Item, 2 Move, 3 A.G.W.S. board, 4 A.G.W.S. leave, 5 Guard, 6 (A.G.W.S. top row) */
    int32_t cmd;                    /* the command id (menuCmdExecChk) */
    int32_t enabled;
    int32_t x, y;                   /* the row origin, PS2 pixels (slides in) */
} GpuUiCmdRow;
typedef struct {
    volatile uint32_t seq;
    volatile uint32_t frame;        /* game vblank of the last publish (0: never) */
    int32_t x, y;
    int32_t sel, sel_cnt;           /* the selected row, its 0..3 open animation */
    int32_t agws;                   /* the A.G.W.S. command table */
    GpuUiCmdRow row[5];
} GpuUiBattleCmd;

/* Rebuilt UI: battle damage / heal numbers (ov01 menu.c objDmgNumDraw, PORT_HOST). */
#define XSG_GPU_UI_DMG_OFF 3712u
#define XSG_GPU_UI_DMGS 12
typedef struct {
    volatile uint32_t frame;        /* game vblank of the last publish (0: unused) */
    int32_t key;                    /* the number's task, a slot per number */
    int32_t x, y;                   /* its centre, PS2 pixels (rises as it ages) */
    int32_t value, type;            /* type: 1 damage, 2 / 4 heal, 3 / 9 / 10 EP */
    int32_t count;                  /* the game's countdown (30 -> 0) */
} GpuUiDmg;
typedef struct {
    volatile uint32_t seq;
    GpuUiDmg d[XSG_GPU_UI_DMGS];
} GpuUiDmgList;

/* Rebuilt UI: captured battle widgets (ov01 menu.c, PORT_HOST).  The game runs its own
   code in a capture mode: the helpers it draws with record what they would draw
   (gpu_client.cpp xsg_ui_kg_op), and xsg-gpu redraws it.  One GpuUiKeyGuide per channel,
   at XSG_GPU_UI_KG_OFF + channel * sizeof(GpuUiKeyGuide). */
#define XSG_GPU_UI_KG_OFF 4096u
#define XSG_GPU_UI_KG_OPS 64
#define XSG_GPU_UI_KG_CHANNELS 4   /* 0 key guide, 1 target name, 2..3 spare */
/* The menus' whole 2D display list (end_print.c PrintFlush, PORT_HOST): a bigger capture. */
#define XSG_GPU_UI_MENU_OFF (32u << 10)
#define XSG_GPU_UI_MENU_OPS 2048
#define XSG_GPU_UI_MENU_POOL (32u << 10)   /* text bytes (GPU_KG_TEXT / GPU_KG_TAG: b = offset, u = length) */
enum {
    GPU_KG_BUTTON = 1,   /* keyGideDisp: a = the icon id (uvTbl, 1-based) */
    GPU_KG_FRAME = 2,    /* menuGuideFrameDisp: a = half width, b = colour (0, 1, 2) */
    GPU_KG_TEXT = 3,     /* menuFontPut: text */
    GPU_KG_NUMBER = 4,   /* menuNumPut: a = value, b = digits */
    GPU_KG_SPRITE = 5,   /* menuStatSprite: a = u, b = v (the EP / AP labels, the L1 / R1 strip) */
    GPU_KG_ART = 6,      /* a textured sprite drawn as is: tex0 (TEX0_1 incl. CLUT), u v w h in texels,
                            (x, y) + (dw, dh) on screen, rgba (PS2 0x80 = 1.0), a = blend (0 alpha, 1 add) */
    GPU_KG_RECT = 7,     /* a flat or gradient rectangle: rgba (top) / a (bottom) */
    GPU_KG_SCISSOR = 8,  /* clip to (x, y, dw, dh); dw = 0: no clip */
    GPU_KG_GROUP = 9,    /* a semantic marker for what follows: a = kind (GPU_UI_*), b = key */
    GPU_KG_LINE = 10,    /* a line (x, y) -> (x + dw, y + dh), rgba */
    /* menu list ops (end_print.c xsg_ui_print_capture): a = depth (larger in front) for all;
       ART: b = texture-set key (GPU_MSG_UI_TEX); RECT: rgba left, b right colour;
       NUMBER: b = value, u = digits, v = flags (0xF style, 0x10 right-aligned);
       TAG / TEXT: b = pool offset, u = length; TAG v... (TAG: w = right-aligned) */
    GPU_KG_WINDOW = 11,  /* a window: (x, y, dw, dh), rgba; b = 1 with its fill, 0 the frame only */
    GPU_KG_TAG = 12,     /* the bitmap title letters (ASCII) */
    GPU_KG_CIRCLE = 13,  /* (x, y), radius dw, rgba centre, b rim colour */
    GPU_KG_PAUSE = 14,   /* the pause screen (game_pause_disp.c): the rebuilt title */
    GPU_KG_QUAD = 16,    /* a textured quad (strip order): corners (x, y) + text as int16 x1 y1 x2 y2 x3 y3, uvs
                            u v / w h / dw dh / a (u3 | v3 << 16), b = texture set, text[12] ALPHA, text[13] FIX */
    GPU_KG_TITLE = 17,   /* the title choices (ov02 title.c): a = selected, b = install row (-1 none, 1 uninstall), rgba = fade */
    GPU_KG_THUMB = 18,   /* a save thumbnail's rectangle (the game draws the picture) */
    GPU_KG_SELECT = 15,  /* a selected row (x, y, dw, dh) (twsys TMENU): glow bar and chevron; b = the item */
};
/* GPU_MSG_UI_TEX: one host->local upload of a texture directory (xtx), replayed by xsg-gpu
   into a private GS memory image per set (the menus swap sets at one VRAM address). */
typedef struct {
    uint32_t dbp, dbw, w, h;        /* PSMCT32 rectangle at (0, 0) */
    uint32_t words;                 /* data words that follow (w * h) */
} GpuUiTexEntry;

/* No 64-bit members: the game is a 32-bit build (i386 aligns them to 4) and xsg-gpu 64-bit. */
typedef struct {
    int32_t type, x, y, a, b;
    uint32_t tex0_lo, tex0_hi;
    int16_t u, v, w, h, dw, dh;
    uint32_t rgba;
    char text[24];
} GpuUiKgOp;
typedef struct {
    volatile uint32_t seq;
    volatile uint32_t frame;
    uint32_t nops, pad_;
    GpuUiKgOp op[XSG_GPU_UI_KG_OPS];
} GpuUiKeyGuide;
typedef struct {
    volatile uint32_t seq;
    volatile uint32_t frame;
    uint32_t nops, pad_;
    uint32_t texa_lo, texa_hi;      /* TEXA at capture (16 / 24-bit alpha expansion) */
    uint32_t pool_used, pad2_;
    GpuUiKgOp op[XSG_GPU_UI_MENU_OPS];
    char pool[XSG_GPU_UI_MENU_POOL];
} GpuUiMenuList;

#define XSG_GPU_SETTINGS_OFF 2048u
#define XSG_GPU_FRAME_US_KEEP 0xffffffffu /* frame_us: leave the game's pacing alone */
typedef struct {
    volatile uint32_t seq;
    volatile uint32_t valid;       /* 1 once xsg-gpu publishes settings */
    volatile uint32_t frame_us;    /* vblank period in us; 0 = uncapped; FRAME_US_KEEP = no change */
    volatile uint32_t audio_on;    /* 0 = muted */
    volatile uint32_t vol[4];      /* master, music, effects, voice: 0..100 (percent) */
    volatile uint32_t restart;     /* nonzero: the game restarts itself (re-exec) */
    volatile uint32_t menu_open;   /* diagnostics: the menu is open (game input neutral) */
    volatile uint32_t open_req;    /* the game asks for the settings menu: bumped per request (title Settings) */
    uint32_t reserved[21];
} GpuSettings;

#ifdef __cplusplus
static_assert(sizeof(GpuAudio) == 64 + XSG_GPU_AUDIO_FRAMES * 4 && sizeof(GpuAudio) <= XSG_GPU_AUDIO, "audio ring layout");
static_assert(sizeof(GpuUiKgOp) == 68 && sizeof(GpuUiKeyGuide) == 16 + 68 * XSG_GPU_UI_KG_OPS, "ui op layout (32 / 64-bit)");
static_assert(sizeof(GpuUiBattleCmd) == 28 + 5 * 20 && sizeof(GpuUiUnit) == 52, "battle layout (32 / 64-bit)");
static_assert(XSG_GPU_UI_KG_OFF + XSG_GPU_UI_KG_CHANNELS * sizeof(GpuUiKeyGuide) <= XSG_GPU_UI_MENU_OFF, "ui capture channels overlap the menu list");
static_assert(XSG_GPU_UI_MENU_OFF + sizeof(GpuUiMenuList) <= XSG_GPU_HDR, "the menu list overruns the header");
static_assert(XSG_GPU_UI_CMD_OFF + sizeof(GpuUiBattleCmd) <= XSG_GPU_UI_DMG_OFF, "battle command block overlaps");
static_assert(XSG_GPU_UI_DMG_OFF + sizeof(GpuUiDmgList) <= XSG_GPU_UI_KG_OFF && sizeof(GpuUiDmg) == 28, "damage list layout");
static_assert(XSG_GPU_UI_BATTLE_OFF + sizeof(GpuUiBattle) <= XSG_GPU_UI_CMD_OFF, "battle block overlaps");
#endif

#endif
