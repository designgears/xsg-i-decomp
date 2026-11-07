/*
 * Software SPU2 (Stage 10, port/docs/STAGE10_SOUND.md).  Clean-room, MIT.
 *
 * Behaviour follows the public hardware descriptions (psx-spx "Sound
 * Processing Unit" for the voice, envelope, ADPCM and reverb algorithms,
 * which the PS2's two SPU2 cores keep; the PS2 specifics -- 48 kHz, 24
 * voices per core, 2 MB RAM addressed in halfwords, core 0's output feeding
 * core 1's external input, MMIX gates, AutoDMA input -- as libsd exposes
 * them).  The hardware tables (Gaussian interpolation, reverb presets) are in
 * spu2_tables.c, copied from ico-pc (MIT).
 *
 * Per output sample and voice: Gaussian 4-point interpolation of the decoded
 * ADPCM stream at the voice's pitch (0x1000 = 48 kHz, optional pitch
 * modulation by the previous voice), noise instead if NON is set, times the
 * ADSR envelope, times the left/right volume (fixed or sweep), into the dry
 * and wet (reverb) buses selected by VMIX(E)L/R.  Each core adds its gated
 * inputs (core 0's output into core 1, AutoDMA PCM), runs the reverb on the
 * wet bus at 24 kHz, applies EVOL and MVOL.  Core 1's output is the result.
 */
#include <stdlib.h>
#include <string.h>

#include "spu2.h"

typedef struct spu2_reverb_preset {
    uint32_t size;
    uint16_t regs[32];
} spu2_reverb_preset;
extern const int16_t spu2_gauss[512];
extern const spu2_reverb_preset spu2_libsd_presets[10];
extern const int16_t spu2_reverb_fir[39];
#define RV_HIST 64 /* power of two >= 39: the resampling rings */

enum { PH_OFF, PH_ATTACK, PH_DECAY, PH_SUSTAIN, PH_RELEASE };

typedef struct {
    uint16_t reg;     /* raw VOL register */
    int32_t level;    /* current signed level (-0x8000..0x7fff) */
    uint32_t counter; /* sweep envelope counter */
} vol_t;

typedef struct {
    vol_t vol[2];
    uint16_t pitch, adsr1, adsr2;
    uint32_t ssa, lsa, nax; /* halfword addresses */
    int32_t env;
    uint32_t env_counter;
    int phase;
    uint32_t counter; /* pitch phase, 12-bit fraction */
    int pos;          /* sample index 0..27 in the current block */
    int16_t smp[3 + 28];
    int32_t s1, s2;   /* ADPCM history */
    uint8_t flags;    /* current block's loop flags */
    int16_t out;      /* last output (pitch modulation source) */
} voice_t;

typedef struct {
    voice_t v[24];
    uint16_t attr;
    uint32_t pmon, non, vmixl, vmixr, vmixel, vmixer, endx;
    uint16_t mmix;
    vol_t mvol[2];
    int16_t evol[2], avol[2], bvol[2];
    uint32_t irqa; /* halfwords */
    int irq_latched;
    uint32_t esa, eea; /* halfwords */
    int effect_mode;
    uint16_t rv[32]; /* reverb registers (preset order) */
    uint32_t rvb_pos;
    int16_t rvb_in[2][RV_HIST], rvb_out[2][RV_HIST]; /* 48 kHz rings of the FIR */
    uint32_t rvb_t;                                  /* 48 kHz frame count */
    int delay, feedback;                             /* libsd echo/delay parameters */
    int32_t noise_timer;
    uint16_t noise;
    int32_t out[2];
    /* AutoDMA */
    const int16_t *adma;
    uint32_t adma_frames, adma_pos;
    int adma_loop;
} core_t;

static uint16_t ram[SPU2_RAM_BYTES / 2];
static core_t C[2];
static void (*irq_cb)(int core);
static void (*adma_cb)(int core);
static void (*keyon_cb)(int core, int voice, uint32_t ssa, uint16_t pitch);
static unsigned keyons;

#define RAM_MASK (SPU2_RAM_BYTES / 2 - 1)

static int16_t clamp16(int32_t x)
{
    return x > 32767 ? 32767 : x < -32768 ? -32768 : (int16_t)x;
}

/* ---- envelope generator (ADSR phases and volume sweeps) ------------- */
static void env_step(int32_t *level, uint32_t *counter, int rate, int expo, int dec)
{
    int shift = rate >> 2, step = rate & 3;
    int32_t inc = dec ? -8 + step : 7 - step;
    uint32_t cycles = 1u << (shift > 11 ? shift - 11 : 0);
    if (shift < 11)
        inc <<= 11 - shift;
    if (expo && !dec && *level > 0x6000)
        cycles <<= 2;
    if (expo && dec)
        inc = inc * *level >> 15;
    if (rate == 0x7f)
        return;
    if (++*counter < cycles)
        return;
    *counter = 0;
    *level += inc;
    if (*level > 0x7fff)
        *level = 0x7fff;
    if (*level < 0)
        *level = 0;
}

static void vol_set(vol_t *v, uint16_t reg)
{
    v->reg = reg;
    if (!(reg & 0x8000))
        v->level = (int16_t)(reg << 1);
}

static void vol_tick(vol_t *v)
{
    if (!(v->reg & 0x8000))
        return;
    int32_t mag = v->level < 0 ? -v->level : v->level;
    if (mag > 0x7fff)
        mag = 0x7fff;
    env_step(&mag, &v->counter, v->reg & 0x7f, (v->reg >> 14) & 1, (v->reg >> 13) & 1);
    v->level = (v->reg & 0x1000) ? -mag : mag;
}

static void adsr_tick(voice_t *v)
{
    switch (v->phase) {
    case PH_ATTACK:
        env_step(&v->env, &v->env_counter, (v->adsr1 >> 8) & 0x7f, v->adsr1 >> 15, 0);
        if (v->env >= 0x7fff) {
            v->env = 0x7fff;
            v->phase = PH_DECAY;
            v->env_counter = 0;
        }
        break;
    case PH_DECAY:
        env_step(&v->env, &v->env_counter, ((v->adsr1 >> 4) & 0xf) << 2, 1, 1);
        if (v->env <= (int32_t)(((v->adsr1 & 0xf) + 1) * 0x800)) {
            v->phase = PH_SUSTAIN;
            v->env_counter = 0;
        }
        break;
    case PH_SUSTAIN:
        env_step(&v->env, &v->env_counter, (v->adsr2 >> 6) & 0x7f, v->adsr2 >> 15, (v->adsr2 >> 14) & 1);
        break;
    case PH_RELEASE:
        env_step(&v->env, &v->env_counter, (v->adsr2 & 0x1f) << 2, (v->adsr2 >> 5) & 1, 1);
        if (v->env <= 0) {
            v->env = 0;
            v->phase = PH_OFF;
        }
        break;
    }
}

/* ---- IRQ ------------------------------------------------------------ */
static void irq_check(uint32_t hw, uint32_t count)
{
    for (int c = 0; c < 2; c++) {
        core_t *k = &C[c];
        if (!(k->attr & 0x40) || k->irq_latched)
            continue;
        if (((k->irqa - hw) & RAM_MASK) < count) {
            k->irq_latched = 1;
            if (irq_cb)
                irq_cb(c);
        }
    }
}

/* ---- ADPCM ---------------------------------------------------------- */
static const int adpcm_f[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};

static void decode_block(core_t *k, voice_t *v)
{
    (void)k;
    const uint16_t *b = &ram[v->nax & RAM_MASK];
    irq_check(v->nax & RAM_MASK, 8);
    int hdr = b[0] & 0xff, shift = hdr & 15, f = (hdr >> 4) & 7;
    v->flags = b[0] >> 8;
    if (v->flags & 4)
        v->lsa = v->nax;
    if (f > 4)
        f = 4;
    if (shift > 12)
        shift = 9;
    memmove(v->smp, v->smp + 28, 3 * sizeof v->smp[0]);
    for (int i = 0; i < 28; i++) {
        int nib = (ram[(v->nax + 1 + i / 4) & RAM_MASK] >> ((i & 3) * 4)) & 15;
        int32_t s = ((int16_t)(nib << 12)) >> shift;
        s += (v->s1 * adpcm_f[f][0] + v->s2 * adpcm_f[f][1] + 32) >> 6;
        s = clamp16(s);
        v->s2 = v->s1;
        v->s1 = s;
        v->smp[3 + i] = (int16_t)s;
    }
}

static void next_block(core_t *k, voice_t *v, int vi, int c)
{
    if (v->flags & 1) {
        k->endx |= 1u << vi;
        v->nax = v->lsa;
        if (!(v->flags & 2)) {
            v->phase = PH_OFF;
            v->env = 0;
        }
    } else {
        v->nax = (v->nax + 8) & RAM_MASK;
    }
    (void)c;
    decode_block(k, v);
}

static void key_on(core_t *k, int vi)
{
    voice_t *v = &k->v[vi];
    v->nax = v->ssa;
    v->lsa = v->ssa;
    v->s1 = v->s2 = 0;
    memset(v->smp, 0, sizeof v->smp);
    v->counter = 0;
    v->pos = 0;
    v->env = 0;
    v->env_counter = 0;
    v->phase = PH_ATTACK;
    k->endx &= ~(1u << vi);
    decode_block(k, v);
    keyons++;
    if (keyon_cb)
        keyon_cb((int)(k - C), vi, v->ssa << 1, v->pitch);
}

static void key_off(core_t *k, int vi)
{
    voice_t *v = &k->v[vi];
    if (v->phase != PH_OFF) {
        v->phase = PH_RELEASE;
        v->env_counter = 0;
    }
}

/* ---- registers ------------------------------------------------------ */
static int vidx(int e)
{
    int v = (e >> 1) & 31;
    return v < 24 ? v : 0;
}

void spu2_reset(void)
{
    memset(ram, 0, sizeof ram);
    memset(C, 0, sizeof C);
    for (int c = 0; c < 2; c++) {
        C[c].noise = 1;
        C[c].eea = c ? 0xfffff : 0xeffff; /* libsd: core 0 0x1dffff, core 1 0x1fffff */
        C[c].esa = C[c].eea;
    }
    keyons = 0;
}

void spu2_set_param(int e, uint16_t val)
{
    core_t *k = &C[e & 1];
    voice_t *v = &k->v[vidx(e)];
    switch (e & 0xff00) {
    case 0x0000: vol_set(&v->vol[0], val); break;
    case 0x0100: vol_set(&v->vol[1], val); break;
    case 0x0200: v->pitch = val > 0x3fff ? 0x3fff : val; break;
    case 0x0300: v->adsr1 = val; break;
    case 0x0400: v->adsr2 = val; break;
    case 0x0500: v->env = val & 0x7fff; break;
    case 0x0800: k->mmix = val; break;
    case 0x0900: vol_set(&k->mvol[0], val); break;
    case 0x0a00: vol_set(&k->mvol[1], val); break;
    case 0x0b00: k->evol[0] = (int16_t)val; break;
    case 0x0c00: k->evol[1] = (int16_t)val; break;
    case 0x0d00: k->avol[0] = (int16_t)val; break;
    case 0x0e00: k->avol[1] = (int16_t)val; break;
    case 0x0f00: k->bvol[0] = (int16_t)val; break;
    case 0x1000: k->bvol[1] = (int16_t)val; break;
    }
}

uint16_t spu2_get_param(int e)
{
    core_t *k = &C[e & 1];
    voice_t *v = &k->v[vidx(e)];
    switch (e & 0xff00) {
    case 0x0000: return v->vol[0].reg;
    case 0x0100: return v->vol[1].reg;
    case 0x0200: return v->pitch;
    case 0x0300: return v->adsr1;
    case 0x0400: return v->adsr2;
    case 0x0500: return (uint16_t)v->env;
    case 0x0600: return (uint16_t)v->vol[0].level;
    case 0x0700: return (uint16_t)v->vol[1].level;
    case 0x0800: return k->mmix;
    case 0x0900: return k->mvol[0].reg;
    case 0x0a00: return k->mvol[1].reg;
    case 0x0b00: return (uint16_t)k->evol[0];
    case 0x0c00: return (uint16_t)k->evol[1];
    case 0x0d00: return (uint16_t)k->avol[0];
    case 0x0e00: return (uint16_t)k->avol[1];
    case 0x0f00: return (uint16_t)k->bvol[0];
    case 0x1000: return (uint16_t)k->bvol[1];
    case 0x1100: return (uint16_t)k->mvol[0].level;
    case 0x1200: return (uint16_t)k->mvol[1].level;
    }
    return 0;
}

void spu2_set_switch(int e, uint32_t val)
{
    core_t *k = &C[e & 1];
    val &= 0xffffff;
    switch (e & 0xff00) {
    case 0x1300: k->pmon = val & ~1u; break;
    case 0x1400: k->non = val; break;
    case 0x1500:
        for (int i = 0; i < 24; i++)
            if (val & (1u << i))
                key_on(k, i);
        break;
    case 0x1600:
        for (int i = 0; i < 24; i++)
            if (val & (1u << i))
                key_off(k, i);
        break;
    case 0x1700: k->endx = 0; break;
    case 0x1800: k->vmixl = val; break;
    case 0x1900: k->vmixel = val; break;
    case 0x1a00: k->vmixr = val; break;
    case 0x1b00: k->vmixer = val; break;
    }
}

uint32_t spu2_get_switch(int e)
{
    core_t *k = &C[e & 1];
    switch (e & 0xff00) {
    case 0x1300: return k->pmon;
    case 0x1400: return k->non;
    case 0x1700: return k->endx;
    case 0x1800: return k->vmixl;
    case 0x1900: return k->vmixel;
    case 0x1a00: return k->vmixr;
    case 0x1b00: return k->vmixer;
    }
    return 0;
}

void spu2_set_addr(int e, uint32_t a)
{
    core_t *k = &C[e & 1];
    voice_t *v = &k->v[vidx(e)];
    uint32_t hw = (a >> 1) & RAM_MASK;
    switch (e & 0xff00) {
    case 0x1c00: k->esa = hw; break;
    /* EEA keeps only A20..A17: the end of a 128 KiB (0x10000-halfword) page */
    case 0x1d00: k->eea = hw | 0xffff; break;
    case 0x1f00: k->irqa = hw; break;
    case 0x2000: v->ssa = hw & ~7u; break;
    case 0x2100: v->lsa = hw & ~7u; break;
    case 0x2200: v->nax = hw; break;
    }
}

uint32_t spu2_get_addr(int e)
{
    core_t *k = &C[e & 1];
    voice_t *v = &k->v[vidx(e)];
    switch (e & 0xff00) {
    case 0x1c00: return k->esa << 1;
    case 0x1d00: return k->eea << 1;
    case 0x1f00: return k->irqa << 1;
    case 0x2000: return v->ssa << 1;
    case 0x2100: return v->lsa << 1;
    case 0x2200: return v->nax << 1;
    }
    return 0;
}

/* libsd SD_C_*: 2 effect enable, 4 IRQ enable, 6 mute, 8 noise clock */
void spu2_set_core_attr(int e, int val)
{
    core_t *k = &C[e & 1];
    switch (e & ~1) {
    case 2: k->attr = (k->attr & ~0x80) | (val ? 0x80 : 0); break;
    case 4:
        k->attr = (k->attr & ~0x40) | (val ? 0x40 : 0);
        k->irq_latched = 0;
        break;
    case 6: k->attr = (k->attr & ~0x4000) | (val ? 0x4000 : 0); break;
    case 8: k->attr = (k->attr & ~0x3f00) | ((val & 0x3f) << 8); break;
    }
}

int spu2_get_core_attr(int e)
{
    core_t *k = &C[e & 1];
    switch (e & ~1) {
    case 2: return (k->attr >> 7) & 1;
    case 4: return (k->attr >> 6) & 1;
    case 6: return (k->attr >> 14) & 1;
    case 8: return (k->attr >> 8) & 0x3f;
    }
    return 0;
}

/* ---- reverb --------------------------------------------------------- */
enum {
    dAPF1, dAPF2, vIIR, vCOMB1, vCOMB2, vCOMB3, vCOMB4, vWALL, vAPF1, vAPF2,
    mLSAME, mRSAME, mLCOMB1, mRCOMB1, mLCOMB2, mRCOMB2, dLSAME, dRSAME,
    mLDIFF, mRDIFF, mLCOMB3, mRCOMB3, mLCOMB4, mRCOMB4, dLDIFF, dRDIFF,
    mLAPF1, mRAPF1, mLAPF2, mRAPF2, vLIN, vRIN
};

uint32_t spu2_effect_size(int mode)
{
    return mode >= 0 && mode < 10 ? spu2_libsd_presets[mode].size : 0;
}

/* sceSdSetEffectAttr: the mode's libsd preset.  For echo (7) and delay (8)
 * libsd then rewrites the delay lines and the feedback from the caller's
 * delay and feedback (0..127; delay 127 gives the preset's own lines):
 * d = (delay + 1) * 32 (8-byte units), vWALL = feedback * 258,
 * mLSAME = 2d - dAPF1, mRSAME = d - dAPF2, dLSAME = dRSAME + d,
 * mLCOMB1 = mRCOMB1 + d, mLAPF1 = mLAPF2 + d, mRAPF1 = mRAPF2 + d
 * (as ps2-port-engine's Spu2::SetEffectAttr, MIT, programs them). */
void spu2_set_effect(int core, int mode, int16_t dl, int16_t dr, int delay, int feedback)
{
    core_t *k = &C[core & 1];
    if (mode < 0 || mode > 9)
        mode = 0;
    k->effect_mode = mode;
    memcpy(k->rv, spu2_libsd_presets[mode].regs, sizeof k->rv);
    k->delay = k->feedback = 0;
    if (mode == 7 || mode == 8) {
        delay &= 0x7f;
        feedback &= 0x7f;
        uint16_t d = (uint16_t)((delay + 1) << 5);
        k->rv[vWALL] = (uint16_t)(((feedback << 7) + feedback) << 1);
        k->rv[mLSAME] = (uint16_t)((d << 1) - k->rv[dAPF1]);
        k->rv[mRSAME] = (uint16_t)(d - k->rv[dAPF2]);
        k->rv[dLSAME] = (uint16_t)(k->rv[dRSAME] + d);
        k->rv[mLCOMB1] = (uint16_t)(k->rv[mRCOMB1] + d);
        k->rv[mRAPF1] = (uint16_t)(k->rv[mRAPF2] + d);
        k->rv[mLAPF1] = (uint16_t)(k->rv[mLAPF2] + d);
        k->delay = delay;
        k->feedback = feedback;
    }
    k->esa = (k->eea - spu2_libsd_presets[mode].size / 2 + 1) & RAM_MASK;
    k->rvb_pos = k->esa;
    k->evol[0] = dl;
    k->evol[1] = dr;
}

uint16_t spu2_get_reverb_reg(int core, int i) { return C[core & 1].rv[i & 31]; }

void spu2_clear_effect_area(int core, int mode)
{
    core_t *k = &C[core & 1];
    uint32_t n = spu2_effect_size(mode) / 2;
    uint32_t start = (k->eea - n + 1) & RAM_MASK;
    for (uint32_t i = 0; i < n; i++)
        ram[(start + i) & RAM_MASK] = 0;
}

static uint32_t rv_addr(core_t *k, int32_t off_hw)
{
    uint32_t size = ((k->eea - k->esa) & RAM_MASK) + 1;
    int64_t rel = (int64_t)((k->rvb_pos - k->esa) & RAM_MASK) + off_hw;
    rel %= size;
    if (rel < 0)
        rel += size;
    return (k->esa + (uint32_t)rel) & RAM_MASK;
}
#define RD(o) ((int32_t)(int16_t)ram[rv_addr(k, (o))])
#define WR(o, x) (ram[rv_addr(k, (o))] = (uint16_t)clamp16(x))
#define M(a, b) ((int32_t)(a) * (int32_t)(int16_t)(b) >> 15)
#define OFF(r) ((int32_t)k->rv[r] * 4)

/* One 24 kHz step: psx-spx's "Reverb Formula" with every register (the
 * same/different-side reflections with their IIR and wall, the four combs,
 * the two all-pass stages).  The work area is written only while the
 * core's effect enable (ATTR bit 7) is set; the read side always runs. */
static void reverb_tick(core_t *k, int32_t lin, int32_t rin, int32_t *lout, int32_t *rout)
{
    const uint16_t *r = k->rv;
    int wr = (k->attr & 0x80) != 0;
    int32_t in[2] = {M(lin, r[vLIN]), M(rin, r[vRIN])};
    for (int ch = 0; ch < 2; ch++) {
        int32_t msame = OFF(mLSAME + ch), mdiff = OFF(mLDIFF + ch);
        int32_t dsame = OFF(dLSAME + ch), ddiff = OFF(ch ? dLDIFF : dRDIFF); /* crossed */
        int32_t prev = RD(msame - 1);
        int32_t t = clamp16(in[ch] + M(RD(dsame), r[vWALL]) - prev);
        int32_t vs = clamp16(M(t, r[vIIR]) + prev);
        prev = RD(mdiff - 1);
        t = clamp16(in[ch] + M(RD(ddiff), r[vWALL]) - prev);
        int32_t vd = clamp16(M(t, r[vIIR]) + prev);
        if (wr) {
            WR(msame, vs);
            WR(mdiff, vd);
        }
    }
    int32_t out[2];
    for (int ch = 0; ch < 2; ch++) {
        int32_t o = M(RD(OFF(mLCOMB1 + ch)), r[vCOMB1]) + M(RD(OFF(mLCOMB2 + ch)), r[vCOMB2]) +
                    M(RD(OFF(mLCOMB3 + ch)), r[vCOMB3]) + M(RD(OFF(mLCOMB4 + ch)), r[vCOMB4]);
        o = clamp16(o);
        int32_t a = RD(OFF(mLAPF1 + ch) - OFF(dAPF1));
        o = clamp16(o - M(a, r[vAPF1]));
        if (wr)
            WR(OFF(mLAPF1 + ch), o);
        o = clamp16(M(o, r[vAPF1]) + a);
        a = RD(OFF(mLAPF2 + ch) - OFF(dAPF2));
        o = clamp16(o - M(a, r[vAPF2]));
        if (wr)
            WR(OFF(mLAPF2 + ch), o);
        out[ch] = clamp16(M(o, r[vAPF2]) + a);
    }
    *lout = out[0];
    *rout = out[1];
    k->rvb_pos = (k->rvb_pos + 1) & RAM_MASK;
    if (((k->rvb_pos - k->esa) & RAM_MASK) > ((k->eea - k->esa) & RAM_MASK))
        k->rvb_pos = k->esa;
}

/* The 48 <-> 24 kHz conversion around the reverb: psx-spx's 39-tap half-band
 * FIR (spu2_reverb_fir: symmetric, the odd taps zero except the centre
 * 0x4000).  Down: on odd frames, the filter over the last 39 wet inputs.
 * Up: the reverb output zero-stuffed to 48 kHz through the same filter at
 * gain 2, so odd frames meet only the even taps and even frames only the
 * centre.  The layout of this follows ico-pc's reverb_frame (MIT). */
static int32_t fir_even(const int16_t *h, uint32_t p)
{
    int32_t acc = 0;
    for (int i = 0; i < 19; i += 2)
        acc += spu2_reverb_fir[i] * ((int32_t)h[(p - (uint32_t)i) & (RV_HIST - 1)] +
                                     h[(p - 38u + (uint32_t)i) & (RV_HIST - 1)]);
    return acc;
}

static void reverb_frame(core_t *k, int32_t wl, int32_t wr, int32_t out[2])
{
    uint32_t p = k->rvb_t++ & (RV_HIST - 1);
    k->rvb_in[0][p] = (int16_t)wl;
    k->rvb_in[1][p] = (int16_t)wr;
    if (p & 1) {
        int32_t in[2], o[2];
        for (int ch = 0; ch < 2; ch++)
            in[ch] = clamp16((fir_even(k->rvb_in[ch], p) + 0x4000 * k->rvb_in[ch][(p - 19u) & (RV_HIST - 1)]) >> 15);
        reverb_tick(k, in[0], in[1], &o[0], &o[1]);
        for (int ch = 0; ch < 2; ch++) {
            k->rvb_out[ch][p] = (int16_t)o[ch];
            out[ch] = clamp16(fir_even(k->rvb_out[ch], p) >> 14);
        }
    } else {
        for (int ch = 0; ch < 2; ch++) {
            k->rvb_out[ch][p] = 0;
            out[ch] = k->rvb_out[ch][(p - 19u) & (RV_HIST - 1)];
        }
    }
}

/* ---- DMA ------------------------------------------------------------ */
void spu2_dma_write(uint32_t a, const void *src, uint32_t bytes)
{
    uint32_t hw = (a >> 1) & RAM_MASK, n = bytes / 2;
    const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++)
        ram[(hw + i) & RAM_MASK] = (uint16_t)(s[2 * i] | s[2 * i + 1] << 8);
    irq_check(hw, n);
}

void spu2_dma_read(uint32_t a, void *dst, uint32_t bytes)
{
    uint32_t hw = (a >> 1) & RAM_MASK, n = bytes / 2;
    uint8_t *d = dst;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t w = ram[(hw + i) & RAM_MASK];
        d[2 * i] = (uint8_t)w;
        d[2 * i + 1] = (uint8_t)(w >> 8);
    }
    irq_check(hw, n);
}

void spu2_adma_start(int core, const void *buf, uint32_t bytes, int loop)
{
    core_t *k = &C[core & 1];
    k->adma = buf;
    k->adma_frames = bytes / 4;
    k->adma_pos = 0;
    k->adma_loop = loop;
}
void spu2_adma_stop(int core) { C[core & 1].adma = NULL; }
uint32_t spu2_adma_status(int core)
{
    core_t *k = &C[core & 1];
    if (!k->adma)
        return 0;
    return (uint32_t)(k->adma_pos >= k->adma_frames / 2) << 24 | (k->adma_pos * 4);
}

void spu2_set_irq_callback(void (*fn)(int)) { irq_cb = fn; }
void spu2_set_keyon_callback(void (*fn)(int, int, uint32_t, uint16_t)) { keyon_cb = fn; }

void spu2_peek(uint32_t a, void *dst, uint32_t bytes)
{
    uint32_t hw = (a >> 1) & RAM_MASK;
    uint8_t *d = dst;
    for (uint32_t i = 0; i < bytes / 2; i++) {
        uint16_t w = ram[(hw + i) & RAM_MASK];
        d[2 * i] = (uint8_t)w;
        d[2 * i + 1] = (uint8_t)(w >> 8);
    }
}
void spu2_set_adma_callback(void (*fn)(int)) { adma_cb = fn; }

static void adma_sample(core_t *k, int c, int32_t *l, int32_t *r)
{
    *l = *r = 0;
    if (!k->adma || !k->adma_frames)
        return;
    uint32_t blk = k->adma_pos / 256, i = k->adma_pos % 256;
    *l = k->adma[blk * 512 + i];
    *r = k->adma[blk * 512 + 256 + i];
    k->adma_pos++;
    if (k->adma_pos == k->adma_frames / 2 || k->adma_pos >= k->adma_frames) {
        if (k->adma_pos >= k->adma_frames) {
            k->adma_pos = 0;
            if (!k->adma_loop)
                k->adma = NULL;
        }
        if (adma_cb)
            adma_cb(c);
    }
}

/* ---- render --------------------------------------------------------- */
static void noise_tick(core_t *k)
{
    int clk = (k->attr >> 8) & 0x3f, shift = clk >> 2, step = clk & 3;
    k->noise_timer -= step + 4;
    while (k->noise_timer < 0) {
        k->noise_timer += 0x20000 >> shift;
        int p = ((k->noise >> 15) ^ (k->noise >> 12) ^ (k->noise >> 11) ^ (k->noise >> 10) ^ 1) & 1;
        k->noise = (uint16_t)(k->noise << 1 | p);
    }
}

static void core_sample(int c, int32_t ext_l, int32_t ext_r)
{
    core_t *k = &C[c];
    int32_t dl = 0, dr = 0, wl = 0, wr = 0;
    noise_tick(k);
    for (int i = 0; i < 24; i++) {
        voice_t *v = &k->v[i];
        if (v->phase == PH_OFF) {
            v->out = 0;
            continue;
        }
        int32_t s;
        if (k->non & (1u << i)) {
            s = (int16_t)k->noise;
        } else {
            int g = (v->counter >> 4) & 0xff;
            const int16_t *p = &v->smp[v->pos];
            s = (spu2_gauss[0xff - g] * p[0] >> 15) + (spu2_gauss[0x1ff - g] * p[1] >> 15) +
                (spu2_gauss[0x100 + g] * p[2] >> 15) + (spu2_gauss[g] * p[3] >> 15);
        }
        int32_t o = s * v->env >> 15;
        v->out = clamp16(o);
        vol_tick(&v->vol[0]);
        vol_tick(&v->vol[1]);
        int32_t l = o * v->vol[0].level >> 15, r = o * v->vol[1].level >> 15;
        uint32_t bit = 1u << i;
        if (k->vmixl & bit) dl += l;
        if (k->vmixr & bit) dr += r;
        if (k->vmixel & bit) wl += l;
        if (k->vmixer & bit) wr += r;
        uint32_t step = v->pitch;
        if ((k->pmon & bit) && i > 0) {
            step = (uint32_t)((int32_t)step * (k->v[i - 1].out + 0x8000) >> 15);
            if (step > 0x3fff)
                step = 0x3fff;
        }
        v->counter += step;
        while (v->counter >= 0x1000) {
            v->counter -= 0x1000;
            if (++v->pos == 28) {
                v->pos = 0;
                next_block(k, v, i, c);
            }
        }
        adsr_tick(v);
    }
    /* inputs: external (core 0 -> core 1) through AVOL, AutoDMA through BVOL */
    int32_t al, ar;
    adma_sample(k, c, &al, &ar);
    int32_t el = ext_l * k->avol[0] >> 15, er = ext_r * k->avol[1] >> 15;
    int32_t il = al * k->bvol[0] >> 15, ir = ar * k->bvol[1] >> 15;
    /* MMIX gates: 0x800/0x400 voices dry L/R, 0x200/0x100 voices wet,
     * 0x80/0x40 AutoDMA input dry, 0x20/0x10 wet, 0x8/0x4 external dry, 0x2/0x1 wet */
    uint16_t m = k->mmix;
    if (!(m & 0x800)) dl = 0;
    if (!(m & 0x400)) dr = 0;
    if (!(m & 0x200)) wl = 0;
    if (!(m & 0x100)) wr = 0;
    if (m & 0x008) dl += el;
    if (m & 0x004) dr += er;
    if (m & 0x002) wl += el;
    if (m & 0x001) wr += er;
    if (m & 0x080) dl += il;
    if (m & 0x040) dr += ir;
    if (m & 0x020) wl += il;
    if (m & 0x010) wr += ir;
    /* reverb at 24 kHz */
    int32_t rl = 0, rr = 0;
    if (k->effect_mode) {
        int32_t o[2];
        reverb_frame(k, clamp16(wl), clamp16(wr), o);
        rl = o[0];
        rr = o[1];
        rl = rl * k->evol[0] >> 15;
        rr = rr * k->evol[1] >> 15;
    }
    vol_tick(&k->mvol[0]);
    vol_tick(&k->mvol[1]);
    int32_t ol = clamp16(dl) + rl, or_ = clamp16(dr) + rr;
    if (k->attr & 0x4000)
        ol = or_ = 0;
    k->out[0] = clamp16(clamp16(ol) * k->mvol[0].level >> 15);
    k->out[1] = clamp16(clamp16(or_) * k->mvol[1].level >> 15);
}

void spu2_render(int16_t *out, int frames)
{
    for (int n = 0; n < frames; n++) {
        core_sample(0, 0, 0);
        core_sample(1, C[0].out[0], C[0].out[1]);
        out[2 * n] = (int16_t)C[1].out[0];
        out[2 * n + 1] = (int16_t)C[1].out[1];
    }
}

int spu2_active_voices(void)
{
    int n = 0;
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < 24; i++)
            n += C[c].v[i].phase != PH_OFF;
    return n;
}

unsigned spu2_keyons(void) { return keyons; }

int spu2_voice_info(int core, int vi, spu2_voice_info_t *o)
{
    const core_t *k = &C[core & 1];
    const voice_t *v = &k->v[vi % 24];
    uint32_t bit = 1u << (vi % 24);
    o->phase = v->phase;
    o->env = v->env;
    o->pitch = v->pitch;
    o->adsr1 = v->adsr1;
    o->adsr2 = v->adsr2;
    o->vol_l = (int16_t)v->vol[0].level;
    o->vol_r = (int16_t)v->vol[1].level;
    o->ssa = v->ssa << 1;
    o->lsa = v->lsa << 1;
    o->nax = v->nax << 1;
    o->noise = (k->non & bit) != 0;
    o->pmod = (k->pmon & bit) != 0;
    o->wet = ((k->vmixel | k->vmixer) & bit) != 0;
    return v->phase != PH_OFF;
}
