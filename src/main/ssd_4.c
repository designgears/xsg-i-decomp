#include "common.h"
#include "ssd_4.h"
#include "ssd_init.h"

/*
 * libkernel syscall stubs (main:0x00208fe0 / main:0x00209028).
 */
extern int DIntr(void);
extern int EIntr(void);

typedef struct { char pad[0x1C0]; void *first; unsigned end, size; } SsdMMWork;
typedef struct { int next; void *end; char flag; char x; char pad[2]; unsigned tag; } SsdMMHead;

void SsdInitMemoryManager(unsigned start, unsigned end)
{
    unsigned lo = start;
    unsigned hi = end & ~0x3F;
    SsdMMWork *w;
    SsdMMHead *p;

    if (lo & 0x3F) {
        lo = (lo + 0x3F) & ~0x3F;
        hi -= 0x40;
    }
    w = (SsdMMWork *)&RssdWork;
    w->first = (void *)lo;
    w->end = lo + hi;
    w->size = hi;
    p = (SsdMMHead *)lo;
    p->flag = 0x81;
    p->tag = 0x504F544D;
    p->end = (void *)(lo + 0x40);
    p->x = 0;
    p->next = 0;
}

/*
 * iSsdNewMemoryPtr is this TU's own low-address block allocator (its body is
 * still scaffold below, main:0x00242150). `tag` is stored verbatim at +0xc of
 * the returned block's header; nothing recovered in this TU reads it back.
 */
extern void *iSsdNewMemoryPtr(int size, int tag);

void *SsdNewMemoryPtr(int size, int tag)
{
    void *ptr;

    DIntr();
    ptr = iSsdNewMemoryPtr(size, tag);
    EIntr();
    return ptr;
}

extern void SsdClearMemory(void *buffer, int size);

void *iSsdNewMemoryPtr(int size, int tag)
{
    SsdMemoryBlock *next;
    SsdMemoryBlock *blk;
    SsdMMHead *h;
    char *buf;
    int need;
    int len;

    need = ((size + 0x3F) & ~0x3F) + 0x40;
    blk = RssdWork.first_block;
    for (;;) {
        next = blk->next;
        if (next != 0) {
            len = (int)next - blk->end;
            if (len < need) {
                blk = next;
                continue;
            }
            goto found;
        }
        break;
    }
    len = (int)((SsdMMWork *)&RssdWork)->end - blk->end;
    if (len < need)
        return 0;
found:
    h = (SsdMMHead *)((blk->end + 0x3F) & ~0x3F);
    buf = (char *)h + 0x40;
    h->tag = tag;
    h->flag = 2;
    h->end = buf + size;
    h->next = 0;
    h->x = 0;
    SsdClearMemory(buf, size);
    ((SsdMemoryBlock *)h)->next = blk->next;
    blk->next = (SsdMemoryBlock *)h;
    return buf;
}

/*
 * iSsdNewMemoryPtr2 is this TU's own high-address block allocator (its body
 * is still scaffold below, main:0x00242278); same header convention as
 * iSsdNewMemoryPtr.
 */
extern void *iSsdNewMemoryPtr2(int size, int tag);

void *SsdNewMemoryPtr2(int size, int tag)
{
    void *ptr;

    DIntr();
    ptr = iSsdNewMemoryPtr2(size, tag);
    EIntr();
    return ptr;
}

INCLUDE_ASM("asm/main/nonmatchings/ssd_4", iSsdNewMemoryPtr2);

/*
 * iSsdDisposeMemoryPtr is this TU's own block-release routine (its body is
 * still scaffold below, main:0x002423b0): `ptr` minus the fixed 0x40-byte
 * header size is the block header, returned to the free list.
 */
extern void iSsdDisposeMemoryPtr(void *ptr);

void SsdDisposeMemoryPtr(void *ptr)
{
    DIntr();
    iSsdDisposeMemoryPtr(ptr);
    EIntr();
}

void iSsdDisposeMemoryPtr(void *ptr)
{
    void **blk = (void **)((char *)ptr - 0x40);
    void **prev = (void **)RssdWork.first_block;
    void **cur = *prev;

    while (cur != blk) {
        if (cur == 0)
            return;
        prev = cur;
        cur = *cur;
    }
    *prev = *blk;
}

/*
 * Returns the payload size recorded in a sound-memory allocation header:
 * `ptr` minus one SsdMemoryBlock header is the header itself (same
 * convention as SsdDisposeMemoryPtr above), and `end` is the absolute
 * address just past the payload.
 */
int SsdGetBlockMemorySize(void *ptr)
{
    SsdMemoryBlock *block;

    block = (SsdMemoryBlock *)ptr - 1;
    return block->end - (int)block;
}

static inline int ssdTailSize(void *w, SsdMemoryBlock *b)
{
    return (*(int *)((char *)w + 0x1C4) - b->end) & ~0x3F;
}

int SsdGetMemoryFreeSize(void)
{
    SsdMemoryBlock *block;
    SsdMemoryBlock *next;
    SsdMemoryBlock *tmp;
    int best = 0;
    int size;

    DIntr();
    block = (SsdMemoryBlock *)RssdWork.first_block;
    next = block->next;
    if (next != 0) {
        do {
            size = ((int)next - block->end) & ~0x3F;
            block = next;
            tmp = block->next;
            next = tmp;
            if (best < size) best = size;
        } while (tmp != 0);
    }
    size = ssdTailSize(&RssdWork, block);
    if (best < size) best = size;
    EIntr();
    return best;
}

int SsdGetMemoryBlocks(void)
{
    SsdMemoryBlock *block;
    SsdMemoryBlock *nextBlock;
    int count;

    DIntr();
    count = 0;
    block = ((SsdMemoryBlock *)RssdWork.first_block)->next;
    if (block != 0) {
        do {
            nextBlock = block->next;
            count++;
            block = nextBlock;
        } while (nextBlock != 0);
    }
    EIntr();
    return count;
}

void SsdCopyMemory(void *destination, void *source, int size)
{
    int *dst32 = destination;
    unsigned int *src32 = source;
    unsigned char *dst8;
    unsigned char *src8;
    long count;
    long a, b, c, d;

    count = size >> 4;
    if (count != 0) {
        do {
            a = src32[0];
            b = src32[1];
            c = src32[2];
            d = src32[3];
            src32 += 4;
            dst32[0] = a;
            dst32[1] = b;
            dst32[2] = c;
            dst32[3] = d;
            dst32 += 4;
            count--;
        } while (count != 0);
    }

    count = (size >> 2) & 3;
    if (count != 0) {
        do {
            *dst32++ = *src32++;
            count--;
        } while (count != 0);
    }

    count = size & 3;
    if (count != 0) {
        src8 = (unsigned char *)src32;
        dst8 = (unsigned char *)dst32;
        do {
            *dst8++ = *src8++;
            count--;
        } while (count != 0);
    }
}

void SsdClearMemory(void *buffer, int size)
{
    unsigned long long *destination64 = buffer;
    unsigned int *destination32;
    unsigned char *destination_bytes;
    long block_count;
    long byte_count;

    block_count = size >> 5;
    if (block_count != 0) {
        do {
            destination64[3] = 0;
            destination64[2] = 0;
            destination64[1] = 0;
            destination64[0] = 0;
            destination64 += 4;
            block_count--;
        } while (block_count != 0);
    }

    destination32 = (unsigned int *)destination64;
    block_count = (size >> 4) & 1;
    if (block_count != 0) {
        do {
            destination32[3] = 0;
            destination32[2] = 0;
            destination32[1] = 0;
            destination32[0] = 0;
            destination32 += 4;
            block_count--;
        } while (block_count != 0);
    }

    destination_bytes = (unsigned char *)destination32;
    byte_count = size & 0xf;
    while (byte_count != 0) {
        *destination_bytes++ = 0;
        byte_count--;
    }
}
