#include "common.h"

/* Number of message sprites currently allocated by the print routines. */
extern int msg_spr_count;

void eMessageSpriteReset(void)
{
    msg_spr_count = 0;
}

void eMessageDrawType00(void)
{
}

int eMessageHalfSpaseCheck(unsigned char *text)
{
    unsigned char character = *text++;
    int half_spaces = 0;

    while ((signed char)character == ' ') {
        character = *text++;
        half_spaces++;
    }
    return half_spaces;
}

extern int xglFontGetSPcodeSize(int code, unsigned char *text);

char *eMessageNextGyou(char *text)
{
    int n;
    int z = 0;

    for (;;) {
        char s;
        unsigned int c;

        s = text[z];
        c = ((unsigned char *)text)[z];
        if (s == 0 || s == '\n') {
            break;
        }
        if (c - 1 < 0x19) {
            text += xglFontGetSPcodeSize(((unsigned char *)text)[z], (unsigned char *)text) + 1;
        } else if ((char)c == 0x1E) {
            text += 2;
        } else if ((char)c == 0x1F || c - 0x20 < 0x5F) {
            text++;
        } else {
            n = eMessageHalfSpaseCheck((unsigned char *)text);
            text = n ? text + n : text + 2;
        }
    }
    return text;
}

char *eMessageNextGyou(char *text);

int eMessageNextGyouMaxGet(char *text)
{
    int lines = 0;
    char c;

    for (;;) {
        lines++;
        text = eMessageNextGyou(text);
        c = *text;
        if (c == 0) {
            break;
        }
        if (c == 0x1F) {
            break;
        }
        if (c == '\n') {
            text++;
        }
    }
    return lines;
}

char *eMessageNextWaitKeySearch(char *text)
{
    int n;
    int z = 0;
    int c;
    char s;
    int t;

    s = *text;
    c = (unsigned char)*text;
    goto check;
body:
    if ((unsigned int)(c - 1) < 0x19U) {
        text += xglFontGetSPcodeSize(((unsigned char *)text)[z], (unsigned char *)text) + 1;
    } else if ((char)c == 0x1E) {
        text += 2;
    } else if ((unsigned int)(c - 0x20) < 0x5FU) {
        text++;
    } else {
        n = eMessageHalfSpaseCheck((unsigned char *)text);
        text = n ? text + n : text + 2;
    }
    t = ((unsigned char *)text)[z];
    c = t;
    s = (char)t;
check:
    if (s == 0) {
        goto out;
    }
    if (s != 0x1F) {
        goto body;
    }
    text++;
out:
    return text;
}

typedef struct EMessageUp {
    unsigned char flags;
    unsigned char mode;
    unsigned char visible;
    unsigned char wrap_width_half;
    short x;
    short y;
    int draw_parameter;
    short text_x_offset;
    short text_y_offset;
    unsigned char color[4];
    unsigned short text_timer;
    unsigned short frame;
    char *text;
    unsigned char page_available;
    unsigned char page_index;
    unsigned char page_count;
    unsigned char unmodeled_1f;
    unsigned char cursol_state;
    unsigned char cursol_active;
    unsigned char cursol_timer;
    unsigned char cursol_width;
    short cursol_x;
    short cursol_y;
    int cursol_work;
} EMessageUp;

typedef struct EMessageSprite {
    short x;
    short y;
    int work;
    short w;
    short h;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    short kind;
    unsigned char blink;
    unsigned char trans;
} EMessageSprite;
typedef struct EMessageRgba {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} EMessageRgba;
typedef struct EMessageIconKinds {
    short kind[4];
} EMessageIconKinds;
typedef struct EMessageIconCode {
    char code[3];
} EMessageIconCode;
extern unsigned char moji[];
extern unsigned char otb[];
extern EMessageSprite msg_spr[];
extern short D_004DA920[];
extern char D_004DA928[];
extern int xglFontGetStringWidth(const char *text);
extern void xglFontPrint(int x, int y, int color, const char *text);
extern void endSpriteSet(void *sprite, int flags);
extern void endPrintExtFunc(int flags, int id, void *param);
extern void eCursolModeChange(unsigned char *mode, unsigned int next);
extern void eCursolMain(void *cursol);
extern char *eMessageNextWaitKeySearch(char *text);

int eMessageDrawType01(EMessageUp *message)
{
    unsigned char *mp = moji;
    int flags = message->flags;
    EMessageRgba col = { message->color[0], message->color[1], message->color[2], message->color[3] };
    int done = 0;
    int ctl = 3;
    int wrap = message->wrap_width_half << 1;
    unsigned char wrap_hi = wrap / 256;
    unsigned char wrap_lo = wrap % 256;
    int xw = 0;
    unsigned char *text = (unsigned char *)message->text;
    int yh = 0;
    unsigned char *strbuf[32];
    char buf[16];
    int cursor = 0;
    short count;
    unsigned char **it;
    unsigned char *p;
    unsigned char *last;
    int width;
    int i;
    int n;
    unsigned int c;
    signed char s;
    int t;
    int t2;
    unsigned int c1;
    signed char s1;
    int t3;
    signed char s3;
    unsigned char v;
    int flag20;
    int z = 0;

    ((union { unsigned char *p; unsigned char c; } *)strbuf)->p = 0;
    count = message->frame;
    if (message->flags & 1) {
        *mp++ = otb[0];
        *mp++ = otb[0x15];
        *mp++ = otb[0x16];
        *mp++ = otb[0x17];
        *mp++ = otb[0x18];
        *mp++ = otb[1];
        *mp++ = (int)(otb[2] * (message->color[0] / 128.0f));
        *mp++ = (int)(otb[3] * (message->color[1] / 128.0f));
        *mp++ = (int)(otb[4] * (message->color[2] / 128.0f));
        *mp++ = otb[5];
        *mp++ = otb[6];
        *mp++ = otb[7];
        *mp++ = otb[8];
        *mp++ = otb[9];
        *mp++ = otb[0xA];
        *mp++ = otb[0xB];
        *mp++ = otb[0xC];
        *mp++ = otb[0xD];
        *mp++ = otb[0xE];
        *mp++ = otb[0xF];
        *mp++ = (int)(otb[0x10] * (message->color[3] / 128.0f));
        *mp++ = otb[0x11];
        *mp++ = otb[0x12];
        *mp++ = otb[0x13];
        *mp++ = otb[0x14];
        *mp++ = 0x15;
        *mp++ = 2;
        *mp++ = 2;
        *mp++ = 4;
        *mp++ = 0x15;
        *mp++ = ctl;
        *mp++ = ctl;
        *mp++ = wrap_hi;
        *mp++ = wrap_lo;
    }

    for (i = 0; i < message->page_index; i++) {
        text = (unsigned char *)eMessageNextWaitKeySearch((char *)text);
    }

    it = strbuf;
    p = text;
    width = 0;
    last = text;
    s1 = *text;
    c = *text;
    if (s1 != 0) {
        c1 = c;
        do {
        if (c1 - 1 < 0x19) {
            if ((signed char)c1 == '\n') {
                width = 0;
            }
            p += xglFontGetSPcodeSize(*p, p) + 1;
        } else if ((signed char)c1 == 0x1E) {
            p += 2;
        } else if ((signed char)c1 == 0x1F) {
            goto measured;
        } else if ((signed char)c1 == ' ') {
            width += 4;
            p++;
            if (wrap < width) {
                *it++ = last;
                p = last;
                width = 0;
                if (it > &strbuf[31]) {
                    goto measured;
                }
            } else {
                last = p;
            }
        } else {
            int next;

            buf[0] = 0x19;
            buf[1] = 3;
            buf[2] = p[0];
            next = p[1];
            if ((unsigned int)(next - 1) < 0x20 || (unsigned int)(p[0] - 0x20) < 0x60) {
                buf[3] = 0;
                p++;
            } else {
                buf[3] = next;
                p++;
                if (*(signed char *)p != 0) {
                    p++;
                }
            }
            buf[4] = 0;
            width += xglFontGetStringWidth(buf);
        }
        t = *p;
        c1 = t;
        } while (t != 0);
        c = *text;
        goto draw;
    }
    goto draw;
measured:
    c = *text;
draw:
    it = strbuf;
    if (c != 0) {
        flag20 = flags & 0x20;
        do {
            if (it <= &strbuf[31] && text == *it) {
                it++;
                xw = 0;
                yh += 0x18;
            }
            if (c - 1 < 0x19) {
                s = c;
                n = xglFontGetSPcodeSize(text[z], text) + 1;
                switch (s) {
                case 0xC:
                    *mp++ = *text++;
                    mp[0] = (int)(*text++ * (message->color[0] / 128.0f));
                    mp[1] = (int)(*text++ * (message->color[1] / 128.0f));
                    mp[2] = (int)(*text++ * (message->color[2] / 128.0f));
                    col.r = *mp++;
                    col.g = *mp++;
                    col.b = *mp++;
                    break;
                case 0xF:
                    *mp++ = *text++;
                    *mp++ = *text++;
                    *mp++ = *text++;
                    *mp = (int)(*text++ * (message->color[3] / 128.0f));
                    col.a = *mp++;
                    break;
                case '\n':
                    if (flag20) {
                        count = message->frame;
                    }
                    *mp++ = *text++;
                    xw = 0;
                    yh += 0x18;
                    break;
                case 0x19:
                    *mp++ = *text++;
                    c = text[z];
                    t3 = c;
                    c = (signed char)t3;
                    if (c == 0 || c == 2) {
                        ctl = 0;
                    } else {
                        ctl = 3;
                    }
                    *mp++ = t3;
                    text++;
                    break;
                default:
                    while (n--) {
                        *mp++ = *text++;
                    }
                    break;
                }
            } else if ((signed char)c == 0x1E) {
                text++;
                if (msg_spr_count < 16) {
                    EMessageIconKinds kinds = *(EMessageIconKinds *)D_004DA920;
                    EMessageIconCode code = *(EMessageIconCode *)D_004DA928;
                    EMessageSprite *spr = &msg_spr[msg_spr_count];
                    signed char *k = (signed char *)text;

                    if (*k < 4) {
                        spr->kind = kinds.kind[*k & 3];
                    } else if (*k >= 4 && *k < 0x1B) {
                        spr->kind = *k + 0x14FC;
                    } else if (*k >= 0x1B && *k < 0x1F) {
                        spr->kind = *k + 0x15E5;
                    } else if (*k >= 0x1F && *k < 0x23) {
                        spr->kind = *k + 0x15E5;
                    } else if (*k >= 0x23 && *k < 0x3A) {
                        spr->kind = *k + 0x7DD;
                    } else if (*k >= 0x3A && *k < 0x4E) {
                        spr->kind = *k + 0x16C6;
                    } else {
                        spr->kind = 0;
                    }
                    endSpriteSet(spr, 0xFF);
                    spr->x = message->x + (message->text_x_offset + xw) - spr->w / 2 + 10;
                    spr->work = message->draw_parameter;
                    spr->y = message->y + (message->text_y_offset + yh) - spr->h / 2 + 12;
                    if ((signed char)*text < 4) {
                        if (col.r != 0x80 || col.g != 0x80 || col.b != 0x80) {
                            spr->trans = 1;
                        }
                        spr->a = col.a;
                    } else {
                        spr->r = col.r;
                        spr->g = col.g;
                        spr->b = col.b;
                        spr->a = col.a;
                    }
                    xw += 0x14;
                    endPrintExtFunc(message->draw_parameter, 2, spr);
                    msg_spr_count++;
                    *mp++ = 0x19;
                    *mp++ = 0;
                    *mp++ = code.code[0];
                    *mp++ = code.code[1];
                    *mp++ = 0x19;
                    *mp++ = ctl;
                }
                text++;
            } else if ((signed char)c == 0x1F) {
                cursor = 1;
                break;
            } else {
                int next;

                buf[0] = 0x19;
                buf[1] = ctl;
                buf[2] = text[0];
                next = text[1];
                if ((unsigned int)(next - 1) < 0x1F || (unsigned int)(text[0] - 0x20) < 0x60) {
                    buf[3] = 0;
                    *mp++ = *text++;
                } else {
                    buf[3] = next;
                    *mp++ = *text++;
                    if (*(signed char *)text != 0) {
                        *mp++ = *text++;
                    }
                }
                buf[4] = 0;
                xw += xglFontGetStringWidth(buf);
                if (flags & 0x10) {
                    if (count == 0) {
                        done = 1;
                        if (flag20 == 0) {
                            break;
                        }
                        text = (unsigned char *)eMessageNextGyou((char *)text);
                        count = message->frame;
                    } else {
                        count = count - 1;
                    }
                }
            }
            t2 = *text;
            c = t2;
        } while (t2 != 0);
    }
    *mp = 0;
    if (!done) {
        message->flags |= 0x80;
        if (message->page_available == 1) {
            cursor = 1;
        }
    }
    xglFontPrint(message->x + message->text_x_offset, message->y + message->text_y_offset - 1,
                 message->draw_parameter, moji);
    if (message->page_available) {
        if (cursor) {
            message->cursol_x = message->x + message->text_x_offset + xw + 2;
            message->cursol_y = message->y + message->text_y_offset + yh + 6;
            message->cursol_work = message->draw_parameter;
            eCursolModeChange(&message->cursol_state, 0x20);
            eCursolMain(&message->cursol_state);
        } else {
            message->cursol_state = 0;
        }
    }
}

/*
 * The message object eMessageModeChange, eMessageDraw and eMessageMain
 * receive.  Callers build it on their stack and it extends well past the
 * mode byte (eMessageMain also reads +0x00 and +0x18, TextTest fills +0x04
 * to +0x1C); only the mode byte eMessageModeChange writes is modelled.
 * eMessageMain dispatches on it with lbu, so it is unsigned.
 */
typedef struct EMessageParam {
    unsigned char unmodeled_00;
    unsigned char mode;
} EMessageParam;

extern int eMessageDrawType01(EMessageUp *);

int eMessageMain(EMessageParam *in)
{
    EMessageUp *message = (EMessageUp *)in;

    switch (message->mode) {
    case 0:
        message->visible = 0;
        message->mode = 1;
        /* fall through */
    case 1:
        message->mode = 240;
        break;
    case 32:
        message->visible = 1;
        message->mode = 33;
        message->flags &= 15;
        /* fall through */
    case 33:
        message->mode = 80;
        break;
    case 34:
        message->visible = 1;
        message->mode = 35;
        message->text_timer = 0;
        message->flags = (message->flags & 15) | 16;
        message->frame = 0;
        /* fall through */
    case 35:
        if (message->flags & 128) {
            message->mode = 80;
        } else {
            message->frame++;
            message->text_timer++;
        }
        break;
    case 36:
        message->visible = 1;
        message->mode = 37;
        message->text_timer = 0;
        message->flags = (message->flags & 15) | 48;
        message->frame = 0;
        /* fall through */
    case 37:
        if (message->flags & 128) {
            message->mode = 80;
        } else {
            message->frame++;
            message->text_timer++;
        }
        break;
    case 80:
        message->mode = 81;
        break;
    case 81:
        break;
    case 112:
        message->visible = 0;
        message->mode = 113;
        /* fall through */
    case 113:
        message->mode = 240;
        break;
    case 240:
        message->visible = 0;
        message->mode = 241;
        break;
    case 241:
        break;
    }
    if (message->text != 0 && message->visible != 0) {
        eMessageDrawType01(message);
    }
}

extern int eMessageMain(EMessageParam *message);

void eMessageModeChange(EMessageParam *message, unsigned char mode)
{
    message->mode = mode;
}

extern void eCursolSet(void *cursol, signed char width);

void eMessageSet(EMessageParam *message, char *text)
{
    void *cursol = (unsigned char *)message + 0x20;

    *(char **)((unsigned char *)message + 0x18) = text;
    *(short *)((unsigned char *)message + 0x04) = 0;
    *(short *)((unsigned char *)message + 0x06) = 0;
    *(int *)((unsigned char *)message + 0x08) = 0;
    ((unsigned char *)message)[0x10] = 0x80;
    ((unsigned char *)message)[0x11] = 0x80;
    ((unsigned char *)message)[0x12] = 0x80;
    ((unsigned char *)message)[0x13] = 0x80;
    ((unsigned char *)message)[0x00] = 1;
    ((unsigned char *)message)[0x03] = 0xF0;
    ((unsigned char *)message)[0x01] = 0;
    ((unsigned char *)message)[0x02] = 0;
    *(short *)((unsigned char *)message + 0x14) = 0;
    *(short *)((unsigned char *)message + 0x16) = 0;
    *(short *)((unsigned char *)message + 0x0C) = 0;
    *(short *)((unsigned char *)message + 0x0E) = 0;
    ((unsigned char *)message)[0x1D] = 0;
    ((unsigned char *)message)[0x1C] = 0;
    eCursolSet(cursol, 3);
}

extern char *eMessageNextWaitKeySearch(char *text);

void eMessageTextChange(EMessageParam *message, char *text)
{
    char *p;

    *(char **)((char *)message + 0x18) = text;
    if (*((unsigned char *)message + 0x1C) != 0) {
        *((unsigned char *)message + 0x1D) = 0;
        *((unsigned char *)message + 0x1E) = 0;
        p = text;
        while (*(p = eMessageNextWaitKeySearch(p)) != 0) {
            (*((unsigned char *)message + 0x1E))++;
        }
    }
}

typedef struct EMessagePageState {
    unsigned char flags;
    unsigned char mode;
    unsigned char unmodeled_02[26];
    unsigned char page_available;
    unsigned char page_index;
    unsigned char page_count;
} EMessagePageState;

int eMessageNextPage(EMessagePageState *message, int reset_page)
{
    int next_page;
    unsigned char wrapped_page;
    unsigned char next_mode;

    if (message->page_available != 0) {
        if (reset_page == 0) {
            if ((message->flags & 0x80) != 0) {
                next_page = message->page_index + 1;
                wrapped_page = next_page;
                message->page_index = next_page;
                if (message->page_count < wrapped_page) {
                    message->page_index = message->page_count;
                    return 0;
                }
                next_mode = 34;
            } else {
                next_mode = 32;
            }
            message->mode = next_mode;
            return 1;
        } else {
            message->page_index = 0;
        }
    }
    return 0;
}

void eMessageDraw(EMessageParam *message)
{
    eMessageMain(message);
}

/* Current write position of the message text being built. */
extern char *MessageCpyEnd;

extern void eMessageCat(char *src);

void eMessageCpy(char *dst, char *src)
{
    MessageCpyEnd = dst;
    eMessageCat(src);
}

void eMessageCat(char *src)
{
  unsigned long new_var;
  char *dst = MessageCpyEnd;
  int n;
  int i;
  int z = 0;
  if (src == 0)
  {
    return;
  }
  for (;;)
  {
    char s = src[z];
    unsigned char c = ((unsigned char *) src)[z];
    if (s == 0)
    {
      break;
    }
    if (((unsigned int) (c - 1)) < 0x19)
    {
      n = xglFontGetSPcodeSize(((unsigned char *) src)[z], (unsigned char *) src) + 1;
      for (i = 0; i < n; i++)
      {
        *(dst++) = *(src++);
      }

    }
    else
    {
      c = ((unsigned char *) src)[z];
      if (src[z] == 0x1E)
      {
        *(dst++) = c;
        src++;
        *(dst++) = *(src++);
      }
      else
        if ((src[z] == 0x1F) || (((unsigned char) (new_var = c - 0x20)) < 0x5F))
      {
        *(dst++) = c;
        src++;
      }
      else
      {
        n = eMessageHalfSpaseCheck((unsigned char *) src);
        if (n != 0)
        {
          for (i = 0; i < n; i++)
          {
            *(dst++) = *(src++);
          }

        }
        else
        {
          *(dst++) = *(src++);
          *(dst++) = *(src++);
        }
      }
    }
  }

  *dst = 0;
  MessageCpyEnd = dst;
}
