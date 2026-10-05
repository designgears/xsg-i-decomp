#include "common.h"

extern unsigned char *MenuEtherDataBuf;

void *MenuEtherDataGet(int id)
{
    int index = (unsigned short)id;
    unsigned char *buf = MenuEtherDataBuf;

    if ((unsigned int)(index - 1) >= 0x50U)
        return 0;
    return buf + (index << 2) - 4;
}

int MenuEtherJoutoCheck(int id)
{
    unsigned int index = (unsigned short)id;
    unsigned int flags;

    if ((index - 1) >= 0x50u)
        return 0;
    flags = ((unsigned short *)func_A1A488(index))[1] & 0x4000;
    return flags == 0;
}

extern int *func_A19210(unsigned short chrNo);

int MenuEtherCharPointCheck(unsigned short charId, unsigned short etherId)
{
    int pts = func_A19210(charId)[4];
    if (pts < ((short *)func_A1A488(etherId))[4]) {
        return 0;
    }
    return 1;
}

int MenuEtherWhoCheck(int id)
{
    unsigned int index = (unsigned short)id;

    if ((index - 1) < 0x10u)
        return 3;
    if ((index - 0x11) < 0xAu)
        return 2;
    if ((index - 0x1B) < 0xAu)
        return 1;
    if ((index - 0x25) < 0xCu)
        return 6;
    if ((index - 0x31) < 0xAu)
        return 7;
    if ((index - 0x3B) < 0xDu)
        return 4;
    if ((index - 0x48) < 0x8u)
        return 5;
    return 0;
}

extern char *func_A191C0(unsigned short);

int MenuEtherSetCheck(unsigned short charId, unsigned short etherId)
{
    short *p = (short *)(func_A191C0(charId) + 0x8E);
    int i;
    for (i = 0; i < 12; i++) {
        if (*p++ == etherId) {
            return 1;
        }
    }
    return 0;
}

/*
 * The retail ELF names these ov01 data-table accessors (still assembler in
 * their own TU, src/ov01/data_unit_org_get.c) dataEthGet (0x00a1a488) and
 * dataTecGet (0x00a1a378); until that TU recovers them, the synthetic
 * linker script only binds their scaffold func_<VA> spelling.
 */
typedef struct EtherStatus {
    unsigned char unmodeled_00[6];
    short tecId; /* +6: technique dataTecGet resolves for this ether */
} EtherStatus;

typedef struct TecStatus {
    unsigned char unmodeled_00[4];
    signed char type; /* +4 */
} TecStatus;

extern EtherStatus *func_A1A488(unsigned short etherId); /* dataEthGet */
extern TecStatus *func_A1A378(short tecId);               /* dataTecGet */

signed char MenuEtherTypeGet(int etherId) {
    return func_A1A378(func_A1A488(etherId)->tecId)->type;
}

int MenuEtherUseCheck(unsigned short id)
{
    switch (id) {
    case 1:
    case 8:
    case 0x25:
        return 1;
    case 4:
    case 0x1C:
        return 2;
    case 0x4A:
        return 3;
    }
    return 0;
}

extern char *ld191c0(int) __asm__("func_A191C0");
extern EtherStatus *ld1a488(int) __asm__("func_A1A488");

void MenuEtherCapSet(void)
{
    int i;

    for (i = 1; i < 8; i++) {
        int j;
        char *org = ld191c0(i);
        short *p = (short *)(org + 0x8E);

        org[0x17] = 0;
        j = 0;
        __asm__(".align 3");
    lp:
        if (j >= 12)
            goto end;
        j++;
        {
            short id = *p++;
            if (id == 0)
                goto end;
            org[0x17] = org[0x17] + ld1a488(id)->unmodeled_00[4];
        }
        goto lp;
    end:;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherPasMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherInfoMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherMenuMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherExMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherL1R1Main);

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherStatusMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherStatusMain2);

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherListMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherListMain2);

typedef struct {
    int name;
    int value;
    unsigned char unk8;
    unsigned char unk9;
    unsigned char pad[2];
} MenuEtherRow;
extern MenuEtherRow *MenuListGet(int list);
extern MenuEtherRow *MenuListMake(int list, int mode);
extern int *MenuSortAddrGet(int list);
extern int MenuSortCheck(int list);
extern int *MenuTextGet(int index);
extern int func_00A11220(int, int);
extern int etherUseCk(int) __asm__("MenuEtherUseCheck");
extern struct { char pad[0x40]; signed char unk40; } MenuWork;

void MenuEtherListMake00(void)
{
    MenuEtherRow *row = MenuListGet(0);
    int *sort = MenuSortAddrGet(0);
    int n = MenuSortCheck(0);
    char *chr = ld191c0(MenuWork.unk40);
    int v;

    MenuListMake(0, 0);
    if (n > 0) do {
        func_A1A378(ld1a488(*(short *)sort)->tecId);
        row->name = *MenuTextGet(*sort);
        v = func_00A11220(MenuWork.unk40, *(short *)sort);
        row->value = v;
        if (*(short *)(chr + 0x36) >= v) {
            row->unk8 = 0;
            row->unk9 = 0;
        } else {
            row->unk8 = 1;
            row->unk9 = 1;
        }
        if (etherUseCk(*(short *)sort) == 0) {
            row->unk8 = 1;
            row->unk9 = 2;
        }
        row++;
        sort++;
    } while (--n != 0);
}

void MenuEtherListMake01(void)
{
    MenuEtherRow *row = MenuListGet(0);
    int *sort = MenuSortAddrGet(0);
    int n = MenuSortCheck(0);
    char *chr = ld191c0(MenuWork.unk40);
    EtherStatus *e;
    int i, j;

    MenuListMake(0, 0);
    for (i = 0; i < n; i++) {
        e = ld1a488((short)sort[i]);
        func_A1A378(e->tecId);
        row[i].name = *MenuTextGet(sort[i]);
        row[i].value = *(short *)((char *)e + 4);
        if (chr[0x17] + row[i].value < 13) {
            row[i].unk8 = 0;
        } else {
            row[i].unk8 = 1;
        }
        for (j = 0; j < 12; j++) {
            if (((short *)(chr + 0x8E))[j] == (short)sort[i]) break;
        }
        if (j != 12) {
            row[i].unk9 = 1;
        } else {
            row[i].unk9 = 0;
        }
    }
}

extern int *ld19210(int) __asm__("func_A19210");

void MenuEtherListMake02(int list)
{
    MenuEtherRow *row = MenuListGet(list);
    int *sort = MenuSortAddrGet(list);
    int n = MenuSortCheck(list);
    int *chr = ld19210(MenuWork.unk40);
    int i, v, who;

    MenuListMake(list, 0);
    for (i = 0; i < n; i++) {
        row[i].value = ((short *)ld1a488((short)sort[i]))[4];
        who = MenuEtherWhoCheck((short)sort[i]);
        v = row[i].value;
        if (v == 0) {
            row[i].unk8 = 1;
            row[i].value = -1;
        } else if (MenuWork.unk40 != who) {
            row[i].unk8 = 1;
        } else if (chr[4] >= v / 2) {
            row[i].unk8 = 0;
        } else {
            row[i].unk8 = 1;
        }
    }
}

extern void MenuSortSet(int list, int mode, int who);
extern struct { char pad[0x44]; signed char unk44; char pad45; signed char unk46; } MenuWork3 __asm__("MenuWork");

void MenuEtherListMake03(int list)
{
    MenuEtherRow *row = MenuListGet(list);
    int *sort = MenuSortAddrGet(list);
    unsigned char *p;
    int i, n, idx;

    p = (unsigned char *)ld19210(MenuWork3.unk46);
    idx = MenuWork3.unk44 - 1;
    p = (unsigned char *)((idx / 8) + (int)p) + 0x28;
    *p |= 1 << (idx % 8);
    MenuSortSet(list, 0x80, MenuWork3.unk46);
    *p &= ~(1 << ((MenuWork3.unk44 - 1) % 8));
    MenuListMake(list, 0);
    n = MenuSortCheck(list);
    for (i = 0; i < n; i++) {
        row[i].value = ((short *)ld1a488((short)sort[i]))[4];
        if ((short)sort[i] == MenuWork3.unk44) {
            row[i].unk8 = 1;
        } else {
            row[i].unk8 = 0;
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEtherEquip);

INCLUDE_ASM("asm/main/nonmatchings/menu_ether", MenuEther);
