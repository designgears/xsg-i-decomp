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

INCLUDE_ASM("asm/main/nonmatchings/e_message", eMessageNextGyouMaxGet);

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

INCLUDE_ASM("asm/main/nonmatchings/e_message", eMessageDrawType01);

INCLUDE_ASM("asm/main/nonmatchings/e_message", eMessageMain);

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

extern void eMessageMain(EMessageParam *message);

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

INCLUDE_ASM("asm/main/nonmatchings/e_message", eMessageCat);
