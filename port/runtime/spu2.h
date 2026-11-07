/*
 * Software SPU2 (Stage 10, port/docs/STAGE10_SOUND.md).
 *
 * Two cores x 24 voices over 2 MB of sound RAM, rendered at 48 kHz.  The
 * interface is libsd's: register numbers are libsd's sceSdSetParam /
 * sceSdSetSwitch / sceSdSetAddr / sceSdSetCoreAttr encodings (core in bit 0,
 * voice in bits 1-5, register in bits 8-15) and every address is a byte
 * address in sound RAM.  Not thread-safe: the caller serialises (iop_sound.c
 * holds the IOP lock around every call).
 */
#ifndef XSG_SPU2_H
#define XSG_SPU2_H

#include <stdint.h>

#define SPU2_RAM_BYTES 0x200000

void spu2_reset(void);

void spu2_set_param(int entry, uint16_t value);
uint16_t spu2_get_param(int entry);
void spu2_set_switch(int entry, uint32_t value);
uint32_t spu2_get_switch(int entry);
void spu2_set_addr(int entry, uint32_t byte_addr);
uint32_t spu2_get_addr(int entry);
void spu2_set_core_attr(int entry, int value);
int spu2_get_core_attr(int entry);

/* reverb: libsd effect mode 0..9 (off, room, studio A/B/C, hall, space,
 * echo, delay, pipe).  The work area ends at the core's EEA. */
/* delay, feedback: libsd's, used by echo (7) and delay (8) only */
void spu2_set_effect(int core, int mode, int16_t depth_l, int16_t depth_r, int delay, int feedback);
uint16_t spu2_get_reverb_reg(int core, int index); /* preset order */
void spu2_clear_effect_area(int core, int mode);
uint32_t spu2_effect_size(int mode); /* bytes */

/* DMA (sceSdVoiceTrans): immediate copies; writes and reads check IRQA */
void spu2_dma_write(uint32_t byte_addr, const void *src, uint32_t bytes);
void spu2_dma_read(uint32_t byte_addr, void *dst, uint32_t bytes);

/* AutoDMA input (sceSdBlockTrans): 16-bit PCM, 0x200 bytes L then 0x200 R
 * per 256 frames, looping over buf; on_half(core) runs when a half has
 * been consumed. */
void spu2_adma_start(int core, const void *buf, uint32_t bytes, int loop);
void spu2_adma_stop(int core);
uint32_t spu2_adma_status(int core); /* (half being played) << 24 | offset */

/* IRQ: called (inside spu2_render / spu2_dma_*) with the core whose IRQA
 * was hit; the core's IRQ stays latched off until IRQ enable is rewritten. */
void spu2_set_irq_callback(void (*fn)(int core));
void spu2_set_adma_callback(void (*fn)(int core));

/* Debug: called at every key on (byte address of the sample, pitch), and a
 * read of sound RAM with no side effects. */
void spu2_set_keyon_callback(void (*fn)(int core, int voice, uint32_t ssa, uint16_t pitch));
void spu2_peek(uint32_t byte_addr, void *dst, uint32_t bytes);

/* Render `frames` stereo frames of the final output (core 1 out). */
void spu2_render(int16_t *out, int frames);

/* Debug: active voice count, keyon count since reset. */
int spu2_active_voices(void);
unsigned spu2_keyons(void);

/* Debug: one voice's state (phase 0 off, 1 attack, 2 decay, 3 sustain,
 * 4 release); returns nonzero when the voice is on. */
typedef struct {
    int phase, env;
    uint16_t pitch, adsr1, adsr2;
    int16_t vol_l, vol_r;
    uint32_t ssa, lsa, nax; /* byte addresses */
    int noise, pmod, wet;
} spu2_voice_info_t;
int spu2_voice_info(int core, int voice, spu2_voice_info_t *out);

#endif
