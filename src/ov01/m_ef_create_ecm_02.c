/*
 * OV01 original TU 31: 0x00a38190..0x00a387a8 (5 functions)
 */
#include "common.h"
#include "shared.h"

extern void MMathAddRotateVectorY(void *destination, float angle, const Vector4 *base, const Vector4 *offset);

typedef void (*ECM02Callback)(void *self, void *work);

typedef struct ECM02Packet {
    void *data;
    int capacity;
    void *current;
    int count;
    int aux_count;
} ECM02Packet;

typedef struct ECM02InitState {
    unsigned char unmodeled_00[0x30];
    Vector4 source_vector;
    unsigned char unmodeled_40[0x30];
    short frame;
    unsigned char unmodeled_72[0x0E];
    Vector4 copied_vector;
    u32 color[4];
    short visible[21];
    unsigned char unmodeled_ca[2];
    float radius[21];
    float angle[21];
    ECM02Packet packet;
} ECM02InitState;

typedef struct ECM02Effect {
    u32 unmodeled_00;
    ECM02Callback process_callback;
    u32 unmodeled_08;
    ECM02Callback draw_packet_callback;
    ECM02Callback post_process_callback;
    unsigned char unmodeled_14[0x0C];
    ECM02InitState state;
} ECM02Effect;

static void fnECM02_PR000(void *self, void *work);
static void fnECM02_DP000(void *self, void *work);
static void fnECM02_PO000(void *self, void *work);
extern void MGsGPInit(ECM02Packet *packet, void *address, int size);
extern float MMathMakeRandom2PI(void);

int MEfCreate_ECM02(void *self)
{
    ECM02Effect *effect = (ECM02Effect *)self;
    ECM02InitState *state = &effect->state;
    u32 *color_cursor;
    u32 remaining_color;
    int color_index;

    state->frame = 0;
    /* Preserve the original four-word vector before replacing its second float. */
    __asm__ __volatile__(
        "lq $8, 0(%1)\n\t"
        "sq $8, 0(%0)"
        :
        : "r"(&state->copied_vector), "r"(&state->source_vector)
        : "$8", "memory"
    );
    state->copied_vector.y = 11.0f;
    memset(state->visible, 0, sizeof(state->visible));
    memset(state->radius, 0, sizeof(state->radius));
    memset(state->angle, 0, sizeof(state->angle));

    remaining_color = 128;
    color_cursor = state->color;
    for (color_index = 2; color_index >= 0; color_index--) {
        *color_cursor = xglSRand() & 0x3F;
        if (*color_cursor > remaining_color) {
            *color_cursor = remaining_color;
        }
        remaining_color -= *color_cursor;
        color_cursor++;
    }

    state->color[3] = 255;
    state->visible[0] = 1;
    state->radius[0] = 0.0f;
    state->angle[0] = MMathMakeRandom2PI();
    MGsGPInit(&state->packet, 0, 0);

    effect->process_callback = fnECM02_PR000;
    effect->draw_packet_callback = fnECM02_DP000;
    effect->post_process_callback = fnECM02_PO000;
    return 1;
}

static void makeCoord(void *destination, float radius, float angle, void *effect)
{
    unsigned char *base = (unsigned char *)effect;
    Vector4 local;

    /* VF00 is hardwired to (0.0, 0.0, 0.0, 1.0); this
     * materializes that constant into "local" in one quadword store instead of
     * four scalar stores. Only z (0.0) and w (1.0) survive from this store: x
     * and y are overwritten immediately below. */
    __asm__ __volatile__(
        "sqc2 $vf0, 0(%1)"
        : "=m"(local)
        : "r"(&local)
        : "memory"
    );

    local.x = radius * 0.454545468f + 0.5f;
    local.y = -radius;

    MMathAddRotateVectorY(destination, angle, (Vector4 *)(base + 0x80), &local);
}

/*
 * work's layout beyond +0x70 pairs a per-frame lifetime counter with three
 * parallel 21-entry trails (visible flag, radius, angle) for the head this
 * function advances: index 0 is the value fnECM02_PR000 updates each frame,
 * indices 1..20 its trailing history, oldest last.
 */
#define ECM02_HIT_FRAME 0x23

typedef struct ECM02Trail {
    unsigned char unmodeled_00[0x70];
    short frame;                /* +0x70 */
    unsigned char unmodeled_72[0x9E - 0x72];
    short unmodeled_9e;         /* +0x9E: cleared every frame; role beyond that unresolved */
    short visible[21];          /* +0xA0 */
    unsigned char unmodeled_ca[0xCC - 0xCA];
    float radius[21];           /* +0xCC */
    float angle[21];            /* +0x120 */
} ECM02Trail;

static void fnECM02_PR000(void *self, void *work)
{
    ECM02Trail *trail = (ECM02Trail *)work;
    int i;

    for (i = 19; i >= 0; i--) {
        trail->visible[i + 1] = trail->visible[i];
        trail->radius[i + 1] = trail->radius[i];
        trail->angle[i + 1] = trail->angle[i];
    }
    /* i == -1 here: this reaches unmodeled_9e, one slot before visible[0]. */
    trail->visible[i] = 0;

    if (trail->frame < ECM02_HIT_FRAME) {
        trail->visible[0] = 1;
        trail->radius[0] += 0.35f;
        trail->angle[0] += 0.31415924f;
    }
}

typedef unsigned int ECM02Quad __attribute__((mode(TI)));
typedef union ECM02QW {
    ECM02Quad q;
    unsigned long ul[2];
    unsigned short us[8];
    unsigned int ui[4];
    unsigned char uc[16];
    volatile unsigned long vul[2];
    volatile unsigned char vuc[16];
} ECM02QW;
typedef struct ECM02GPacket {
    void *data;
    int capacity;
    ECM02QW *current;
    int count;
    int auxCount;
} ECM02GPacket;
typedef struct ECM02CamView {
    void *screen;
    void *matrix;
} ECM02CamView;
typedef struct ECM02Draw {
    unsigned char unmodeled_00[0x90];
    ECM02QW color;              /* +0x90 */
    short visible[21];          /* +0xA0 */
    unsigned char unmodeled_ca[0xCC - 0xCA];
    float radius[21];           /* +0xCC */
    float angle[21];            /* +0x120 */
    ECM02GPacket packet;        /* +0x174 */
} ECM02Draw;
extern ECM02CamView mefCamParams;
extern int MMathRotTransPersClip(ECM02QW *destination, const void *screen, const void *matrix, ECM02QW *point);
extern void MGsGPTerm(void *packet);
static inline void ecm02Flush(ECM02GPacket *pk, int nreg)
{
    int m = pk->auxCount * nreg + 1;

    pk->current->us[0] |= nreg;
    pk->current += m;
    pk->count += m;
}
static inline ECM02QW *ecm02Begin(ECM02GPacket *gp, unsigned long tag, unsigned long regs, int aux)
{
    ECM02QW *t = gp->current;
    ECM02QW *r = t + 1;

    t[0].ul[1] = regs;
    t[0].ul[0] = tag | ((unsigned long)aux << 60);
    gp->auxCount = aux;
    return r;
}

static void fnECM02_DP000(void *self, void *work)
{
    ECM02Draw *draw = (ECM02Draw *)work;
    ECM02QW col0;
    ECM02QW col1;
    ECM02QW pos0;
    ECM02QW pos1;
    ECM02QW out0;
    ECM02QW out1;
    void *screen;
    void *matrix;
    int n;
    int j;
    ECM02GPacket *pk;
    ECM02QW *q;
    ECM02QW *a1;
    ECM02QW *a2;
    ECM02QW *a3;
    ECM02QW *dst;
    int clip;
    int i;
    float ofs;

    screen = mefCamParams.screen;
    matrix = mefCamParams.matrix;
    pk = &draw->packet;
    MGsGPInit((ECM02Packet *)pk, 0, 0);
    q = pk->current;
    q[0].vul[0] = 0x1000000000008000;
    q[0].vul[1] = 0xE;
    pk->auxCount = 1;
    a1 = q + 1;
    a2 = q + 2;
    a3 = q + 3;
    a1->vuc[8] = 0x47;
    a1->vul[0] = 0x5000D;
    a2->vuc[8] = 0x4E;
    a2->vul[0] = 0x131000000;
    a3->vuc[8] = 0x42;
    a3->vul[0] = 0x48;
    ecm02Flush(pk, 3);

    for (j = -1; j < 2; j += 2) {
        clip = 0;
        n = 0;
        dst = ecm02Begin(&draw->packet, 0x0026400000008000, 0x5151, 4);
        for (i = 0; i < 20; i++) {
            if (draw->visible[i] != 0) {
                col1.q = col0.q = draw->color.q;
                col1.ui[3] = 0x20;
                if (i >= 10) {
                    col0.ui[3] = col0.ui[3] * (20 - i) / 10;
                    col1.ui[3] = col1.ui[3] * (20 - i) / 10;
                }
                if (i != 0 && i != 19) {
                    ofs = (float)j * 0.15f;
                } else {
                    ofs = 0.0f;
                }
                makeCoord(&pos0, draw->radius[i], draw->angle[i], work);
                makeCoord(&pos1, draw->radius[i] + ofs, draw->angle[i], work);
                if (MMathRotTransPersClip(&out0, screen, matrix, &pos0)) {
                    clip = 2;
                } else if (clip != 0) {
                    clip--;
                    out0.ui[3] = 0x8000;
                }
                if (MMathRotTransPersClip(&out1, screen, matrix, &pos1)) {
                    clip = 2;
                } else if (clip != 0) {
                    clip--;
                    out1.ui[3] = 0x8000;
                }
                n++;
                dst[0].q = col0.q;
                dst[1].q = out0.q;
                dst[2].q = col1.q;
                dst[3].q = out1.q;
                dst += 4;
            }
        }
        if (n > 0) {
            ecm02Flush(pk, n);
        }
    }
    MGsGPTerm(pk);
}

extern void sefHitEffect(void);
extern void MEfObjDestroy(void *self);

/*
 * work's layout beyond +0x70 is unresolved: the same per-frame lifetime
 * counter fnECM02_PR000 advances, expiring through MEfObjDestroy once it
 * reaches 0x37.
 */
#define ECM02_LIFETIME 0x37

typedef struct ECM02State {
    unsigned char unmodeled_00[0x70];
    short frame; /* +0x70 */
} ECM02State;

static void fnECM02_PO000(void *self, void *work)
{
    ECM02State *state = (ECM02State *)work;

    state->frame++;
    if (state->frame == ECM02_HIT_FRAME) {
        sefHitEffect();
    }
    if (state->frame >= ECM02_LIFETIME) {
        MEfObjDestroy(self);
    }
}
