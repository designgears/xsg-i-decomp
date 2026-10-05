#include "common.h"

typedef struct Mif2Argument {
    int type;
    union {
        int number;
        const char *text;
    } value;
} Mif2Argument;

extern int ctrlCodeFlags;

void MSG_init(void)
{
    ctrlCodeFlags = 0;
}

void MSG_convert(unsigned char *dst, int size, int a, int b);
int MSG_queuePush(void *q, unsigned char *msg, int size, int n);

void MSG_print2(unsigned char *p, int a, int b)
{
    unsigned char buf[0x400];

    if (*(unsigned short *)(p + 0x16) == 4) {
        buf[0] = 0x19;
        buf[1] = 1;
        MSG_convert(buf + 2, 0x3FE, a, b);
    } else {
        MSG_convert(buf, 0x400, a, b);
    }
    MSG_queuePush(p + 0xAC, buf, 0x400, -1);
}

void MSG_send(void)
{
}

INCLUDE_ASM("asm/main/nonmatchings/msg_init", MSG_getSize);

void MSG_dump(unsigned char *p, int size)
{
    int i, j;
    int c;
    char buf[16];

    for (i = 0; i < size / 16; i++) {
        for (j = 0; j < 16; j++) {
            c = p[j];
            if (c < 0x20) {
                buf[j] = '.';
            } else {
                buf[j] = c;
            }
        }
        p += 16;
    }
}

typedef struct { char pad[8]; short a; short b; short c; short d; short e; } MsgQueue_;

void MSG_queueReset(MsgQueue_ *q, int mode)
{
    if (mode & 0x100) {
        ctrlCodeFlags |= 1;
    } else {
        ctrlCodeFlags = 0;
    }
    mode &= 0xFF;
    if (mode == 0) {
        q->a = 0;
        q->b = 0;
        q->d = 0;
        q->e = 0;
    } else if (mode == 10) {
        q->d = 0;
        q->e = 0;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/msg_init", MSG_queueGetInfo);

INCLUDE_ASM("asm/main/nonmatchings/msg_init", MSG_queuePop);

INCLUDE_ASM("asm/main/nonmatchings/msg_init", MSG_copyln);

INCLUDE_ASM("asm/main/nonmatchings/msg_init", MSG_queuePush);

unsigned char *matchBracket(unsigned char *p)
{
    int open = *p++;
    int depth = 1;
    int close;
    int c;

    switch (open) {
    case '[':
        close = ']';
        break;
    case '(':
        close = ')';
        break;
    case '{':
        close = '}';
        break;
    default:
        return 0;
    }
    while ((c = *p++) != 0) {
        if (c == open) {
            depth++;
        } else if (c == close) {
            if (--depth <= 0) {
                break;
            }
        }
    }
    p--;
    if (*p == close) {
        return p;
    }
    return 0;
}

/*
 * Placeholder entry of the `mfunc` control-code table (listed twice there).
 * MSG_convert calls every entry as handler(out, argc, args) and continues
 * writing at the returned pointer; the other MIF2_* handlers emit their
 * control bytes at `out` and return the end. This one emits nothing: it only
 * walks the argument count and hands `out` back unchanged.
 */
char *MIF2_dummy(char *out, int argc)
{
    int i;

    for (i = 0; i < argc; i++) {
    }
    return out;
}

int PARSE_int(const char *s, int len, int base);

void MSG_setArgs(Mif2Argument *args, int n, const char **argv)
{
    int i;
    const char *s;
    unsigned char c;
    unsigned char c1;
    for (i = 0; i < n; i++) {
        s = argv[i];
        c = s[0];
        c1 = s[1];
        if (c != '$') {
            if ((unsigned)(c - '0') < 10 || (c == '-' && (unsigned)(c1 - '0') < 10)) {
                args[i].type = 0;
                if (c == '0' && c1 == 'x') {
                    args[i].value.number = PARSE_int(s + 2, -1, 16);
                } else {
                    args[i].value.number = PARSE_int(s, -1, 10);
                }
            } else {
                args[i].value.text = s;
                args[i].type = 1;
            }
        }
    }
}

extern int rubyX1;
extern int rubyX2;
extern int rubyY;
typedef struct RubyGlyph { char pad[8]; short x; short y; char pad2[12]; unsigned char flags; } RubyGlyph;

static void rubyFont(RubyGlyph *g, unsigned int code)
{
    unsigned int kind = code & 0xF0000000;
    g->flags |= 0x40;
    switch (kind) {
    case 0:
        rubyX1 = g->x;
        rubyY = g->y;
        break;
    case 0x10000000:
        rubyX2 = g->x;
        g->y -= 4;
        g->x -= (rubyX2 - rubyX1) / 2 + (code & 0xFF) * 4;
        break;
    case 0x20000000:
        g->x = rubyX2;
        g->y += 4;
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/msg_init", MIF2_ruby);

char *MIF2_font(char *out, int argc, int *args)
{
    unsigned char *s = (unsigned char *)args[1];
    unsigned char c;
    *out = 0x19;
    *++out = 1;
    for (;;) {
        out++;
        c = *s++;
        if (c == 0x2F) break;
        if (c == 0) break;
        *out = c;
    }
    *out = 0x19;
    *++out = 0;
    *++out = 0;
    return out;
}

void MSG_convert(unsigned char *dst, int size, int a, int b);
int MSG_queuePush(void *q, unsigned char *msg, int size, int n);
char *MIF2_color(char *out, int argc, unsigned char *args)
{
    unsigned int code = *(unsigned int *)(args + 4);
    char *end;
    *out = (char)0xC;
    *(out + 1) = (unsigned char)(code >> 16);
    *(out + 2) = (unsigned char)(code >> 8);
    *(out + 3) = (unsigned char)code;
    end = out + 4;
    *end = 0;
    return end;
}

char *MIF2_label(char *out, int argc, int *args)
{
    unsigned char *s = (unsigned char *)args[1];
    *out++ = 0x2F;
    *out++ = 0x5B;
    *out++ = 0x85;
    while ((*(unsigned char *)out++ = *s++) != 0);
    *out++ = 0x5D;
    *out = 0;
    return out;
}

char *MIF2_clear(char *out, int argc)
{
    char *end;

    *out = 0x2F;
    *(out + 1) = 0x5B;
    *(out + 2) = (char)0x81;
    *(out + 3) = 0x5D;
    end = out + 4;
    *end = 0;
    return end;
}

char *MIF2_close(char *out, int argc)
{
    char *end;

    *out = 0x2F;
    *(out + 1) = 0x5B;
    *(out + 2) = (char)0x84;
    *(out + 3) = 0x5D;
    end = out + 4;
    *end = 0;
    return end;
}

char *MIF2_waitkey(char *out, int argc, unsigned char *args)
{
    unsigned int code = *(unsigned int *)(args + 4);
    char *end;

    *out = 0x2F;
    *(out + 1) = 0x5B;
    *(out + 2) = (char)0x80;
    *(out + 3) = (unsigned char)code;
    *(out + 4) = 0x5D;
    end = out + 5;
    *end = 0;
    return end;
}

char *MIF2_wait(char *out, int argc, unsigned char *args)
{
    unsigned int code = *(unsigned int *)(args + 4);
    char *end;

    *out = 0x2F;
    *(out + 1) = 0x5B;
    *(out + 2) = (char)0x83;
    *(out + 3) = (unsigned char)code;
    *(out + 4) = 0x5D;
    end = out + 5;
    *end = 0;
    return end;
}

char *MIF2_gaiji(char *out, int argc, const Mif2Argument *args)
{
    int glyph = args[0].value.number + 160;
    char *end = &out[2];

    out[0] = -83;
    out[1] = glyph;
    *end = '\0';
    return end;
}

char *MIF2_code(char *out, int argc, const Mif2Argument *args)
{
    char *end = &out[1];
    unsigned char code = args[0].value.number;

    out[0] = code;
    end[0] = '\0';
    return end;
}

int strcmp(const char *, const char *);

/*
 * One row of the mfunc[] control-code table, which a NULL name terminates.
 * MSG_convert (still asm) calls the matched row's handler as
 * handler(out, argc), the same two parameters the placeholder row's own
 * MIF2_dummy above declares, and continues writing at the pointer it returns.
 */
typedef struct MsgFuncEntry {
    char *(*handler)(char *out, int argc);
    const char *name;
} MsgFuncEntry;

extern MsgFuncEntry mfunc[];

/*
 * Looks up name in the mfunc[] control-code table and returns the matching
 * row, or NULL if the table is empty or no row's name matches.
 */
static MsgFuncEntry *findMsgFunc(const char *name)
{
    MsgFuncEntry *entry = mfunc;

    if (entry->name != 0) {
        do {
            if (strcmp(name, entry->name) == 0) {
                return entry;
            }
            entry++;
        } while (entry->name != 0);
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/msg_init", MSG_convert);

/*
 * One record of the 8-slot mbuf pool msg_buffer reserves (2064 bytes each,
 * 8 * 2064 = msg_buffer's own 0x4080 size). MBUF_dispose (below) evidences
 * the leading word as a plain int, the same field MBUF_init clears here for
 * every slot.
 */
typedef struct MsgBuffer {
    int count;
    int size;
    int unmodeled_08;
    void *data;
    unsigned char unmodeled_10[2064 - 16];
} MsgBuffer;

extern MsgBuffer msg_buffer[8];

void MBUF_init(void)
{
    int slot;

    slot = 7;
    do {
        msg_buffer[slot].count = 0;
        slot--;
    } while (slot >= 0);
}

MsgBuffer *MBUF_create2(void)
{
    int slot;
    int found;
    MsgBuffer *m;
    MsgBuffer *r;

    found = -1;
    slot = 0;
    if (!(msg_buffer[0].count & 1)) {
        found = 0;
    } else {
loop:
        slot++;
        if (slot < 8) {
            m = (MsgBuffer *)((char *)(slot * 2064) + (int)msg_buffer);
            if (!(m->count & 1)) {
                found = slot;
            } else {
                goto loop;
            }
        }
    }
    if (found < 0) {
        return 0;
    }
    m = (MsgBuffer *)((char *)(found * 2064) + (int)msg_buffer);
    m->count = 1;
    m->size = 0x800;
    m->data = m->unmodeled_10;
    return m;
}

MsgBuffer *MBUF_create(int w, int n)
{
    int i;
    int found;
    char *buf;
    MsgBuffer *m;

    found = -1;
    i = 0;
    if (!(msg_buffer[0].count & 1)) {
        found = 0;
    } else {
loop:
        i++;
        if (i < 8) {
            m = (MsgBuffer *)((char *)(i * 2064) + (int)msg_buffer);
            if (!(m->count & 1)) {
                found = i;
            } else {
                goto loop;
            }
        }
    }
    if (found < 0) {
        return 0;
    }
    m = (MsgBuffer *)((char *)(found * 2064) + (int)msg_buffer);
    w = (w + 3) / 4 * 4;
    buf = (char *)m->unmodeled_10;
    m->count = 1;
    m->size = 0;
    m->unmodeled_08 = (int)buf;
    m->data = buf + n * 4;
    for (i = 0; i < n; i++) {
        *(char *)m->data = 0;
        ((void **)m->unmodeled_08)[i] = m->data;
        m->data = (char *)m->data + w;
    }
    return m;
}

/*
 * MBUF_init/MBUF_create (not yet recovered) establish the rest of the mbuf
 * layout; only the leading word is evidenced here.
 */
void MBUF_dispose(int *mbuf)
{
    mbuf[0] = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/msg_init", _copyCTRLCode);

static void _skipCTRLCode(unsigned char **pp)
{
    unsigned char *p = *pp;
    unsigned f;
    f = *p;
    p++;

    switch (f) {
    case 8:
        f = *p++;
        if (f & 1)
            p += 2;
        if (f & 2)
            p += 2;
        if (!(f & 4))
            break;
    case 6:
    case 12:
    case 15:
        p += 3;
        break;
    case 14:
        p += 5;
        break;
    case 18:
        p += 2;
        break;
    case 23:
        p += 9;
        break;
    case 25:
        if (*p != 0)
            ctrlCodeFlags |= 1;
        else
            ctrlCodeFlags &= ~1;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 13:
    case 24:
        p++;
        break;
    }
    *pp = p;
}
