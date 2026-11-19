/* MPEG-2 video decoder for the movies (mpeg2dec.c), in the game's process.
 *
 * Push the elementary stream as it arrives; take pictures in display order as
 * RGBA (BT.601, studio range, alpha 0x80).  Decodes Main Profile frame
 * pictures (I/P/B, frame or field motion, frame or field DCT), all the disc's
 * movies use.  Field pictures and dual-prime motion are reported and skipped. */
#ifndef XSG_MPEG2DEC_H
#define XSG_MPEG2DEC_H

#include <stddef.h>
#include <stdint.h>

typedef struct mpeg2dec mpeg2dec;

mpeg2dec *mpeg2dec_new(void);
void mpeg2dec_free(mpeg2dec *d);

/* Append elementary stream bytes; mpeg2dec_end() when no more will come. */
void mpeg2dec_push(mpeg2dec *d, const uint8_t *p, size_t n);
void mpeg2dec_end(mpeg2dec *d);

/* The next picture in display order: 1 and rgba filled (width*height*4 bytes)
 * with *type its coding type (1 I, 2 P, 3 B); 0 when more input is needed;
 * -1 when the stream is over (ended and drained).  Decodes only as far as the
 * picture needs. */
int mpeg2dec_picture(mpeg2dec *d, uint8_t *rgba, int *type);

/* From the sequence header (0 before it). */
int mpeg2dec_width(const mpeg2dec *d);
int mpeg2dec_height(const mpeg2dec *d);
void mpeg2dec_rate(const mpeg2dec *d, int *num, int *den);

#endif
