#include "common.h"
#include "act_1.h"

extern void *memset(void *destination, int value, unsigned int count);

extern void *RSRC_alloc(MatrixHeap *heap, int size, int tag);
extern void ACT_resetMatrix(void *actor);

int ACT_allocMatrix(char *actor, int count)
{
    int size64;
    int size128;
    char *mem;
    char *p;
    int extra;

    if (count < 0) {
        char *mdl = *(char **)(actor + 0x8D8);
        count = 1;
        if (mdl != 0) {
            count = *(unsigned short *)(mdl + 6);
        }
    }
    if (actMatrixHeap == 0) {
        return -1;
    }
    if (count <= 0) {
        return count;
    }
    size64 = count << 6;
    size128 = count << 7;
    extra = 0;
    if (*(unsigned char *)(actor + 0x82) == 1) {
        extra = size64;
    }
    mem = RSRC_alloc(actMatrixHeap, size128 + extra, (int)actor);
    if (mem != 0) {
        *(char **)(actor + 0x828) = mem + size64;
        *(char **)(actor + 0x824) = mem;
        if (*(unsigned char *)(actor + 0x82) == 1) {
            p = mem + size128;
            *(char **)(actor + 0x82C) = p;
            *(char **)(actor + 0x830) = p + (count << 5);
            memset(p, 0, size64);
        }
        ACT_resetMatrix(actor);
    }
    return count;
}

/*
 * RSRC_alloc is defined by another TU (src/main/rsrc.c, still unrecovered)
 * and is not yet declared in a shared header, so its parameters are
 * declared here from this call site: main:0x00305a84 shifts the block count
 * left by 6 (multiplying by 64) into the size argument, and main:0x00305a7c
 * moves this function's own tag argument into the third argument unchanged.
 */
extern void *RSRC_alloc(MatrixHeap *heap, int size, int tag);

/* Returns the new block's storage (RSRC_alloc returns the item's +0x08 data). */
void *ACT_allocBlock(int tag, int blockCount)
{
    return RSRC_alloc(actMatrixHeap, blockCount << 6, tag);
}

/*
 * ACT_matrixInit establishes the shared actor matrix-heap state consumed by
 * ACT_allocMatrix/ACT_allocBlock through actMatrixHeap before any allocation.
 *
 * Field roles are proven by bounded callers:
 * - heap_origin (+0x00): origin subtracted from the cursor in RSRC_check
 *   (main:0x0026d780: lw v0,4(a0) / lw a2,0(a0) / subu v0,v0,a2); never
 *   advanced after init, so it anchors the heap while the cursor moves.
 * - heap_cursor (+0x04): bump cursor advanced per item in RSRC_alloc
 *   (main:0x0026d84c: lw v0,4(s1) ... addu v0,v0,v1 / sw v0,4(s1)).
 * - heap_bound (+0x08): byte bound compared in RSRC_check
 *   (main:0x0026d78c: lw v1,8(a0) / sltu v0,v0,v1); init value 0xC0000.
 * - heap_store (+0x0c): item storage indexed by RSRC_alloc (0x0026d820:
 *   lw v0,12(s1) / addu s0,v0,a2) and RSRC_info/RSRC_getDirtyItem.
 * - heap_count (+0x10): item count incremented per alloc (0x0026d824:
 *   sh a0,16(s1) with a0 = count+1); loop bound in RSRC_info.
 * - heap_max (+0x12): max items 64 (0x0026d7d8: lhu v1,18(s1) /
 *   sltu v1,v0,v1 gate).
 * Later MatrixHeap fields past offset 0x14 remain unknown; this private type
 * intentionally stops at the six evidenced offsets.
 */

void ACT_matrixInit(void)
{
    matrixHeap.heap_bound = 0xC0000;
    matrixHeap.heap_max = 64;
    matrixHeap.heap_count = 0;
    actMatrixHeap = &matrixHeap;
    matrixHeap.heap_origin = actMatrix;
    matrixHeap.heap_store = matrixHeapBlock;
    matrixHeap.heap_cursor = actMatrix;
    memset(matrixHeapBlock, 0, 0x400);
}

typedef unsigned int ActInitQuad __attribute__((mode(TI)));
extern void UnduParamInit(void *p);
extern void EXM_InitWind(void);
extern void EXM_InitMovedHair(ActRecord *self);
#define AF(T, a, off) (*(T *)((char *)(a) + (off)))

void ACT_init(void)
{
    ActRecord *a;
    int i;
    int n;

    ACT_matrixInit();
    for (n = 0; n < 0x40; n++) {
        a = &actor[n];
        a->flags = 0x20;
        AF(int, a, 0x9a8) = 0x75;
        AF(short, a, 0x84) = 0;
        AF(int, a, 0x900) = 0;
        a->inUseId = 0;
        a->update = 0;
        a->draw = 0;
        a->parent = 0;
        AF(float, a, 0x60) = 1.0f;
        AF(float, a, 0x64) = 1.0f;
        AF(float, a, 0x68) = 1.0f;
        AF(float, a, 0x6c) = 1.0f;
        AF(float, a, 0x1c) = 1.0f;
        AF(float, a, 0x7c) = 1.0f;
        AF(int, a, 0x3c) = 0;
        AF(int, a, 0x4c) = 0;
        AF(int, a, 0x9a0) = 0;
        AF(int, a, 0x9a4) = 0;
        AF(int, a, 0x9ac) = 0;
        AF(int, a, 0x9d0) = 0;
        AF(int, a, 0x9e0) = 0;
        AF(int, a, 0x754) = 0;
        AF(int, a, 0x758) = -1;
        AF(char, a, 0x83) = 0;
        AF(int, a, 0x698) = 0;
        AF(int, a, 0x69c) = -1;
        AF(short, a, 0x696) = 0;
        for (i = 7; i >= 0; i--) {
            ((char *)a + 0x6e0)[i] = 0;
        }
        for (i = 0; i < 3; i++) {
            __asm__ volatile("sq $0, 0(%0)" : : "r"((char *)a + 0x970 + i * 16) : "memory");
        }
        for (i = 10; i >= 0; i--) {
            ((int *)((char *)a + 0x8d0))[i] = 0;
        }
        for (i = 3; i >= 0; i--) {
            ((int *)((char *)a + 0x904))[i] = 0;
        }
        UnduParamInit((char *)a + 0x4c8);
        AF(short, a, 0x676) = 0;
        AF(int, a, 0x678) = 0;
        AF(short, a, 0x674) = 1;
        AF(int, a, 0x67c) = 0;
        __asm__ volatile("sq $0, 0(%0)" : : "r"((char *)a + 0x640) : "memory");
        __asm__ volatile("sq $0, 0(%0)" : : "r"((char *)a + 0x650) : "memory");
        __asm__ volatile("sq $0, 0(%0)" : : "r"((char *)a + 0x660) : "memory");
        AF(float, a, 0x66c) = 1.0f;
        AF(float, a, 0x62c) = 1.0f;
        AF(float, a, 0x670) = 1.0f;
        AF(int, a, 0x690) = 0;
        EXM_InitMovedHair(a);
        AF(int, a, 0xa60) = 0;
    }
    EXM_InitWind();
}

extern float D_004D8450;
extern void EXM_InitMovedHair(ActRecord *self);

ActRecord *ACT_create(int id, int code)
{
    ActRecord *a;
    int i;

    if (id < 0) {
        if (id == -1) {
            for (i = 0; i < ACTOR_COUNT; i++) {
                if (actor[i].inUseId == 0) {
                    id = i;
                    break;
                }
            }
        } else if (id == -2) {
            for (i = ACTOR_COUNT - 1; i >= 0; i--) {
                if (actor[i].inUseId == 0) {
                    id = i;
                    break;
                }
            }
        }
        if (id < 0) {
            return 0;
        }
    }
    if ((unsigned int)id >= ACTOR_COUNT) {
        return 0;
    }
    a = &actor[id];
    a->unmodeled_0c[0x82 - 0x0c] = code >> 16;
    a->flags = 0x20;
    a->unmodeled_0c[0x80 - 0x0c] = id;
    a->inUseId = code;
    *(int *)&a->unmodeled_900[0] = 0;
    *(float *)&a->unmodeled_0c[0x60 - 0x0c] = 1.0f;
    *(float *)&a->unmodeled_0c[0x64 - 0x0c] = 1.0f;
    *(float *)&a->unmodeled_0c[0x68 - 0x0c] = 1.0f;
    *(float *)&a->unmodeled_0c[0x6c - 0x0c] = 1.0f;
    a->parent = 0;
    a->draw = 0;
    if (!(code & 0xF000)) {
        a->unmodeled_88[0x90 - 0x88] = 4;
        a->unmodeled_88[0x91 - 0x88] = 0x10;
    } else {
        a->unmodeled_88[0x90 - 0x88] = 1;
        a->unmodeled_88[0x91 - 0x88] = 0x50;
    }
    *(int *)&a->unmodeled_900[0x9f4 - 0x900] = 0;
    *(float *)&a->unmodeled_900[0x9e8 - 0x900] = D_004D8450;
    *(int *)&a->unmodeled_900[0x9f8 - 0x900] = 0;
    EXM_InitMovedHair(a);
    *(int *)&a->unmodeled_900[0xa60 - 0x900] = 0;
    return a;
}

extern void RSRC_inactiveSource(MatrixHeap *heap, ActRecord *actor);
extern void ACT_resetParent(ActRecord *self, ActRecord *parent);
void ACT_dispose2(ActRecord *self, int quick);

/*
 * The plain default-options entry: ActorAndResourceDispose (main:0x0027fec0)
 * tail-calls it, and it always asks ACT_dispose2 for the full teardown
 * (main:0x00305e64..0x00305e6c).
 */
void ACT_dispose(ActRecord *self)
{
    ACT_dispose2(self, 0);
}

/*
 * Releases one actor entry: RSRC_inactiveSource retires its matrix-heap
 * resources, a live parent link is handed to ACT_resetParent before it is
 * cleared, and the entry's own hooks and in-use id go back to zero. `quick`
 * skips the ten-word block immediately before the parent link
 * (+0x8d4..+0x8f8) when it is nonzero; ACT_dispose always passes zero (full
 * teardown).
 */
void ACT_dispose2(ActRecord *self, int quick)
{
    ActRecord *parent;
    int *word;
    int count;

    RSRC_inactiveSource(actMatrixHeap, self);
    parent = self->parent;
    if (parent != 0) {
        ACT_resetParent(self, parent);
    }
    self->inUseId = 0;
    self->update = 0;
    self->draw = 0;
    self->flags = 0;
    if (!quick) {
        word = (int *)((unsigned char *)self + ACTOR_LINKED_BLOCK_END_OFFSET);
        count = 10;
        do {
            count -= 1;
            *word = 0;
            word -= 1;
        } while (count >= 0);
    }
}

extern void EXM_StepShakeWind(void);
extern void ACT_info_00306090(void);
extern void ACT_modelDraw(ActRecord *actor);

/*
 * Steps the wind simulation, refreshes the debug actor printer, then draws
 * every in-use actor (ACT_info_00306090's own "ACT[%02x]"/in-use reading
 * confirms the same test; see the ActRecord comment in act_1.h).
 */
void ACT_draw(void)
{
    ActRecord *cursor;
    int remaining;

    remaining = ACTOR_COUNT - 1;
    EXM_StepShakeWind();
    ACT_info_00306090();
    cursor = actor;
    do {
        if (cursor->inUseId != 0) {
            ACT_modelDraw(cursor);
        }
        remaining -= 1;
        cursor += 1;
    } while (remaining >= 0);
}

extern void xglStudioFlushActiveCamera(void);
extern void DB_reset(int, int);
extern void LOOK_target_doit(void);

/*
 * Refreshes the active camera, resets the debug light-write window with two
 * literal arguments (the only values this TU ever passes), then runs every
 * in-use actor's own per-frame update hook (main:0x00305f94: jalr through
 * the entry's `update` member, skipped when null) before driving the
 * look-at target.
 */
void ACT_update(void)
{
    ActRecord *cursor;
    int remaining;

    remaining = ACTOR_COUNT - 1;
    xglStudioFlushActiveCamera();
    DB_reset(8, 8);
    cursor = actor;
    do {
        if (cursor->inUseId != 0) {
            if (cursor->update != 0) {
                cursor->update(cursor);
            }
        }
        remaining -= 1;
        cursor += 1;
    } while (remaining >= 0);
    LOOK_target_doit();
}

extern void ACT_updateMotionPause(ActRecord *actor);

void ACT_pauseUpdate(void)
{
    int index;

    xglStudioFlushActiveCamera();
    for (index = 0; index < ACTOR_COUNT; index++) {
        actor[index].runtimeFlags |= 0x80000;
    }
    for (index = 0; index < ACTOR_COUNT; index++) {
        if (actor[index].inUseId != 0 && (actor[index].flags & 8) == 0) {
            ACT_updateMotionPause(&actor[index]);
        }
    }
    for (index = 0; index < ACTOR_COUNT; index++) {
        actor[index].runtimeFlags &= ~0x80000;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/act_1", ACT_info_00306090);
