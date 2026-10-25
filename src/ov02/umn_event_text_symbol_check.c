/*
 * OV02 original TU 14: 0x00a0e938..0x00a0f674 (20 functions)
 */
#include "common.h"
#include "shared.h"
#include "umn_event_text_symbol_check.h"

typedef struct {
    unsigned char *str;
    short len;
    short code;
} UmnEventTextSymEntry;
typedef struct { UmnEventTextSymEntry e[7]; } UmnEventTextSymTbl;
typedef struct { unsigned char raw[56]; } UmnEventTextSymRaw;
extern UmnEventTextSymTbl D_00A13588;

int UmnEventTextSymbolCheck(unsigned char **str)
{
  if ((*(*str)) == '<')
  {
    int i = 0;
    UmnEventTextSymRaw rawtbl;
    UmnEventTextSymEntry *ent;
    unsigned char *a;
    int j;
    rawtbl = *(UmnEventTextSymRaw *)&D_00A13588;
    *str = (int) ((*str) + 1);
    a = *str;
    ent = (UmnEventTextSymEntry *)rawtbl.raw;
    for (; i < 7; i++)
    {
      unsigned char *b = ent[i].str;
      a = *str;
      for (j = 0; j < ent[i].len; j++)
      {
        if (a[j] != b[j])
        {
          break;
        }
      }

      if (j == ent[i].len)
      {
        *str = (*str) + (j + 1);
        return ent[i].code;
      }
    }

  }
  return 0;
}

typedef struct {
    unsigned char *str;
    short len;
    short code;
} UmnEventTextCharEntry;
typedef struct { UmnEventTextCharEntry e[2]; } UmnEventTextCharTbl;
extern UmnEventTextCharTbl D_00A135D0;

int UmnEventTextCharCheck(unsigned char **str)
{
    int i = 0;
    UmnEventTextCharTbl tbl = D_00A135D0;
    unsigned char *a = *str;
    int j;

    for (; i < 2; i++) {
        unsigned char *b = tbl.e[i].str;
        for (j = 0; j < tbl.e[i].len; j++) {
            if (a[j] != b[j]) {
                break;
            }
        }
        if (j == tbl.e[i].len) {
            *str += j;
            return tbl.e[i].code;
        }
    }
    return 0;
}

int UmnEventTextNumberGet(unsigned char **str, int mode)
{
  unsigned char value = 0;
  unsigned char max;
  unsigned char c;
  char base;
  unsigned int ci;
  if (mode == 1)
  {
    max = '9';
    base = 10;
  }
  else
  {
    max = '7';
    base = 8;
  }
  while (((*(*str)) >= '0') && (max >= (*(*str))))
  {
    value *= base;
    value += (*((*str)++)) - '0';
  }

  return value;
}

/* Advances *str past every consecutive newline. */
void UmnEventTextGyouJump(unsigned char **str)
{
    unsigned char *cursor = *str;
    unsigned char *next;

    if (*cursor == '\n') {
        do {
            next = cursor + 1;
            *str = next;
            cursor = next;
        } while (*cursor == '\n');
    }
}

void UmnEventTextMessageMake(unsigned char **str, unsigned char **out)
{
    int c;
    while (**str != '<') {
        if (**str == '\\') {
            (*str)++;
            c = UmnEventTextNumberGet(str, 0);
            if (c == 0x1F) {
                UmnEventTextGyouJump(str);
            }
            *(*out)++ = c;
        } else {
            *(*out)++ = *(*str)++;
        }
    }
    *(*out)++ = 0;
    *str += 6;
}

extern int xglCdGetFileSize(const char *name);
extern char *eMessageNextWaitKeySearch(char *text);
extern char D_00A135E0[];
extern int UmnEventTextMakeWork;
extern unsigned int uet_file_end;
typedef struct UmnEventTextRec {
    short code;
    unsigned char wait;
    unsigned char num;
    unsigned char *msg;
} UmnEventTextRec;

int UmnEventTextMake(int arg0)
{
    char path[21];
    unsigned int buf;
    unsigned int sp20;
    unsigned int sp24;
    unsigned char *sp28;
    unsigned char *msg;
    unsigned char *q;
    char *tb;
    int size;
    int count;
    int idx;
    int t;
    UmnEventTextRec *rec;

    __builtin_memcpy(path, D_00A135E0, 21);
    path[14] = (arg0 / 10) + '0';
    path[15] = (arg0 % 10) + '0';
    buf = (UmnEventTextMakeWork + 0xF) & ~0xF;
    size = xglCdGetFileSize(path);
    if (size < 0) {
        return -1;
    }
    xglCdReadFile(path, (void *)buf, 0, 1);
    sp24 = buf;
    sp28 = (unsigned char *)uet_text_buf;
    tb = uet_text_buf;
    uet_file_end = buf + size;
    count = 0;
    sp20 = buf;
    if (buf < uet_file_end) {
        do {
            switch (UmnEventTextSymbolCheck((unsigned char **)&sp20)) {
            case 6:
                do {
                    sp20 += 1;
                } while (UmnEventTextSymbolCheck((unsigned char **)&sp20) != 7);
                break;
            case 1:
                count = count + 1;
                break;
            }
            sp20 = sp20 + 1;
        } while (sp20 < uet_file_end);
    }
    q = sp28 + 4;
    ((UmnEventTextRecordBuf *)sp28)->cur = 0;
    ((UmnEventTextRecordBuf *)sp28)->count = count;
    sp28 = q + count * 8;
    idx = 0;
    UmnEventTextGyouJump((unsigned char **)&sp24);
    while (sp24 < uet_file_end) {
        rec = (UmnEventTextRec *)(tb + idx * 8 + 4);
        t = UmnEventTextSymbolCheck((unsigned char **)&sp24);
        switch (t) {
        case 6:
            do {
                sp24 += 1;
            } while (UmnEventTextSymbolCheck((unsigned char **)&sp24) != 7);
            break;
        case 0:
            return -1;
        case 1:
            UmnEventTextNumberGet((unsigned char **)&sp24, 1);
            rec->wait = 0;
            rec->num = 0;
            rec->code = 0;
            rec->msg = 0;
            break;
        case 2:
            rec->code = (short)UmnEventTextCharCheck((unsigned char **)&sp24);
            break;
        case 4:
            rec->num = UmnEventTextNumberGet((unsigned char **)&sp24, 1);
            break;
        case 3:
            msg = sp28;
            rec->msg = msg;
            UmnEventTextGyouJump((unsigned char **)&sp24);
            UmnEventTextMessageMake((unsigned char **)&sp24, (unsigned char **)&sp28);
            while (msg[0] != 0) {
                msg = (unsigned char *)eMessageNextWaitKeySearch((char *)msg);
                rec->wait++;
            }
            break;
        case 5:
            idx += 1;
            break;
        }
        UmnEventTextGyouJump((unsigned char **)&sp24);
    }
    uet_flag = 1;
    return 0;
}

/*
 * uet_text_buf holds a signed record count followed by 8-byte records;
 * advances the cursor by delta and returns the record at the new position,
 * or NULL once it runs past count.
 */
void *UmnEventTextNextGet(int delta)
{
    UmnEventTextRecordBuf *buf = (UmnEventTextRecordBuf *) uet_text_buf;
    void *result;
    int wide_cur;
    short cur;

    wide_cur = buf->cur + delta;
    buf->cur = wide_cur;
    cur = wide_cur;
    result = 0;
    if (cur >= buf->count) {
        return result;
    }
    result = (char *) buf + cur * 8 + 4;
    return result;
}

/*
 * event_tbl lists the mail_id values UmnEventEndMailCheck's mailbox check
 * applies to; bit 2 of the mail's data byte marks it already handled.
 */
int UmnMailEventCheck(int mail_id)
{
    unsigned char *data;
    int i;

    for (i = 0; i < 18; i++) {
        if (event_tbl[i] == mail_id) {
            data = UmnMailDataGet(mail_id);
            if (*data & 4) {
                break;
            }
            return i + 1;
        }
    }
    return 0;
}

/*
 * Reading trigger_mail_id unlocks follow_mail_id: its data byte's bit 0 is
 * set once and the mail is registered into the mailbox.
 */
int UmnEventEndMailCheck(int mail_id)
{
    unsigned char *data;

    if (event_end_mail.trigger_mail_id == mail_id) {
        signed char *follow_ptr = &event_end_mail.follow_mail_id;

        data = UmnMailDataGet(*follow_ptr);
        if (!(*data & 1)) {
            *data |= 1;
            UmnMailBoxSet(*follow_ptr);
            return 1;
        }
    }
    return 0;
}

/*
 * work is the arena address UmnFirstLoad passes (UmnWorkEnd): the tree is
 * read into the next 0x800-aligned block and the address after it returned.
 */
int UmnHistoryTreeLoad(int work)
{
    int aligned = (work + 0x7FF) & ~0x7FF;
    int next = aligned + 0x800;

    UmnHistoryTreeBuf = (int *)aligned;
    xglCdReadFile(D_00A13620, (void *)aligned, 0, 0);
    return next;
}

int *UmnHistoryTreeGet(unsigned int history_id)
{
    int *tree = UmnHistoryTreeBuf;
    int *entry = 0;

    if (history_id < 128) {
        entry = &tree[history_id];
    }
    return entry;
}

/*
 * The first kosmos_special_tbl record whose id matches copies its reward-code
 * characters, stopping at a 0 character, into UmnKosmosSpecialBox.
 */
void UmnKosmosSpecialGetCheck(int id)
{
    int i;
    int copied;
    int column;
    int slot;

    slot = 0;
    for (i = 0; i < 4; i++) {
        if (kosmos_special_tbl[i][0] == id) {
            copied = 0;
            column = 1;
            do {
                if (kosmos_special_tbl[i][column] == 0) {
                    break;
                }
                UmnKosmosSpecialBox[slot] = kosmos_special_tbl[i][column];
                slot++;
                column++;
                copied++;
            } while (copied < 3);
            return;
        }
    }
}

unsigned char *UmnMailAttachGet(int box_id)
{
    return umn_attach_tbl + box_id * 3;
}

void UmnMailAttachSet(int box_id)
{
    UmnAttachmentState *state = &D_4A1A0C;
    unsigned char *attachment;
    int index;
    int scale;
    short exponent;
    int quantity;

    if (box_id <= 0) {
        return;
    }

    attachment = UmnMailAttachGet(box_id - 1);
    switch (attachment[0]) {
    case 1:
        state->plugin_unlock_flags |= 1 << attachment[1];
        break;
    case 2:
        if (attachment[2] != 0) {
            index = 0;
            do {
                dataItmBoxInc(attachment[1]);
                index++;
                quantity = attachment[2];
            } while (index < (short)quantity);
        }
        break;
    case 3:
        if (attachment[2] != 0) {
            index = 0;
            do {
                dataWpnBoxInc(attachment[1]);
                index++;
                quantity = attachment[2];
            } while (index < (short)quantity);
        }
        break;
    case 4:
        scale = 10;
        quantity = attachment[2];
        exponent = quantity + 1;
        if (exponent != 0) {
            quantity = exponent;
            do {
                quantity--;
                scale *= 10;
            } while (quantity != 0);
        }
        dataMoneyBoxInc(attachment[1] * scale);
        break;
    }
}

/* One 0x90-byte mail header record per mail_id. */
UmnMailHeader *UmnMailHeaderGet(int mail_id)
{
    return &UmnMailHeaderBuf[mail_id];
}

typedef struct UmnMailHeaderView {
    char sender[0x40];
    char title[0x40];
    unsigned char attach[3];
    unsigned char unmodeled_83;
    unsigned short flags;
    unsigned char kind;
    unsigned char level[5];
    int data;
} UmnMailHeaderView;

int UmnMailHeaderCreate(int work)
{
    int aligned = (work + 0xF) & ~0xF;
    unsigned char *list = (unsigned char *)(aligned + 0x4800);
    unsigned char *record = list;
    unsigned char *cursor = list;
    UmnMailList *entry;
    UmnMailListBody *body;
    UmnMailHeaderView *header;
    unsigned char *sender;
    unsigned char *title;
    int size;
    int slot;
    int level;
    int i;

    UmnMailHeaderBuf = (UmnMailHeader *)aligned;
    memset((void *)aligned, 0, 0x4800);
    xglCdReadFile(D_00A13640, list, 0, 0);
    size = ((UmnMailList *)cursor)->size;
    while (size != 0) {
        cursor = cursor + 4;
        entry = (UmnMailList *)(cursor - 4);
        header = (UmnMailHeaderView *)UmnMailHeaderBuf;
        header += entry->mail_id;
        cursor = cursor + 4;
        body = (UmnMailListBody *)cursor;
        for (i = 0; i < 3; i++) {
            header->attach[i] = body->attach[i];
        }
        header->flags = body->flags;
        header->kind = body->kind;
        slot = body->plugin >> 5;
        level = (body->plugin & 3) + 1;
        memset(header->level, 0, 5);
        if (slot >= 1 && slot <= 4) {
            header->level[slot - 1] = level;
        }
        header->data = body->data;
        cursor = cursor + 96;
        sender = cursor;
        while (*cursor != 0) {
            if (*cursor < 32) {
                cursor = cursor + xglFontGetSPcodeSize(*cursor, cursor);
                cursor = cursor + 1;
            } else {
                cursor = cursor + 1;
            }
        }
        cursor = cursor + 1;
        title = cursor;
        while (*cursor != 0) {
            if (*cursor < 32) {
                cursor = cursor + xglFontGetSPcodeSize(*cursor, cursor);
                cursor = cursor + 1;
            } else {
                cursor = cursor + 1;
            }
        }
        eMessageCpy(header->title, (char *)sender + 9);
        eMessageCpy(header->sender, (char *)title + 8);
        record = record + size + 4;
        cursor = record;
        size = ((UmnMailList *)cursor)->size;
    }
    return aligned + 0x4800;
}

/* One 0x80-byte plugin text record for the first six ids, falling back to
 * the text base for anything else. */
char *UmnPluginTextGet(unsigned int plugin_id)
{
    char *text = umn_text;
    char *entry = text;

    if (plugin_id < 6) {
        entry = umn_text + plugin_id * 0x80;
    }
    text = entry;
    return text;
}

int UmnTextLoad(int work, int mode)
{
    int aligned = (work + 0xF) & ~0xF;
    int next = aligned + 0x800;

    umn_text = (char *)aligned;
    if (mode == 0) {
        xglCdReadFile(f_name_0, (void *)aligned, 0, 1);
    } else {
        MenuLoadFile(f_name_0, (void *)aligned);
    }
    return next;
}

/*
 * compulsion_down_load_tbl lists mail_id values with a forced-download
 * attachment; bit 7 of the mail data's second byte marks it downloaded.
 */
int UmnMailCompulsionDownLoadCheck(int mail_id)
{
    unsigned char *data;
    int i;

    for (i = 0; i < 13; i++) {
        if (compulsion_down_load_tbl[i] == mail_id) {
            data = UmnMailDataGet(mail_id);
            if (data[1] & 0x80) {
                break;
            }
            return 1;
        }
    }
    return 0;
}

int UmnEventTextInit(int work)
{
    int aligned = (work + 0xF) & ~0xF;

    uet_text_buf = (char *)aligned;
    uet_flag = 0;
    return aligned + 0x2000;
}
