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
#define XSG_GPU_VERSION 3u
#define XSG_GPU_FD 3
#define XSG_GPU_HDR 4096u
#define XSG_GPU_RING (64u << 20)
#define XSG_GPU_REPLY (4u << 20)
#define XSG_GPU_SHM_SIZE (XSG_GPU_HDR + XSG_GPU_RING + XSG_GPU_REPLY)

enum {
    GPU_MSG_PAD = 0,       /* skip to the end of the ring */
    GPU_MSG_GIF = 1,       /* a = path (1..3), payload = GIF qwords */
    GPU_MSG_PRIV = 2,      /* a = register offset (0x0000..0x1FFF), payload = u64 */
    GPU_MSG_VSYNC = 3,     /* a = vblank number, b = field (0/1) */
    GPU_MSG_READ_VRAM = 4, /* a = seq: copy all 4 MB of local memory to the reply area */
    GPU_MSG_READ_FIFO = 5, /* a = seq, b = qwords: local->host transfer data (TRXDIR 1) */
    GPU_MSG_QUIT = 6,
    GPU_MSG_NATIVE = 7,    /* a = kind (GPU_NATIVE_*), b = vertex count; payload: GpuNativeBatch + vertices */
};

/* Native renderer (port/docs/NATIVE_RENDERER.md, XSG_GPU_NATIVE=1): the inputs
 * of one VU1 geometry batch, taken from VU1 data memory after the batch ran.
 * The VU1 output is still kicked to the GS as before; the renderer redraws
 * the batch natively from these inputs. */
enum { GPU_NATIVE_SET0_STRIP = 1 };   /* set 0, loop 0x0c18 (entry 0x0bd8) */
typedef struct {
    float m[4][4];      /* qw 962-965: object -> clip (rows as VU1 holds them: m[k] multiplies coordinate k) */
    float scale[4];     /* qw 966 */
    float offset[4];    /* qw 967 */
    float cscale[4];    /* vf24 (qw 971): colour scale */
    float uv[4];        /* qw 972: .xy UV offset */
    float fog[4];       /* qw 1007: x min, y max, z bias, w scale */
    uint32_t tag[4];    /* qw 1011: GIF tag template (PRIM in bits 47..57) */
    uint32_t frame, seq, pad[2];
} GpuNativeBatch;
/* followed by `b` vertices of GpuNativeVertex */
typedef struct {
    float p[4];         /* xyz, w = S */
    float c[4];         /* RGBA (0..255 floats, after ITOF) */
    float t, pad[3];    /* T */
} GpuNativeVertex;

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

#endif
