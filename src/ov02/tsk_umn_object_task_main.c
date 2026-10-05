/*
 * OV02 original TU 5: 0x00a023a8..0x00a02d68 (7 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/xgl_cd.h"

#include "tsk_umn_object_task_main.h"
#include "main/xgl_task.h"
extern int UmnSimulationNo;
extern int UmnMemory;
extern unsigned char *umn_task;
extern unsigned char *task_xmx;
extern int UmnWorkEndTop;
extern int UmnEventTextMakeWork;
extern const char D_00A11D90[];
extern const char D_00A11DB0[];
extern void endPrintInit(void);
extern void GameCFSoundMenuPurge(int flag);
extern void xglRenderClearDepth(void);
extern int xglSoundSendSwd(void *swd, int bank);
extern int SsdSpuDmaCompleted(int wait);
extern int xglSoundSendSmd2(void *smd, int bank);
extern void xglSoundSequenceNormal2(int channel, int volume);
extern void xglFSrand(float seed);
extern void tyaUmlDispInit(void);
extern void tyaUmlDispInit2(unsigned char *work_buffer);
extern void *UmnInterface2Init(void *work);
extern void *UmnBgCubeInit(void *work);
extern void *eBattleWinInit2(void *arg);
extern int UmnMailHeaderCreate(int work);
extern int UmnHistoryTreeLoad(int work);
extern XglPacket *xglPacketGetCurrent(void);
extern void endPrintDirectFrameCopy(XglPacket *packet, int width, int height);
extern void xglSleep(void);
extern void *memset(void *dest, int value, unsigned int size);
typedef struct {
    unsigned char unmodeled_00[0x10];
    unsigned short width;
    unsigned short height;
    unsigned short height2;
} UmnMainRenderSize;
extern UmnMainRenderSize sRender;
extern int MenuModelWorkTop;
extern void MenuLoadInit(void);
extern void MenuModelMemoryInit(void *base, int size);
extern void MenuModelMemorySet(int count);
extern int UmnModelUkn;
extern int UmnModelSon;
extern void UmnChangeTopLevel(int level);
extern void GameSnapShotCheck(void);
extern void UmnTopMenu(void);
extern void UmnMail(int menuId);
extern void UmnDataBase(void);
extern void UmnSimulation(void);
extern void UmnPlugin(void);
extern void endPrintExtFunc(int kind, int id, void *data);
extern void UmnInterface2Main(void);
extern void MenuModelMain(void);
extern void MenuModelAllBreak(void);
extern void GameCFSoundMenuReload(void);

int tskUmnObjectTaskMain(XglTaskPrefix *base) {
    UmnObjectTask *task = (UmnObjectTask *)base;
    UmnObjectTaskWorker worker = task->worker;

    if (UmnWork.mode == 0xff)
        task->state = -1;

    switch (task->state) {
    case 0:
        worker(task, task->data);
        task->state = 2;
        /* fall through */
    case 2:
        worker(task, task->data);
        break;
    case -1:
        xglTaskWaitRemove(base);
        break;
    }
}

void UmnObjectTaskCreate(UmnObjectTaskWorker worker, void *data) {
    XglTaskScheduler *scheduler = (XglTaskScheduler *)umn_task;
    UmnObjectTask *task = (UmnObjectTask *)xglTaskEntryNext(
        scheduler, tskUmnObjectTaskMain,
        scheduler != 0 ? scheduler->active_tail : 0);
    int i;

    task->state = 0;
    task->worker = worker;
    task->data = data;
    for (i = 24; i >= 0; --i)
        task->zeroedOnCreate[i] = 0;
}

typedef int umn_q __attribute__((mode(TI)));
void UmnProcuratorSet(void);

void UmnChangeTopLevel(int lvl)
{
    umn_q *p = (umn_q *)&UmnWork;
    int i;
    for (i = 1; i < 8; i++) {
        p[i] = 0;
    }
    UmnWork.mode = lvl;
    UmnWork.unmodeled_02[1] = 0;
    UmnProcuratorSet();
}

/* UmnTextLoad is ov02:0x00a0f578 (src/ov02/umn_event_text_symbol_check.c). */
extern int UmnTextLoad(int work, int mode);

/* "data\\endou\\umn\\cube.xtx", "data\\endou\\umn\\cube.lex" and
 * "data\\endou\\umn\\umn00.xtx" filename buffers loaded below. */
extern const char D_00A11D40[];
extern const char D_00A11D58[];
extern const char D_00A11D70[];

extern int UmnBgCubeXtx;
extern int UmnBgCubeLex;
extern int UmnTexAddr;
extern int UmnWorkEnd;

static void UmnFirstLoad(void) {
    UmnWorkEnd = UmnTextLoad(UmnWorkEnd, 0);
    xglCdReadFile(D_00A11D40, (void *)UmnBgCubeXtx, 0, 1);
    xglCdReadFile(D_00A11D58, (void *)UmnBgCubeLex, 0, 1);
    xglCdReadFile(D_00A11D70, (void *)UmnTexAddr, 0, 1);
}

static void UmnInit(void) {
    int buffer;
    int base;
    int next;

    endPrintInit();
    GameCFSoundMenuPurge(1);
    UmnSimulationNo = 0;
    xglRenderClearDepth();

    buffer = (UmnMemory + 0xF) & ~0xF;
    xglCdReadFile(D_00A11D90, (void *)buffer, 0, 1);
    xglSoundSendSwd((void *)buffer, -5);
    while (SsdSpuDmaCompleted(0))
        ;

    xglCdReadFile(D_00A11DB0, (void *)buffer, 0, 1);
    xglSoundSendSmd2((void *)buffer, 4);
    xglSoundSequenceNormal2(4, 0x7F);
    xglFSrand(0.12345679f);
    tyaUmlDispInit();
    tyaUmlDispInit2((unsigned char *)UmnMemory);

    base = UmnMemory;
    UmnBgCubeXtx = base + 0x180000;
    UmnBgCubeLex = base + 0x190000;
    UmnTexAddr = base + 0x1A0000;
    UmnWorkEndTop = base + 0x1E0800;
    UmnMemory = base + 0x220800;
    UmnWorkEnd = UmnWorkEndTop;
    umn_task = (unsigned char *)UmnWorkEndTop;
    UmnWorkEnd = (int)xglTaskInitial((void *)UmnWorkEndTop, 0x20, 0);

    task_xmx = (unsigned char *)UmnWorkEnd;
    UmnWorkEnd = (int)xglTaskInitial((void *)UmnWorkEnd, 0x20, 0);
    UmnWorkEnd = (int)UmnInterface2Init((void *)UmnWorkEnd);
    UmnWorkEnd = (int)UmnBgCubeInit((void *)UmnWorkEnd);
    UmnWorkEnd = (int)eBattleWinInit2((void *)UmnWorkEnd);
    UmnWorkEnd = UmnMailHeaderCreate(UmnWorkEnd);
    next = UmnHistoryTreeLoad(UmnWorkEnd);

    UmnEventTextMakeWork = next;
    UmnWorkEnd = next + 0x2000;
}

void UmnMain(void) {
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    endPrintDirectFrameCopy(packet, sRender.width << 5, sRender.height << 5);
    xglSleep();

    packet = xglPacketGetCurrent();
    endPrintDirectFrameCopy(packet, sRender.height << 5, sRender.height2 << 5);

    UmnMemory = MenuModelWorkTop;
    memset((void *)UmnMemory, 0, 0x480000);

    UmnInit();
    MenuLoadInit();
    MenuModelMemoryInit((void *)UmnMemory, 0x200000);
    MenuModelMemorySet(0);

    UmnModelUkn = 0;
    UmnModelSon = 0;
    UmnChangeTopLevel(0);
    UmnFirstLoad();

    for (;;) {
        GameSnapShotCheck();
        switch (UmnWork.mode) {
        case 0:
            UmnTopMenu();
            break;
        case 1:
            UmnMail(0);
            break;
        case 2:
            UmnDataBase();
            break;
        case 3:
            UmnSimulation();
            break;
        case 4:
            UmnPlugin();
            break;
        default:
            UmnChangeTopLevel(0);
            break;
        }

        endPrintExtFunc(0, 0x64, 0);
        UmnInterface2Main();
        xglTaskExecute((XglTaskScheduler *)task_xmx);
        MenuModelMain();
        xglTaskExecute((XglTaskScheduler *)umn_task);
        xglSleep();

        if (UmnWork.mode != 0xff) {
            if (UmnSimulationNo == 0)
                continue;
            UmnWork.mode = 0xff;
        }
        break;
    }

    packet = xglPacketGetCurrent();
    endPrintDirectFrameCopy(packet, sRender.width << 5, sRender.height << 5);
    xglTaskExecute((XglTaskScheduler *)task_xmx);
    xglTaskExecute((XglTaskScheduler *)umn_task);
    MenuModelAllBreak();
    xglSleep();
    GameCFSoundMenuReload();
}

typedef struct { unsigned char pad[0x10]; unsigned int flags; } UmnMain2LoopState;
extern UmnMain2LoopState GameLoopState;
typedef union UmnMain2LightVector {
    double alignment;
    Vector4 value;
} UmnMain2LightVector;
typedef struct UmnMain2LightTable {
    UmnMain2LightVector ambient;
    UmnMain2LightVector color[3];
    UmnMain2LightVector direction[3];
} UmnMain2LightTable;
extern const UmnMain2LightTable D_00A11DF0;
extern void MenuCfTaikiPush(void);
extern void MenuCfTaikiPop(void);
extern void MenuGameDataPush(void);
extern void MenuGameDataPop(void);
extern void xglStudioChange(int studio_index);
extern void *xglStudioGetLight2(void);
extern void xglLightIntensityAmbient(void *light, const Vector4 *intensity);
extern void xglLightIntensityParallel(void *light, unsigned int index, const Vector4 *intensity);
extern void xglLightDirection(void *light, unsigned int index, const Vector4 *direction);
extern void *GameResourceWorkAlloc(int size);
extern int MenuModelInit(int work_start);
extern int MenuFontLoad(int load, int cb);
extern void WindowTexLoad(int window_id, int texture_set);
extern void VibrationEngine(void);
extern void MenuLoadCancel(void);
extern void GameResourceWorkReload(void);
extern void GameCfPlayerLoadResource(int flag);

void UmnMain2(int menu_id) {
    UmnMain2LightTable table;
    void *light;
    void *mem;

    endPrintDirectFrameCopy(xglPacketGetCurrent(), sRender.width << 5, sRender.height << 5);
    xglSleep();
    endPrintDirectFrameCopy(xglPacketGetCurrent(), sRender.height << 5, sRender.height2 << 5);
    GameLoopState.flags |= 0x04000000;
    MenuCfTaikiPush();
    MenuGameDataPush();
    GameCFSoundMenuPurge(0);
    MenuLoadInit();
    xglStudioChange(0);
    light = xglStudioGetLight2();
    table = D_00A11DF0;
    xglLightIntensityAmbient(light, &table.ambient.value);
    xglLightIntensityParallel(light, 0, &table.color[0].value);
    xglLightDirection(light, 0, &table.direction[0].value);
    xglLightIntensityParallel(light, 1, &table.color[1].value);
    xglLightDirection(light, 1, &table.direction[1].value);
    xglLightIntensityParallel(light, 2, &table.color[2].value);
    xglLightDirection(light, 2, &table.direction[2].value);
    memset((void *)(UmnMemory = (int)GameResourceWorkAlloc(0x480000)), 0, 0x480000);
    UmnInit();
    MenuLoadInit();
    UmnWorkEnd = MenuModelInit(UmnWorkEnd);
    MenuModelMemoryInit((void *)UmnMemory, 0x200000);
    MenuModelMemorySet(0);
    UmnModelUkn = 0;
    UmnModelSon = 0;
    UmnChangeTopLevel(1);
    UmnFirstLoad();
    MenuFontLoad(1, 0);
    WindowTexLoad(0, 1);
    WindowTexLoad(0, 2);
    while (UmnModelUkn == 0 || ((unsigned char *)UmnModelUkn)[0x11] == 0) {
        UmnInterface2Main();
        MenuModelMain();
        xglSleep();
    }
    while (UmnWork.mode == 1) {
        GameSnapShotCheck();
        UmnMail(menu_id);
        UmnInterface2Main();
        endPrintExtFunc(0, 0x64, 0);
        xglTaskExecute((XglTaskScheduler *)task_xmx);
        MenuModelMain();
        xglTaskExecute((XglTaskScheduler *)umn_task);
        VibrationEngine();
        xglSleep();
    }
    MenuLoadCancel();
    endPrintDirectFrameCopy(xglPacketGetCurrent(), sRender.width << 5, sRender.height << 5);
    xglTaskExecute((XglTaskScheduler *)task_xmx);
    xglTaskExecute((XglTaskScheduler *)umn_task);
    MenuModelAllBreak();
    xglSleep();
    MenuCfTaikiPop();
    MenuFontLoad(0, 0);
    WindowTexLoad(0, 0);
    GameResourceWorkReload();
    GameCfPlayerLoadResource(1);
    MenuGameDataPop();
    GameCFSoundMenuReload();
    GameLoopState.flags &= ~0x04000000;
}
