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

unsigned int MSG_convert(unsigned char *destination, int capacity, const char *text, int length);
struct MsgQueue;
int MSG_queuePush(struct MsgQueue *q, unsigned char *msg, int size, int n);

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

int MSG_getSize(const unsigned char *message)
{
    int code;
    const unsigned char *cursor = message;

    for (;;) {
        code = *cursor++;
        if (code == '/' && *cursor == '[') {
            const unsigned char *bracketCursor = cursor;
            int bracketCode;

            do {
                bracketCode = *bracketCursor++;
                if (bracketCode < 0x20) {
                    switch (bracketCode) {
                    case 0x01:
                    case 0x02:
                    case 0x03:
                    case 0x04:
                    case 0x05:
                    case 0x0d:
                    case 0x18:
                    case 0x19:
                        bracketCursor++;
                        break;
                    case 0x07:
                    case 0x09:
                    case 0x0a:
                    case 0x0b:
                    case 0x10:
                    case 0x11:
                    case 0x12:
                    case 0x13:
                    case 0x14:
                    case 0x15:
                    case 0x16:
                        break;
                    case 0x08:
                        bracketCode = *bracketCursor++;
                        if (bracketCode & 1) {
                            bracketCursor += 2;
                        }
                        if (bracketCode & 2) {
                            bracketCursor += 2;
                        }
                        if (!(bracketCode & 4)) {
                            break;
                        }
                        /* The optional color has the same three-byte extent. */
                    case 0x06:
                    case 0x0c:
                    case 0x0f:
                        bracketCursor += 3;
                        break;
                    case 0x0e:
                        bracketCursor += 5;
                        break;
                    case 0x17:
                        bracketCursor += 7;
                        break;
                    default:
                        break;
                    }
                } else if (bracketCode > 0x8d) {
                    bracketCursor++;
                } else if (bracketCode == ']') {
                    break;
                }
            } while (1);
            cursor = bracketCursor;
            continue;
        }

        if (code == 0x0a) {
            continue;
        }
        if (code == 0) {
            break;
        }
        if (code < 0x20) {
            switch (code) {
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x0d:
            case 0x18:
            case 0x19:
                cursor++;
                break;
            case 0x07:
            case 0x09:
            case 0x0b:
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x13:
            case 0x14:
            case 0x15:
            case 0x16:
                break;
            case 0x08:
                code = *cursor++;
                if (code & 1) {
                    cursor += 2;
                }
                if (code & 2) {
                    cursor += 2;
                }
                if (!(code & 4)) {
                    break;
                }
                /* The optional color has the same three-byte extent. */
            case 0x06:
            case 0x0c:
            case 0x0f:
                cursor += 3;
                break;
            case 0x0e:
                cursor += 5;
                break;
            case 0x17:
                cursor += 7;
                break;
            default:
                break;
            }
        } else if (code >= 0x8e) {
            cursor++;
        }
    }
    return cursor - message - 1;
}

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

static void _skipCTRLCode(unsigned char **pp);
int xglFontCheckProportional(unsigned char c0, unsigned char c1);
unsigned int xglFontGetProportionalSize(int code);

void MSG_queueGetInfo(void *arg0, void *arg1)
{
    unsigned char *sp;
    unsigned char *p;
    unsigned char *q;
    unsigned char *b;
    unsigned char *e;
    int c;
    int chars = 0;
    int w = 0;
    int maxA = 0;
    int maxB = 0;
    int lines = 0;
    unsigned int t;
    unsigned char *end;
    int r;
    int k;

    q = (unsigned char *)(*(int *)arg0 + *(short *)((char *)arg0 + 0x8));
    end = (unsigned char *)(*(int *)arg0 + *(short *)((char *)arg0 + 0xA));
    sp = q;
    p = q;
    while (p < end) {
        q = p;
        q++;
        c = *p;
        sp = q;
        if (c == '/') {
            if (*sp != '[') {
                goto chain;
            }
            q++;
            sp = q;
            c = *q++;
            sp = q;
            if (c >= 0x80) {
                switch (c) {
                case 0x85:
                    do {
                        e = sp;
                        k = *e;
                        q = e + 1;
                        sp = q;
                    } while (k != 0);
                    break;
                case 0x80:
                case 0x82:
                case 0x83:
                    q = sp + 1;
                    sp = q;
                    break;
                case 0x5D:
                default:
                    break;
                }
                q = sp + 1;
                sp = q;
                goto done;
            }
            sp = q - 1;
            for (;;) {
                b = sp;
                c = *b;
                q = b + 1;
                sp = q;
                if (c < 0x20) {
                    sp = b;
                    _skipCTRLCode(&sp);
                    continue;
                }
                if (c == 0x5D) {
                    break;
                }
                if (c >= 0x8E) {
                    q = b + 2;
                    sp = q;
                }
            }
            do {
                goto next;
            } while (0);
        }
        q = sp;
    chain:
        {
            if (c == 10) {
                if (maxA < w) {
                    maxA = w;
                }
                w = 0;
                if (maxB < chars) {
                    maxB = chars;
                }
                chars = 0;
                lines++;
            } else if (c < 0x20) {
                q = q - 1;
                sp = q;
                _skipCTRLCode(&sp);
                q = sp;
            } else {
                chars++;
                if (c >= 0xA0) {
                    r = xglFontCheckProportional(c, *q);
                    if ((ctrlCodeFlags & 1) && r >= 0) {
                        q = sp + 1;
                        sp = q;
                        t = xglFontGetProportionalSize(r);
                        w = w + (((t >> 8) & 0xFF) - (t & 0xFF)) + 1;
                        q = sp;
                    } else {
                        w += 0x14;
                        q = sp + 1;
                        sp = q;
                    }
                } else {
                    w += 8;
                }
            }
        }
    next:
        p = q;
    }
done:
    if (maxA < w) {
        maxA = w;
    }
    if (w > 0) {
        lines++;
    }
    if (maxB < chars) {
        maxB = chars;
    }
    *(short *)((char *)arg1 + 0x4) = maxA;
    *(short *)((char *)arg1 + 0x0) = maxB;
    *(short *)((char *)arg1 + 0x2) = lines;
}

static void _copyCTRLCode(unsigned char **dstp, unsigned char **srcp);
int xglFontGetStringWidth(unsigned char *s);
int xglFontGetStringWidth2(unsigned char *s, int n);
unsigned short xglFontAscii2Euc(char code, char **textp);
extern unsigned char D_004DA3B0[];

int MSG_queuePop(unsigned char *q, unsigned char *dst, int n)
{
    unsigned char *out;
    unsigned char *in;
    unsigned char *end;
    unsigned char *base;
    unsigned char *start;
    int limit;
    int count;
    int more;
    int flag;
    unsigned int s;
    unsigned int r;
    int t;
    int c;
    int c3;

    count = 0;
    limit = (n < 0) ? 0x400 : n;
    more = count < limit;
    base = *(unsigned char **)q;
    start = base + *(short *)(q + 8);
    end = base + *(short *)(q + 0xA);
    in = start;
    *end = 0;
    out = dst;
    xglFontGetStringWidth(D_004DA3B0);
    *(short *)(q + 0x12) = 0;

    while (more && in < end) {
        c = *in;
        in++;
        if (c == '/' && in[0] == '[') {
            in++;
            c = *in++;
            if (c >= 0x80) {
                *(short *)(q + 0x12) = c;
                switch (c) {
                case 0x5D: case 0x5E: case 0x5F: case 0x60:
                case 0x61: case 0x62: case 0x63: case 0x64:
                case 0x65: case 0x66: case 0x67: case 0x68:
                case 0x69: case 0x6A: case 0x6B: case 0x6C:
                case 0x6D: case 0x6E: case 0x6F: case 0x70:
                case 0x71: case 0x72: case 0x73: case 0x74:
                case 0x75: case 0x76: case 0x77: case 0x78:
                case 0x79: case 0x7A: case 0x7B: case 0x7C:
                case 0x7D: case 0x7E: case 0x7F:
                case 0x81: case 0x84:
                    break;
                case 0x85:
                    *(unsigned char **)(q + 0x1C) = in;
                    while (*in++ != 0) {
                    }
                    break;
                case 0x80:
                case 0x82:
                case 0x83:
                    *(unsigned int *)(q + 0x1C) = *in++;
                    break;
                }
                in++;
                break;
            }
            in--;
            for (;;) {
                c = *in;
                in++;
                if (c < 0x20) {
                    in--;
                    _copyCTRLCode(&out, &in);
                    continue;
                }
                if (c == ']') {
                    break;
                }
                *out++ = c;
                if (c > 0x8D) {
                    *out++ = *in++;
                }
            }
            continue;
        }
        if (c == '\n') {
            *out++ = c;
            *(short *)(q + 0x12) = c;
            break;
        }
        if (c < 0x20) {
            in--;
            _copyCTRLCode(&out, &in);
            continue;
        }
        if (c == ' ') {
            if (*(short *)(q + 8) != 0 && *(short *)(q + 0x10) == 0 && in[1] != 0x20) {
                continue;
            }
            c3 = xglFontGetStringWidth2(in, 2);
            if (*(short *)(q + 0x10) + c3 >= *(short *)(q + 0x14)) {
                *out++ = 0x0A;
                *(short *)(q + 0x12) = 0x0A;
                break;
            }
        }
        c3 = *in;
        if (c < 0xA0) {
            in--;
            r = xglFontAscii2Euc((char)c, (char **)&in);
            c = (r >> 8) & 0xFFFF;
            c3 = r & 0xFF;
            in++;
            flag = 1;
        } else {
            in++;
            flag = ctrlCodeFlags & 1;
        }
        count += 1;
        *out++ = c;
        if (c >= 0xA0) {
            t = xglFontCheckProportional(c & 0xFF, c3 & 0xFF);
            *out++ = c3;
            if (flag != 0 && t >= 0) {
                more = count < limit;
                s = xglFontGetProportionalSize(t);
                *(short *)(q + 0x10) = (short)(*(unsigned short *)(q + 0x10) + (((s >> 8) & 0xFF) - (s & 0xFF)) + 1);
            } else {
                more = count < limit;
                *(short *)(q + 0x10) = *(short *)(q + 0x10) + 0x14;
            }
        } else {
            more = count < limit;
            *(short *)(q + 0x10) = *(short *)(q + 0x10) + 8;
        }
    }
    *out = 0;
    *(short *)(q + 8) = (short)(*(unsigned short *)(q + 8) + ((unsigned short)(unsigned int)in - (unsigned int)start));
    *(unsigned short *)(q + 0xE) = (unsigned short)(*(unsigned short *)(q + 0xE) + count);
    return out - dst;
}

static void _copyCTRLCode(unsigned char **dstp, unsigned char **srcp);
unsigned short xglFontAscii2Euc(char code, char **textp);

int MSG_copyln(unsigned char **dst, unsigned char **src, int flag)
{
    unsigned char *d;
    unsigned char *s;
    unsigned char *q;
    unsigned char *p;
    unsigned char *t;
    int count;
    int c;
    unsigned char ch;
    int trail;
    int prop;
    int f1;
    int pflag;
    unsigned int sz;
    unsigned short r;

    d = *dst;
    q = *src;
    s = q;
    count = 0;
    if (d == 0) {
        return 0;
    }
    for (;;) {
        t = s;
        c = *t;
        q = t + 1;
        s = q;
        if (c == 0) {
            s = 0;
            break;
        }
        if (c == '\n') {
            if (flag & 2) {
                *d = c;
                d++;
            }
            if (*s == 0) {
                q = 0;
                s = q;
            }
            break;
        }
        if (c == '/') {
            if (*s != '[') {
                goto chain;
            }
            f1 = flag & 1;
            if (f1) {
                *d = c;
                d++;
                *d = *s;
                q = s + 1;
                s = q;
                d++;
            } else {
                q = s + 1;
                s = q;
            }
            c = *s;
            q = s + 1;
            s = q;
            if (c >= 0x80) {
                if (flag == 0) {
                    continue;
                }
                *d = c;
                d++;
                switch (c) {
                case 0x85:
                    do {
                        *d = *s;
                        t = s;
                        ch = *d;
                        q = t + 1;
                        d++;
                        s = q;
                    } while (ch != 0);
                    break;
                case 0x80:
                case 0x82:
                case 0x83:
                    *d = *s;
                    q = s + 1;
                    s = q;
                    d++;
                    break;
                case 0x5D:
                default:
                    break;
                }
                *d = *s;
                q = s + 1;
                s = q;
                d++;
                continue;
            }
            s--;
            for (;;) {
                c = *s;
                q = s + 1;
                s = q;
                if (c < 0x20) {
                    s--;
                    _copyCTRLCode(&d, &s);
                    continue;
                }
                if (c == ']') {
                    break;
                }
                p = d;
                *p = c;
                p++;
                d = p;
                if (c >= 0x8E) {
                    q = s + 1;
                    *p = *s;
                    s = q;
                    d = p + 1;
                }
            }
            if (!f1) {
                continue;
            }
            *d++ = c;
            do {
                goto next;
            } while (0);
        }
        q = s;
    chain:
        if (c < 0x20) {
            q--;
            s = q;
            _copyCTRLCode(&d, &s);
            continue;
        }
        trail = *q;
        if (c < 0xA0) {
            q--;
            s = q;
            r = xglFontAscii2Euc((char)c, (char **)&s);
            c = (r >> 8) & 0xFFFF;
            trail = r & 0xFF;
            q = s + 1;
            s = q;
            pflag = 1;
        } else {
            pflag = ctrlCodeFlags & 1;
            q++;
            s = q;
        }
        *d++ = c;
        if (c >= 0xA0) {
            prop = xglFontCheckProportional(c & 0xFF, trail);
            *d++ = trail;
            if (pflag && prop >= 0) {
                sz = xglFontGetProportionalSize(prop);
                count = count + (((sz >> 8) & 0xFF) - (sz & 0xFF)) + 1;
            } else {
                count += 0x14;
            }
        } else {
            count += 8;
        }
    next:;
    }
    *d = 0;
    *src = s;
    *dst = d;
    return count;
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct MsgQueue {
    unsigned char *buffer;
    unsigned char unmodeled_04[4];
    short head;
    short tail;
    short size;
    short count;
    short width;
    short last;
    short limit;
    unsigned char unmodeled_22[6];
    int label;
} MsgQueue;

struct MsgPrintContext {
    unsigned char unmodeled_00[0x16];
    unsigned short render_mode;
    unsigned char unmodeled_18[0x94];
    MsgQueue queue;
};

typedef struct RubyFontContext {
    unsigned char unmodeled_00[8];
    short x;
    short y;
    unsigned char unmodeled_0c[12];
    unsigned char flags;
} RubyFontContext;

int MSG_queuePush(struct MsgQueue *queue, unsigned char *text, int capacity, int length)
{
    int newTail;
    int tail;
    unsigned char *currentOutput;
    unsigned char *output;
    unsigned char *input;
    unsigned char *end;

    if (length < 0) {
        length = MSG_getSize(text);
    }
    tail = queue->tail;
    newTail = tail + length;
    output = queue->buffer + tail;
    input = (unsigned char *)text;
    end = output + length;
    if (newTail >= queue->size) {
        return -1;
    }
    queue->tail = newTail;
    currentOutput = output;
    if (currentOutput < end) {
        do {
            int code;

            code = *input;
            input++;
            if (code == '/') {
                int bracketCode;

                *currentOutput = '/';
                output++;
                bracketCode = *input;
                if (bracketCode == '[') {
                    *output++ = bracketCode;
                    input++;
                    bracketCode = *input++;
                    if (bracketCode >= 0x80) {
                        *output++ = bracketCode;
                        switch (bracketCode) {
                        case ']':
                        case 0x81:
                        case 0x84:
                            break;
                        case 0x85: {
                            int payloadCode;

                            do {
                                *output = *input;
                                currentOutput = output + 1;
                                payloadCode = *output;
                                output = currentOutput;
                                input++;
                            } while (payloadCode != 0);
                            break;
                        }
                        case 0x80:
                        case 0x82:
                        case 0x83:
                            *output++ = *input++;
                            break;
                        default:
                            break;
                        }
                        *output++ = *input++;
                    } else {
                        input--;
                        for (;;) {
                            bracketCode = *input++;
                            if (bracketCode < 0x20) {
                                input--;
                                _copyCTRLCode(&output, &input);
                            } else if (bracketCode == ']') {
                                currentOutput = output + 1;
                                *output = ']';
                                output = currentOutput;
                                break;
                            } else {
                                *output++ = bracketCode;
                                if (bracketCode >= 0x8e) {
                                    *output++ = *input++;
                                }
                            }
                        }
                    }
                }
            } else if (code == '\n') {
                *currentOutput++ = '\n';
                output = currentOutput;
            } else if (code < 0x20) {
                input--;
                _copyCTRLCode(&output, &input);
            } else {
                *currentOutput++ = code;
                output = currentOutput;
                if (code >= 0xa0) {
                    *currentOutput++ = *input++;
                    output = currentOutput;
                }
            }
            {
                unsigned char *completedOutput = output;

                currentOutput = completedOutput;
                if (completedOutput >= end) {
                    break;
                }
            }
        } while (1);
    }
    return newTail;
}

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

char *MIF2_ruby(char *out, int argc, const Mif2Argument *args)
{
    const char *text = args[0].value.text;
    int callback = (int)rubyFont;
    int ch;
    int width;

    *out++ = 0x17;
    *out++ = callback >> 16;
    *out++ = callback >> 8;
    *out++ = callback;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    while ((ch = (unsigned char)*text++) != '/' && ch != 0) {
        *out++ = ch;
    }
    *out++ = '/';
    if (ch == 0) {
        text--;
    }
    *out++ = '[';
    {
        int ruby_width;
        const char *cursor;
        const unsigned char character_map[96] = {
            0x00, 0x07, 0x11, 0x08, 0x12, 0x09, 0x13, 0x0a, 0x14, 0x0b, 0x15, 0x16, 0x56, 0x17, 0x57, 0x18,
            0x58, 0x19, 0x59, 0x1a, 0x5a, 0x1b, 0x5b, 0x1c, 0x5c, 0x1d, 0x5d, 0x1e, 0x5e, 0x1f, 0x5f, 0x20,
            0x60, 0x21, 0x61, 0x0f, 0x22, 0x62, 0x23, 0x63, 0x24, 0x64, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
            0x6a, 0xea, 0x2b, 0x6b, 0xeb, 0x2c, 0x6c, 0xec, 0x2d, 0x6d, 0xed, 0x2e, 0x6e, 0xee, 0x2f, 0x30,
            0x31, 0x32, 0x33, 0x0c, 0x34, 0x0d, 0x35, 0x0e, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x00, 0x3c,
            0x00, 0x00, 0x06, 0x3d, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        for (ruby_width = 0, cursor = text; (ch = (unsigned char)*cursor++) != 0; ) {
            ruby_width++;
            if (ch >= 0xa0) {
                ch = (unsigned char)*cursor++;
                if (ch >= 0xa3 && ch <= 0xa5) {
                    if (character_map[ch - 0xa0] & 0x40) {
                        ruby_width++;
                    }
                }
            }
        }
        width = ruby_width + 0x10000000;
    }
    *out++ = 0x17;
    *out++ = callback >> 16;
    *out++ = callback >> 8;
    *out++ = callback;
    *out++ = width >> 24;
    *out++ = width >> 16;
    *out++ = width >> 8;
    *out++ = width;
    *out++ = 0x18;
    *out++ = 1;
    while ((ch = (unsigned char)*text++) != '/' && ch != 0) {
        *out++ = ch;
    }
    *out++ = 0x18;
    *out++ = 0;
    *out++ = 0x17;
    *out++ = callback >> 16;
    *out++ = callback >> 8;
    *out++ = callback;
    *out++ = 0x20;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = ']';
    *out = 0;
    return out;
}

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

unsigned int MSG_convert(unsigned char *destination, int capacity, const char *text, int length);
int MSG_queuePush(struct MsgQueue *q, unsigned char *msg, int size, int n);
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

extern int cnvCH;
extern int cnvCW;
static void _copyCTRLCode(unsigned char **dstp, unsigned char **srcp);
extern char D_004DA418[];

unsigned int MSG_convert(unsigned char *destination, int capacity, const char *text, int length)
{
    char bracket[256];
    const char *tokens[8];
    Mif2Argument args[8];
    unsigned char *output;
    unsigned char *input;
    unsigned char *end;
    int width = 0;

    cnvCH = 0;
    cnvCW = 0;
    input = (unsigned char *)text;
    if (length < 0)
        length = strlen(text);
    end = input + length;
    output = destination;
    while (input < end) {
        int code = *input++;
        if (code == '/') {
            if (*input == '[') {
                char *closing = matchBracket(input);
                if (closing) {
                    char *token;
                    int span;
                    input++;
                    span = (unsigned char *)closing - input;
                    strncpy(bracket, (const char *)input, span);
                    input = (unsigned char *)closing + 1;
                    bracket[span] = 0;
                    token = STR_tokenGetNext(bracket, D_004DA418);
                    while (token) {
                        int argc = 0;
                        MsgFuncEntry *entry = findMsgFunc(token);
                        if (entry) {
                            int type;
                            token = STR_tokenGetNext(0, D_004DA418);
                            type = STR_tokenGetType();
                            if (token) {
                                tokens[argc++] = token;
                                if (type != ')') {
                                    do {
                                        token = STR_tokenGetNext(0, D_004DA418);
                                        type = STR_tokenGetType();
                                        if (!token)
                                            break;
                                        tokens[argc++] = token;
                                    } while (type != ')');
                                }
                            }
                            MSG_setArgs(args, argc, tokens);
                            {
                                char *(*handler)(char *, int, const Mif2Argument *) = entry->handler;
                                unsigned int address = (unsigned int)handler;
                                if (address - 0x1F0000U <= 0x1E0FFFFU && !(address & 3))
                                    output = (unsigned char *)handler((char *)output, argc, args);
                            }
                            if (!token)
                                break;
                        } else {
                            do {
                                *output = *token++;
                            } while (*output++);
                            output--;
                        }
                        token = STR_tokenGetNext(0, D_004DA418);
                    }
                }
            } else {
                *output++ = code;
            }
        } else if (code == '\n') {
            if (cnvCW < width)
                cnvCW = width;
            *output++ = code;
        } else if (code < 32) {
            input--;
            _copyCTRLCode(&output, &input);
        } else {
            *output++ = code;
            if (code >= 142)
                *output++ = *input++;
            width++;
        }
    }
    *output = 0;
    return cnvCH;
}

/*
 * One record of the 8-slot mbuf pool msg_buffer reserves (2064 bytes each,
 * 8 * 2064 = msg_buffer's own 0x4080 size). MBUF_dispose (below) evidences
 * the leading word as a plain int, the same field MBUF_init clears here for
 * every slot.
 */
typedef struct MsgBuffer {
    int count;
    int size;
    union {
        int unmodeled_08; /* former padding name, kept for saved bodies */
        unsigned char **entry_table;
    };
    void *data;
    unsigned char unmodeled_10[0]; /* +0x10: zero-length alias of the former padding member, kept for saved bodies */
    unsigned char workspace[0x800];
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
    m->data = m->workspace;
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
    buf = (char *)m->workspace;
    m->count = 1;
    m->size = 0;
    m->entry_table = (int)buf;
    m->data = buf + n * 4;
    for (i = 0; i < n; i++) {
        *(char *)m->data = 0;
        ((void **)m->entry_table)[i] = m->data;
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

static void _copyCTRLCode(unsigned char **dstp, unsigned char **srcp)
{
    unsigned char *s = *srcp;
    unsigned char *d = *dstp;
    unsigned int c;

    c = *s++;
    *d++ = c;
    switch (c) {
    case 6: case 0xc: case 0xf:
    copy3:
        *d++ = *s++;
    case 0x12:
        *d++ = *s++;
        goto copy1;
    case 8:
        c = *s++;
        if (c & 1) {
            *d++ = *s++;
            *d++ = *s++;
        }
        if (c & 2) {
            *d++ = *s++;
            *d++ = *s++;
        }
        if (c & 4) {
            goto copy3;
        }
        break;
    case 0xe:
    copy5:
        *d++ = *s++;
        *d++ = *s++;
        goto copy3;
    case 0x17:
        *d++ = *s++;
        *d++ = *s++;
        goto copy5;
    case 0x19:
        if (*s) {
            ctrlCodeFlags |= 1;
        } else {
            ctrlCodeFlags &= ~1;
        }
    case 1: case 2: case 3: case 4: case 5: case 0xd: case 0x18:
    copy1:
        *d++ = *s++;
    }
    *dstp = d;
    *srcp = s;
}

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
