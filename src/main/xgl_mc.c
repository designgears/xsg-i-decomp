#include "common.h"
#include "shared.h"
#include "xgl_mc.h"

/* Types recovered upstream (origin/master); kept for reference. */
/* The state machine uses both a word and per-slot halfwords at offset 4. */
typedef struct McRequestState {
    unsigned char phase;
    unsigned char unmodeled_01[3];
    union {
        int combined;
        short slots[2];
    } status;
} McRequestState;

/* Fields beyond this partial view remain in the complete retail workspace. */
typedef union McRequestStorage {
    McRequestState state;
    unsigned char bytes[0x20];
} McRequestStorage;


extern unsigned char D_0093E120[];

int sceMcOpen(int port, int slot, const char *name, int mode);
int sceMcClose(int fd);
int sceMcRead(int fd, void *buffer, int size);
int sceMcWrite(int fd, const void *buffer, int size);
int sceMcSync(int mode, int *command, int *result);
int sceMcGetInfo(int port, int slot, int *type, int *free, int *format);

typedef struct
{
    int a;
    unsigned char *name;
    int b;
    int c;
} XglMcParam;
typedef struct
{
    short port;
    short type;
    int *result;
    int a;
    int b;
    int c;
    unsigned char name[0x6C];
} XglMcQueue;
extern XglMcQueue queue[];

int xglMcRequest(int port, int type, XglMcParam *param, int *result)
{
    XglMcQueue *q;
    unsigned char *src;
    char *dst;
    int i;
    unsigned char *nm;
    short c;
    int a;
    int b;
    unsigned char end;

    end = queue_end;
    *result = 0;
    q = &queue[end];
    q->port = port;
    q->result = result;
    q->type = type;
    switch (type)
    {
    case 1:
    case 9:
    case 10:
        break;
    case 2:
    case 4:
    case 6:
    case 8:
        a = param->a;
        b = param->b;
        q->a = a;
        q->b = b;
        q->c = param->c;
        break;
    case 3:
    case 5:
    case 7:
        a = param->a;
        b = param->b;
        q->a = a;
        q->b = b;
        nm = param->name;
        q->c = param->c;
        if (nm == 0)
        {
            *(int *)q->name = 0;
        }
        else
        {
            src = nm;
            dst = (unsigned char *)q->name;
            i = 0;
            if (((unsigned int)(*dst = *src) << 24) != 0)
            {
                for (;;)
                {
                    i++;
                    src++;
                    dst++;
                    if (i >= 0x6C)
                        break;
                    if (((unsigned int)(*dst = *src) << 24) == 0)
                        break;
                }
            }
        }
        break;
    }
    queue_end = (end + 1) & 7;
    return 0;
}

/*
 * The word xglMcGetState reports lives at mw+4; mw+0 is the byte the request
 * state machine of xglMcMain switches on (seven states, jump table at
 * 0x004D2560) and xglMcMain writes the halfwords of the same +4 window per
 * slot (`sh` through `mw + 4 + 2 * mw[8]`, 0x00220600..0x0022069C).  The rest
 * of the record is still assembly, so mw cannot become a struct without
 * inventing the bytes between its members (docs/naming.md); the one access
 * takes docs/style.md rule 2's named-offset fallback.
 */
#define XGL_MC_STATE_OFFSET 4

int xglMcGetState(void)
{
    return *(int *)(mw + XGL_MC_STATE_OFFSET);
}

void xglMcReset(void)
{
    queue_top = 0;
    queue_end = 0;
    xglMcSetMapName(0, 0);
    mw[0] = 0;
}

/*
 * Converts one EUC-JIS double-byte character to Shift-JIS in place: hi and
 * lo hold the character's lead and trail byte, EUC-encoded on entry and
 * Shift-JIS on return. Called from xglMcSetMapName while building a memory
 * card map name.
 */
static void xglMcEUC2SJIS(u8 *hi, u8 *lo)
{
    u32 hi_byte;
    int lo_byte;
    u8 sjis_hi;
    u8 sjis_lo;

    hi_byte = ((*hi) + 0x80) & 0xFF;
    lo_byte = ((*lo) + 0x80) & 0xFF;
    sjis_hi = hi_byte;
    if (sjis_hi & 1)
    {
        lo_byte = lo_byte + 0x1F;
        sjis_hi = (sjis_hi >> 1) + 0x71;
    }
    else
    {
        lo_byte = lo_byte + 0x7D;
        sjis_hi = (sjis_hi >> 1) + 0x70;
    }
    sjis_lo = lo_byte & 0xFF;
    if (sjis_hi >= 0xA0U)
    {
        sjis_hi = (sjis_hi + 0x40) & 0xFF;
    }
    if (sjis_lo >= 0x7FU)
    {
        sjis_lo = (sjis_lo + 1) & 0xFF;
    }
    if ((sjis_hi == 0x87) && (sjis_lo == 0x54))
    {
        sjis_hi = 0x82;
        sjis_lo = 0x50;
    }
    *hi = sjis_hi;
    *lo = sjis_lo;
}

void xglMcSetMapName(const unsigned char *primary_euc_name,
                     const unsigned char *secondary_euc_name)
{
    const unsigned char *primary;
    unsigned char *destination;
    int character_count;
    unsigned char lead;
    unsigned char trail;

    primary = primary_euc_name;
    destination = D_0093E120;
    character_count = 0;

    if (primary != 0)
    {
        lead = primary[0];
        if (lead != 0)
        {
            do
            {
                trail = primary[1];
                primary += 2;
                xglMcEUC2SJIS(&lead, &trail);
                character_count++;
                destination[0] = lead;
                destination[1] = trail;
                destination += 2;
                if (character_count >= 16)
                {
                    break;
                }
                lead = ((const volatile unsigned char *)primary)[0];
            } while (lead != 0);
        }

        if ((character_count < 16) && (secondary_euc_name != 0) &&
            (((const signed char *)secondary_euc_name)[0] != 0))
        {
            destination[0] = 0x81;
            destination[1] = 0x45;
            destination += 2;
            character_count++;
        }
    }

    if ((secondary_euc_name != 0) && (character_count < 16))
    {
        lead = secondary_euc_name[0];
        if (lead != 0)
        {
            do
            {
                trail = secondary_euc_name[1];
                secondary_euc_name += 2;
                xglMcEUC2SJIS(&lead, &trail);
                character_count++;
                destination[0] = lead;
                destination[1] = trail;
                destination += 2;
                if (character_count >= 16)
                {
                    break;
                }
                lead = ((const unsigned char *)secondary_euc_name)[0];
            } while (lead != 0);
        }
    }

    while (character_count < 17)
    {
        destination[0] = 0;
        destination[1] = 0;
        destination += 2;
        character_count++;
    }
}

void xglMcWriteMapName(char *buffer, int slot)
{
    int value = slot + 1;
    int digit = value % 10;

    buffer[229] = digit + 'O';
    value /= 10;
    buffer[227] = value % 10 + 'O';
}

extern char base_0_00491770[];
extern char n_1[];
extern char n_2[];
extern char D_0093E0C0[];

/*
 * xglMcSetFullPath (main:0x0021fed8): builds "<base><card 2 digits>" in
 * D_0093E0C0, then appends a suffix chosen by `path`: 0 repeats the
 * prefix, -1 nothing, 1/2 the fixed names n_1/n_2, anything else is a
 * string appended after a '/'.
 */
char *xglMcSetFullPath(int card, char *path)
{
    char *s;
    char *d;
    char *e;

    s = base_0_00491770;
    d = D_0093E0C0;
    while ((*d = *s) != 0) {
        s++;
        d++;
    }
    if (card < 0) {
        *d++ = '*';
    } else {
        d[1] = card % 10 + '0';
        d[0] = card / 10 % 10 + '0';
        d += 2;
    }
    switch ((int)path) {
    case 0:
        e = d;
        for (s = D_0093E0C0; s != e; s++, d++) {
            *d = *s;
        }
        *d = 0;
        break;
    case 1:
        s = n_1;
        while ((*d = *s) != 0) {
            s++;
            d++;
        }
        break;
    case 2:
        s = n_2;
        while ((*d = *s) != 0) {
            s++;
            d++;
        }
        break;
    case -1:
        *d = 0;
        break;
    default:
        *d++ = '/';
        s = path;
        while ((*d = *s) != 0) {
            s++;
            d++;
        }
        break;
    }
    return D_0093E0C0;
}

int sceMcGetDir(int port, int slot, const char *name, unsigned mode, int maxent, void *table);
int sceMcMkdir(int port, int slot, const char *name);
int sceMcFormat(int port, int slot);
int sceMcUnformat(int port, int slot);
extern char D_0093E0A1[];
static void execute(XglMcQueue *q)
{
    int v = 4;
    char *s;
    unsigned char *p;

    switch (q->type)
    {
    case 1:
        sceMcGetInfo(q->port, 0, 0, q->result, 0);
        return;
    case 2:
        if (q->a == -1) {
            char *_g;
            xglMcSetFullPath(-1, (char *)-1);
            _g = D_0093E0C0;
            sceMcGetDir(q->port, 0, _g, 0, 100, (void *)((q->b + 0xA3) & ~0x3F));
            _g[-0x20] = 6;
            return;
        }
        xglMcSetFullPath(q->a, 0);
        sceMcOpen(q->port, 0, D_0093E0C0, 1);
        p = D_0093E0C0 - 0x20;
        p[0] = 2;
        return;
    case 3:
        xglMcSetFullPath(0, q->name);
        sceMcOpen(q->port, 0, D_0093E0C0, 1);
        p = D_0093E0C0 - 0x20;
        p[0] = 2;
        return;
    case 5:
    case 7:
    {
        unsigned char w;
        short t;

        xglMcSetFullPath(0, q->name);
        t = q->type;
        switch (t)
        {
        case 7:
            {
                s = D_0093E0C0;
                sceMcOpen(q->port, 0, s, 0x202);
                s[-0x20] = 3;
            }
            break;
        default:
            {
                sceMcOpen(q->port, 0, (char *)D_0093E0C0, 1);
                s = D_0093E0C0;
                s[-0x20] = 4;
            }
            break;
        }
        *D_0093E0A1 = 0;
        return;
    }
    case 8:
        p = D_0093E0C0 - 0x20;
        xglMcSetFullPath(q->a, (char *)-1);
        s = D_0093E0C0;
        sceMcMkdir(q->port, 0, s);
        v = 5;
        goto set;
    case 6:
        p = D_0093E0C0 - 0x20;
        xglMcSetFullPath(q->a, 0);
        s = D_0093E0C0;
        sceMcOpen(q->port, 0, s, 0x202);
        v = 3;
        goto set;
    case 4:
        do {
            p = (unsigned char *)D_0093E0C0 - 0x20;
            xglMcSetFullPath(q->a, 0);
            s = (char *)(unsigned char *)D_0093E0C0;
            sceMcOpen(q->port, 0, s, 1u);
            v = (unsigned char)4;
        set:
            do { p[1] = 0; } while (0);
            p[0] = (unsigned char)v;
        } while (0);
        break;
    case 9:
        sceMcFormat(q->port, 0);
        return;
    case 10:
        sceMcUnformat(q->port, 0);
        break;
    default:
        break;
    }
}

typedef struct McSavePayload {
    unsigned char unmodeled_00[4];
    int entry_count;
    unsigned char unmodeled_08[8];
    int group_count;
} McSavePayload;
typedef struct McSaveHeader {
    unsigned char unmodeled_00[4];
    int start;
    int end;
    unsigned char unmodeled_0c[0x34];
    char icon_header[0x400];
    McSavePayload payload;
} McSaveHeader;
typedef struct McSaveChecksum {
    unsigned char unmodeled_00[28];
    float checksum;
} McSaveChecksum;
typedef struct McWorkState {
    unsigned char phase;
    unsigned char subphase;
    unsigned char unmodeled_02;
    unsigned char next_card;
    int status;
    int descriptor;
    unsigned char unmodeled_0c[0x14];
} McWorkState;
typedef struct McWorkspaceView {
    McWorkState state;
    unsigned char full_path[96];
    unsigned char map_name[36];
    XglMcQueue request;
} McWorkspaceView;
extern float D_004D8794;
float xglFRand(void);
int sceMcDelete(int port, int slot, const char *name);
void FlushCache(int mode);
#define MC_WORKSPACE (&*(McWorkspaceView *)mw)

static int create_sub2(int result)
{
    int slot = MC_WORKSPACE->request.a;
    McSaveHeader *save;
    McSavePayload *payload;
    McSaveChecksum *trailer;
    float *checksum;
    int size;
    unsigned char *write_data;

    if (MC_WORKSPACE->state.subphase < 2 && (result >= 0 || result == -4)) {
        if (MC_WORKSPACE->state.subphase == 0) {
            xglMcSetFullPath(slot, 0);
            sceMcDelete(MC_WORKSPACE->request.port, 0, (char *)MC_WORKSPACE->full_path);
        } else {
            xglMcSetFullPath(slot, (char *)1);
            sceMcOpen(MC_WORKSPACE->request.port, 0, (char *)mw + 32, 0x202);
        }
    } else {
        if (result < 0)
            goto store_error;
        save = (McSaveHeader *)MC_WORKSPACE->request.b;
        switch (MC_WORKSPACE->state.subphase) {
        case 0:
        case 1:
        case 2:
            MC_WORKSPACE->state.descriptor = result;
            write_data = (unsigned char *)save->icon_header;
            xglMcWriteMapName((char *)write_data, slot);
            sceMcWrite(MC_WORKSPACE->state.descriptor, write_data, 964);
            break;
        case 3:
            sceMcClose(MC_WORKSPACE->state.descriptor);
            break;
        case 4:
            xglMcSetFullPath(slot, (char *)2);
            {
                McWorkspaceView *view = (McWorkspaceView *)mw;
                sceMcOpen(view->request.port, 0, (char *)mw + 32, 0x202);
            }
            break;
        case 5:
            MC_WORKSPACE->state.descriptor = result;
            write_data = (unsigned char *)&save->payload;
            payload = (McSavePayload *)write_data;
            trailer = (McSaveChecksum *)((unsigned char *)payload + (payload->entry_count * 8 + 16) * payload->group_count);
            checksum = &trailer->checksum;
            size = save->end - save->start - 40;
            *checksum = (xglFRand() - 1.5f) * D_004D8794 + 1.0f;
            FlushCache(0);
            sceMcWrite(MC_WORKSPACE->state.descriptor, write_data, size);
            break;
        case 6:
            sceMcClose(MC_WORKSPACE->state.descriptor);
            return 1;
        default:
            break;
        }
    }
    MC_WORKSPACE->state.subphase++;
    *MC_WORKSPACE->request.result = MC_WORKSPACE->state.subphase;
    return 0;

store_error:
    *MC_WORKSPACE->request.result = result;
    return -1;
}

extern unsigned char D_0093E144[];
extern int D_0093E0A4[];
int create_sub2(int);
static inline unsigned char *mw_addr(void)
{
    return mw;
}

int xglMcMain(void)
{
    unsigned char *p;
    int sp10;
    int sp14;
    int temp_2;
    int c;
    int flag;
    short *t;
    XglMcQueue *q;
    unsigned char *dst;
    unsigned char *src;
    XglClock clk;
    unsigned char *temp_19;
    unsigned char *var_16;
    unsigned char *var_2_3;
    int var_18;
    int var_18_2;
    unsigned int var_21;
    unsigned int temp_2_3;
    int temp_17;
    int var_22;
    int temp_2_4;
    int *wv;
    int *wv2;
    int c2;

    temp_2 = sceMcSync(1, &sp10, &sp14);
    if (temp_2 == 0) {
        /* state 1: reply pending */
        if (mw[0] == 1 && queue_end == queue_top) {
            return *(int *)(mw + 4);
        }
        return 0xFF00FF;
    }
    /* sync error (-1): clear both results */
    if (temp_2 == -1) {
        sp14 = 0;
        sp10 = 0;
    }
    switch (mw_addr()[0]) {
    case 0:
        goto block_36;
    case 1:
        if (queue_end != queue_top) {
        } else {
            wv = (int *)mw;
            c = wv[3];
            t = (short *)(mw + 4) + wv[2];
            switch (c) {
            case 1:
                *t = 0x81;
                break;
            case 2:
                *t = 1;
                break;
            case 3:
                *t = 0x82;
                break;
            default:
                *t = 0;
                break;
            }
            if ((unsigned int)(sp14 + 2) < 2U) {
                *t = 0x80;
                goto block_33;
            }
            /* the halfword stores above may overlap mw+12: re-read it */
            wv2 = (int *)mw;
            c2 = wv2[3];
            if (c2 != 2) {
            } else if (sp14 != 0) {
            } else {
                if (wv2[5] == 0) {
                    *t = c2;
                }
block_33:
            }
        }
        mw[0] = 0;
block_36:
        if (queue_end != queue_top) {
            q = queue + queue_top;
            dst = D_0093E144;
            src = (unsigned char *)q;
            __builtin_memcpy(dst, src, 0x80);
            execute(q);
            queue_top = (queue_top + 1) & 7;
            return 0xFF00FF;
        }
        flag = (mw[3] & 1) & 0xFF;
        *(int *)(mw + 8) = flag;
        sceMcGetInfo(flag, 0, (int *)(mw + 0xC), 0, (int *)(mw + 0x14));
        mw[0] = 1;
        mw[3] = mw[3] ^ 1;
        goto block_ret;
    case 2:
        {
            unsigned char *p = mw_addr();
            *(int *)(*(int *)(p + 0xA8)) = sp14;
            p[0] = 0;
            if (sp14 < 0) {
                goto block_ret;
            }
            sceMcClose(sp14);
            return 0xFF00FF;
        }
    case 6:
        var_18 = 0x64;
        *(int *)(*(int *)(mw_addr() + 0xA8)) = sp14;
        temp_19 = (unsigned char *)*(int *)(mw_addr() + 0xB0);
        var_2_3 = temp_19 + 0x64;
        do {
            var_18 -= 1;
            *var_2_3 = 0;
            var_2_3 -= 1;
        } while (var_18 >= 0);
        if (sp14 > 0) {
        var_21 = 0;
        var_22 = (var_18 = 0);

        var_16 = (unsigned char *)((*(int *)(mw_addr() + 0xB0) + 0xA3) & ~0x3F);
        for (; var_18 < sp14; var_18++) {
            temp_17 = ((var_16[0x30] * 0xA) + var_16[0x31]) - 0x210;
            temp_19[temp_17] = 1;
            clk.year = *(unsigned short *)(var_16 + 0xE);
            clk.month = var_16[0xD];
            clk.day = var_16[0xC];
            clk.hour = var_16[0xB];
            clk.minute = var_16[0xA];
            clk.second = var_16[9];
            clk.status = 0;
            temp_2_3 = xglClockDayTime2UInt(&clk);
            if (var_21 < temp_2_3) {
                var_21 = temp_2_3;
                var_22 = temp_17;
            }
            var_16 += 0x40;
        }
        temp_19[0x64] = var_22;
        }
        mw[0] = 0;
        goto block_ret;
    case 5:
        temp_2_4 = create_sub2(sp14);
        if (temp_2_4 == 0) {
            return 0xFF00FF;
        }
        {
            unsigned char *p5 = mw_addr();
            p5[0] = 0;
            if (temp_2_4 < 0) {
                *(int *)(p5 + 4) = 0xFF00FF;
            }
            /* r < 0 reports the error word */
        }
        goto block_ret;
    case 3:
    case 4:
        if (sp14 >= 0) {
        if (mw[1] == 0) {
            unsigned char type = mw[0];
            void *buf;
            int n;
            *(int *)(mw + 8) = sp14;
            n = *(int *)(mw + 0xB4);
            buf = *(void **)(mw + 0xB0);
            switch (type) {
            case 3:
                sceMcWrite(sp14, buf, n);
                break;
            case 4:
                sceMcRead(sp14, buf, n);
                break;
            }
            mw_addr()[1] = 1;
            return 0xFF00FF;
        }
        sceMcClose(*(int *)(mw + 8));
        goto c34_end;
        }
        {
            unsigned char *pe = mw_addr();
            *(int *)(*(int *)(pe + 0xA8)) = sp14;
            *(int *)(pe + 4) = 0xFF00FF;
        }
    c34_end:
        mw[0] = 0;
        goto block_ret;
    default:
        goto block_ret;
    }
block_49:
block_50:
    return 0xFF00FF;
block_ret:
    return D_0093E0A4[0];
}

int xglMcEasyLoad(const char *path, void *buffer, int size)
{
    int card_type;
    int command;
    int result;
    int descriptor;
    int read_result;

    sceMcGetInfo(0, 0, 0, 0, &card_type);
    sceMcSync(0, &command, &result);
    if (card_type == 0)
    {
        return -1;
    }
    if (result < -1)
    {
        return result;
    }

    sceMcOpen(0, 0, path, 1);
    sceMcSync(0, &command, &result);
    descriptor = result;
    if (descriptor < 0)
    {
        return descriptor;
    }

    sceMcRead(descriptor, buffer, size);
    sceMcSync(0, &command, &result);
    read_result = result;
    if (read_result < 0)
    {
        return read_result;
    }

    sceMcClose(descriptor);
    sceMcSync(0, &command, &result);
    if (result < 0)
    {
        return result;
    }
    return read_result;
}

int xglMcEasySave(const char *path, const void *buffer, int size)
{
    int card_type;
    int command;
    int result;
    int descriptor;

    sceMcGetInfo(0, 0, 0, 0, &card_type);
    sceMcSync(0, &command, &result);
    if (result < -1 || card_type == 0)
    {
        return -1;
    }

    sceMcOpen(0, 0, path, 0x202);
    sceMcSync(0, &command, &result);
    descriptor = result;
    if (descriptor < 0)
    {
        return -1;
    }

    sceMcWrite(descriptor, buffer, size);
    sceMcSync(0, &command, &result);
    if (result < 0)
    {
        return -1;
    }

    sceMcClose(descriptor);
    sceMcSync(0, &command, &result);
    return result >= 0 ? 0 : -1;
}

void xglMcInitial(void)
{
    sceMcInit();
    xglMcReset();
}
