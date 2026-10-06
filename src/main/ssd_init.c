#include "common.h"
#include "shared.h"
#include "ssd_init.h"

/*
 * libkernel syscall stub (main:0x00200550).
 */
extern int iSignalSema(int sema_id);
extern void SleepThread(void);

/*
 * RssdInitIop is this TU's own IOP sound RPC client setup (its body is still
 * scaffold below); sceSifRpcLoop is the SCE SDK SIF RPC server loop.
 */
extern void RssdInitIop(void);
extern void sceSifRpcLoop(void *queue);

INCLUDE_ASM("asm/main/nonmatchings/ssd_init", SsdInit);

enum {
    RSSD_CMD_QUIT    = 0x02,
    RSSD_CMD_RESUME  = 0x05,
    RSSD_CMD_SUSPEND = 0x06
};

/*
 * libkernel syscall stubs.
 */
extern int DeleteSema(int sema_id);
extern int DeleteThread(int thread_id);
extern int TerminateThread(int thread_id);

/*
 * This TU's own RPC completion handler (its body is still scaffold below);
 * sceSifRemoveRpc is the SCE SDK RPC server deregistration call.
 */
extern void RssdFuncCallCompleted(int status);
extern void *sceSifRemoveRpc(void *sd, void *qd);

/*
 * Sends the quit command, then tears down the RPC receive callback, the SIF
 * RPC server registration and the two service threads SsdInit started.
 */
void SsdQuit(void)
{
    RssdRequest request;

    RssdWork.flags &= ~RSSD_FLAG_SUCCESS;
    RssdCallFunc(RSSD_CMD_QUIT, &request, 0, 0);
    RssdFuncCallCompleted(1);
    DeleteSema(RssdWork.sema_id);
    sceSifRemoveRpc(RssdWork.rpc_server, RssdWork.rpc_queue);
    TerminateThread(RssdWork.rpc_thread_id);
    DeleteThread(RssdWork.rpc_thread_id);
    TerminateThread(RssdWork.next_wave_thread_id);
    DeleteThread(RssdWork.next_wave_thread_id);
    RssdWork.flags = 0;
}

/*
 * Initializes the IOP sound RPC client, then runs the SIF RPC receive loop
 * on the queue reserved at RssdWork.rpc_queue. It never returns.
 */
void RSsdSifRpcThread(void)
{
    RssdInitIop();
    sceSifRpcLoop(RssdWork.rpc_queue);
}

extern int sceSifBindRpc(void *cd, unsigned int request, unsigned int mode);
extern int sceSifCheckStatRpc(void *cd);
extern void *sceSifSetRpcQueue(void *qd, int thread_id);
extern void sceSifRegisterRpc(void *sd, unsigned int command, void *func, void *buff, void *cfunc, void *cbuff, void *qd);
extern int GetThreadId(void);
extern int printf(const char *format, ...);
extern const char D_004D4A60[];
extern void RssdSifRpcServer();

void RssdInitIop(void)
{
    RssdRequest request;
    RssdWorkFlags *w;

    if (sceSifBindRpc(&RssdWork._unmodeled_0a0[0x80], 0x900, 1) < 0)
        return;
    while (sceSifCheckStatRpc(&RssdWork._unmodeled_0a0[0x80]) != 0)
        ;
    w = &RssdWork;
    if (*(int *)&w->_unmodeled_0a0[0xa4] == 0) {
        printf(D_004D4A60, 0x900);
        return;
    }
    w->flags = 1;
    sceSifSetRpcQueue(w->rpc_queue, GetThreadId());
    sceSifRegisterRpc(w->rpc_server, 0x901, RssdSifRpcServer,
                      *(void **)&w->_unmodeled_038[0x6c], 0, 0, w->rpc_queue);
    request.arg[0].value = *(int *)w->_unmodeled_00c;
    RssdCallFunc(1, &request, 0, 0);
}

INCLUDE_ASM("asm/main/nonmatchings/ssd_init", RssdSifRpcServer);

extern int SignalSema(int sema_id);
typedef struct RssdWorkView {
    int flags;
    int pad4;
    int a;
    int pad0c;
    unsigned short b;
    short c;
    void *d;
    char pad18[0x1a0];
    int sema;
} RssdWorkView;
typedef union RssdBusyReq {
    struct {
        int pad[4];
        int a;
        int pad14;
        unsigned short b;
        short c;
        void *d;
        } f;
    char raw[0x20];
} RssdBusyReq;
#define W (*(RssdWorkView *)&RssdWork)

void RssdBusy(RssdBusyReq *arg0)
{
    RssdBusyReq *req = arg0;
    unsigned short b;
    short c;
    int d;

    W.flags |= 2;
    W.a = req->f.a;
    c = req->f.c;
    d = req->f.d;
    b = req->f.b;
    W.b = b;
    W.c = c;
    W.d = d;
    SignalSema(W.sema);
}

/* SIF RPC middleware bulk memory copy. */
extern void SsdCopyMemory(void *dst, void *src, int size);

/*
 * Copies one streamed sample chunk (the payload following the RssdRequest
 * header) to the running SPU write pointer and advances it by the copied
 * byte count; clears the streaming-busy flag once the request reports no
 * more data is coming.
 */
void RssdSpuRead(RssdRequest *request)
{
    int size;
    int more_data;

    size = request->arg[1].value;
    more_data = request->arg[2].value;
    SsdCopyMemory(RssdWork.spu_write_ptr, request + 1, size);
    RssdWork.spu_write_ptr += size;
    if (more_data == 0)
        RssdWork.flags &= ~4;
}

void RssdBackgroundNextWave(const int *request)
{
    RssdWork.flags &= ~4;
    if (request[4] >= 0)
        WakeupThread(RssdWork.next_wave_thread_id);
}

void RssdBackNextWaveThread(void)
{
    RssdRequest request;
    RssdWorkFlags *work;
    unsigned char *write_ptr;
    int remaining_size;
    int remaining_after_transfer;
    int maximum_transfer_size;
    int transfer_size;

    work = &RssdWork;
    maximum_transfer_size = 0x10000;
    for (;;) {
        SleepThread();
        remaining_size = work->spu_bytes_remaining;
        if (remaining_size == 0)
            continue;

        transfer_size = remaining_size <= maximum_transfer_size ? remaining_size : maximum_transfer_size;
        write_ptr = work->spu_write_ptr;
        remaining_after_transfer = remaining_size - transfer_size;
        work->spu_write_ptr = write_ptr + transfer_size;
        request.arg[2].value = remaining_after_transfer == 0;
        work->flags |= 0x24;
        work->spu_bytes_remaining = remaining_after_transfer;
        request.arg[0].pointer = write_ptr;
        request.arg[1].value = transfer_size;
        RssdCallFunc(33, &request, write_ptr, transfer_size);
    }
}

extern int WaitSema(int sema_id);
extern void FlushCache(int mode);
extern int sceSifCallRpc(void *cd, unsigned int request, unsigned int mode, void *send, int ssize, void *receive, int rsize, void *end_func, void *end_param);
extern const char D_004D4A80[];

int RssdCallFunc(int command, RssdRequest *request, void *data, int size)
{
    RssdRpcResponse *buf;
    int total;
    int ret;

    WaitSema(RssdWork.sema_id);
    if (data == 0)
        size = 0;
    RssdWork.flags |= 8;
    RssdWork.response.error_code = -1;
    buf = RssdWork.response_source;
    if (request != 0)
        *buf = *(RssdRpcResponse *)request;
    total = ((size + 0xF) & ~0xF) + 0x20;
    buf->error_code = command;
    buf->_unmodeled_0c[0] = size;
    if (total > 0x10020) {
        printf(D_004D4A80);
        RssdWork.flags &= ~8;
        SignalSema(RssdWork.sema_id);
        return -1;
    }
    if (size != 0)
        SsdCopyMemory(buf + 1, data, size);
    FlushCache(0);
    ret = sceSifCallRpc(&RssdWork._unmodeled_0a0[0x80], command, 1, buf, total, buf, 0x20, RssdSifRpcCallback, &RssdWork);
    if (ret < 0) {
        RssdWork.flags &= ~8;
        SignalSema(RssdWork.sema_id);
    } else {
        RssdFuncCallCompleted(1);
    }
    return ret;
}

/*
 * SIF RPC end callback for the RSSD work area: clears flag bit 3, copies the
 * 32-byte receive record into RssdWork, reports a status (-1 on a nonzero
 * result, else flag bit 5) through the registered one-shot completion
 * callback, then signals the waiting thread's semaphore.
 *
 * It runs from the SIF RPC interrupt context, so it ends with the
 * ee-interrupt-handler-return primitive: `sync.l`
 * orders every earlier load/store, including the iSignalSema effects, before
 * `ei` re-enables interrupts on the way out.
 */
void RssdSifRpcCallback(void)
{
    int status;

    RssdWork.flags &= ~0x8;
    RssdWork.response = *RssdWork.response_source;

    if (RssdWork.response.result != 0)
        status = -1;
    else
        status = (RssdWork.flags >> 5) & 1;

    if (RssdWork.complete_callback != 0)
        RssdWork.complete_callback(status, &RssdWork.response, RssdWork.callback_arg);

    RssdWork.complete_callback = 0;
    iSignalSema(RssdWork.sema_id);
    __asm__ __volatile__("sync.l" : : : "memory");
    __asm__ __volatile__("ei" : : : "memory");
    return;
}

void RssdFuncCallCompleted(int status)
{
  int result;
  int next;
  long code;

  do {
    if (next) {
      result = RssdGetCallCompletedCode();
      next = result;
      result = next;
      code = result;
    } else {
      result = RssdGetCallCompletedCode();
      next = result;
      result = next;
      code = result;
    }
  } while (status != 0 && (unsigned short)(code != 0));
}

/* Returns the bit RssdSifRpcCallback clears on RPC completion. */
int RssdGetCallCompletedCode(void)
{
    return (RssdWork.flags >> 3) & 1;
}

extern int printf(const char *format, ...);
const char D_004D4AA0[] = "Rssd get result error !\n";

int SsdGetResultValue(int *value)
{
    int result;

    if (RssdWork.flags & 0x8) {
        result = (RssdWork.flags & RSSD_FLAG_SUCCESS) ? -1 : -2;
    } else if (RssdWork.response.error_code >= 0) {
        result = (RssdWork.flags & RSSD_FLAG_SUCCESS) == 0;
        if (value != 0)
            *value = RssdWork.response.value;
    } else {
        result = -3;
        printf(D_004D4AA0);
    }
    return result;
}

int SsdGetResultParam(RssdRpcResponse *param)
{
    int result;

    if (RssdWork.flags & 0x8) {
        result = (RssdWork.flags & RSSD_FLAG_SUCCESS) ? -1 : -2;
    } else if (RssdWork.response.error_code >= 0) {
        result = (RssdWork.flags & RSSD_FLAG_SUCCESS) == 0;
        *param = RssdWork.response;
    } else {
        result = -3;
    }
    return result;
}

void SsdSetServerCallback(void *callback, void *arg)
{
    RssdWork.server_callback = callback;
    RssdWork.server_callback_arg = arg;
}

void SsdSetFuncCallback(void (*callback)(int status, RssdRpcResponse *response,
                                         void *arg),
                        void *arg)
{
    RssdWork.complete_callback = callback;
    RssdWork.callback_arg = arg;
}

void SsdSetStreamEndCallback(void *callback, void *arg)
{
    RssdWork.stream_end_callback = callback;
    RssdWork.stream_end_callback_arg = arg;
}

void SsdResume(void)
{
    RssdRequest request;

    RssdWork.flags &= ~RSSD_FLAG_SUCCESS;
    RssdCallFunc(RSSD_CMD_RESUME, &request, 0, 0);
}

void SsdSuspend(void)
{
    RssdRequest request;

    RssdWork.flags &= ~RSSD_FLAG_SUCCESS;
    RssdCallFunc(RSSD_CMD_SUSPEND, &request, 0, 0);
}
