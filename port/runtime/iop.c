/*
 * SIF and a model of the IOP side.
 *
 * There is no IOP: sceSifLoadModule "loads" every module, sceSifCallRpc
 * answers synchronously, and the modules the game talks to are modelled just
 * far enough that the EE side sees the replies and the IOP -> EE calls the
 * real modules make.  Calls the IOP makes into EE RPC servers are queued and
 * served by the EE thread parked in sceSifRpcLoop (or by a poller using
 * sceSifGetNextRequest/sceSifExecRequest), as with the real SIF.
 *
 * RSSD protocol (src/audio/rssd = the IOP module, src/main/ssd_init.c and
 * ssd_1.c = the EE library), see port/docs/STAGE4_BOOT.md:
 *   EE -> IOP  rpc 0x900, fno = command, mode 1 (nowait).  send = recv = one
 *              32-byte packet {s16 command, s16 error, int param, int value,
 *              int data_size, int arg[4]} followed by data_size bytes.  The
 *              IOP runs the command and writes the packet back (error = the
 *              SSD function error code, value/args = results).
 *   IOP -> EE  rpc 0x901, fno = command, same packet layout:
 *              0x1000 init done   (arg0 runtime buffer, arg1 size, arg2 0x6001f4, arg3 0x10250)
 *              0x1b   SPU read chunk (arg1 bytes follow, arg2 = bytes still to come)
 *              0x25   SPU DMA of a wave chunk finished (arg0 status, < 0 = error)
 *              0xda/0xdb sampling DMA done / key-off, 0x0a stream end (arg0 channel mask)
 *              0x102/0x103 voice envelope / sequence play info (answers to 0x102/0x103)
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include "xsg_rt.h"

extern int WakeupThread(int id);
extern int GetThreadId(void);
extern int SleepThread(void);

typedef struct {
    struct { void *paddr; unsigned pid; int tid; unsigned mode; } rpcd;
    unsigned command;
    void *buff, *cbuff;
    void (*end_function)(void *);
    void *end_param;
    void *serve;
} sceSifClientData;
static int dummy_serve[16];

void sceSifInitRpc(int mode) { STUB_LOG("mode=%d", mode); }
int sceSifRebootIop(const char *img) { STUB_LOG("%s", img); return 1; }
int sceSifSyncIop(void) { return 1; }
int sceSifInitIopHeap(void) { STUB_LOG(""); return 0; }
int sceSifLoadFileReset(void) { STUB_LOG(""); return 0; }
int sceSifLoadModule(const char *path, int alen, const char *args)
{
    xsg_logf("[sif] LoadModule %s (args %d bytes) -> pretend loaded\n", path, alen);
    (void)args;
    return 1;
}
static unsigned iop_heap = 0x00100000;
void *sceSifAllocIopHeap(unsigned size)
{
    unsigned r = iop_heap;
    iop_heap += (size + 63) & ~63u;
    STUB_LOG("%u -> 0x%x", size, r);
    return (void *)(uintptr_t)r;
}
int sceSifFreeIopHeap(void *p) { (void)p; return 0; }
int sceSifBindRpc(sceSifClientData *c, unsigned rpc, int mode)
{
    STUB_LOG("client=%p rpc=0x%x mode=%d", (void *)c, rpc, mode);
    c->serve = dummy_serve;
    c->command = rpc; /* remember the bound server id (host-only use) */
    c->rpcd.paddr = dummy_serve;
    return 0;
}
int sceSifCheckStatRpc(void *rd) { (void)rd; return 0; }

/* ---- EE-side RPC servers and the IOP -> EE queue -------------------- */
static struct { unsigned rpc; void *(*fn)(unsigned, void *, int); void *buf; } servers[8];
static int nservers;
static int loop_thread = -1;

#define QMAX 64
#define QDATA 0x400
struct iop_req {
    unsigned rpc, command;
    int size;
    unsigned char data[QDATA] __attribute__((aligned(16)));
};
static struct iop_req queue[QMAX];
static int qhead, qtail;

static void iop_call_ee(unsigned rpc, unsigned command, const void *data, int size)
{
    if ((qtail + 1) % QMAX == qhead) {
        xsg_logf("[iop] queue full, dropped 0x%x/%u\n", rpc, command);
        return;
    }
    struct iop_req *r = &queue[qtail];
    r->rpc = rpc;
    r->command = command;
    r->size = size > QDATA ? QDATA : size;
    memset(r->data, 0, sizeof r->data);
    if (data)
        memcpy(r->data, data, r->size);
    qtail = (qtail + 1) % QMAX;
    STUB_LOG("-> EE server 0x%x command 0x%x", rpc, command);
    if (loop_thread > 0)
        WakeupThread(loop_thread);
}

static void exec_req(struct iop_req *r)
{
    for (int i = 0; i < nservers; i++)
        if (servers[i].rpc == r->rpc) {
            static unsigned char scratch[QDATA] __attribute__((aligned(16)));
            void *buf = servers[i].buf ? servers[i].buf : scratch;
            memcpy(buf, r->data, r->size);
            servers[i].fn(r->command, buf, r->size);
            return;
        }
    xsg_logf("[iop] no EE server 0x%x for command 0x%x\n", r->rpc, r->command);
}

static struct iop_req *dequeue(void)
{
    if (qhead == qtail)
        return NULL;
    struct iop_req *r = &queue[qhead];
    qhead = (qhead + 1) % QMAX;
    return r;
}

void sceSifSetRpcQueue(void *q, int tid) { STUB_LOG("%p %d", q, tid); }
void sceSifRegisterRpc(void *s, unsigned n, void *f, void *b, void *cf, void *cb, void *q)
{
    STUB_LOG("rpc=0x%x fn=%p buf=%p", n, f, b);
    (void)s; (void)cf; (void)cb; (void)q;
    if (nservers < 8) {
        servers[nservers].rpc = n;
        servers[nservers].fn = (void *(*)(unsigned, void *, int))f;
        servers[nservers].buf = b;
        nservers++;
    }
}
void *sceSifRemoveRpc(void *s, void *q) { (void)s; (void)q; return NULL; }
/* The game polls these from SsdSpuDmaCompleted(1). The request is copied
 * out of the ring so a nested enqueue cannot overwrite it. */
void *sceSifGetNextRequest(void *q)
{
    (void)q;
    static struct iop_req cur;
    struct iop_req *r = dequeue();
    if (!r)
        return NULL;
    cur = *r;
    return &cur;
}
void sceSifExecRequest(void *s) { exec_req((struct iop_req *)s); }
void sceSifRpcLoop(void *q)
{
    STUB_LOG("%p: serving IOP -> EE calls", q);
    loop_thread = GetThreadId();
    for (;;) {
        void *r;
        while ((r = sceSifGetNextRequest(q)) != NULL)
            sceSifExecRequest(r);
        SleepThread();
    }
}

/* ---- RSSD (sound driver) model -------------------------------------- */
typedef struct {
    short command, error;
    int param, value, data_size;
    int arg[4];
} rssd_packet;

static int rssd_effect_banks, rssd_wave_banks;

static void rssd_notify(unsigned command, int a0, int a1, int a2, int a3)
{
    rssd_packet p = {0};
    p.command = (short)command;
    p.arg[0] = a0; p.arg[1] = a1; p.arg[2] = a2; p.arg[3] = a3;
    iop_call_ee(0x901, command, &p, sizeof p);
}

/* Runs the command the way RssdExecFunc would (results only, no sound) and
 * returns the IOP -> EE calls it causes through rssd_notify. */
void xsg_vagstream_init(void);
void xsg_vagstream_play(int pitch);
void xsg_vagstream_stop(void);
void xsg_vagstream_data(const void *data, int size);
int xsg_vagstream_status(void);

static void rssd_exec(unsigned fno, rssd_packet *p, const void *data)
{
    p->error = 0;
    switch (fno) {
    case 0x01: /* RssdInit: SsdInit, then the 0x1000 "ready" notification */
        rssd_notify(0x1000, 0x00180000, p->arg[0], 0x6001f4, 0x10250);
        break;
    case 0x1b: /* SPU direct read: one empty chunk, nothing more to come */
        rssd_notify(0x1b, p->arg[0], 0, 0, 0);
        break;
    case 0x20: /* add wave data: bank id, then the SPU DMA end callback */
        p->value = rssd_wave_banks++;
        rssd_notify(0x25, 0, 0, 0, 0);
        break;
    case 0x21: /* next wave chunk */
        p->value = rssd_wave_banks - 1;
        rssd_notify(0x25, 0, 0, 0, 0);
        break;
    case 0x30: case 0x31: case 0x38: /* add sequence / seq-effect data */
    case 0x60:                       /* add effect data */
        p->value = fno == 0x60 ? rssd_effect_banks++ : 0;
        p->arg[0] = 0x100000; /* free memory */
        p->arg[1] = 1;        /* block count */
        break;
    case 0xd2: /* transfer sampling: DMA done at once */
        rssd_notify(0xda, 0, 0, 0, 0);
        break;
    case 0x80: case 0x81: case 0x82: /* check play effect: nothing playing */
    case 0x24: case 0x62:            /* check wave/effect data: present */
    /* stereo VAG stream (audio.c) */
    case 0xb0: xsg_vagstream_init(); break;
    case 0xb2: case 0xb4: xsg_vagstream_stop(); break;
    case 0xb3: xsg_vagstream_play(p->arg[2]); break;
    case 0xb5:
        if (data && p->data_size > 0)
            xsg_vagstream_data(data, p->data_size);
        break;
    case 0xb7: p->value = xsg_vagstream_status(); break;
    case 0xa5: case 0xb8: case 0xbe: /* stream status: idle */
        p->value = 0;
        break;
    case 0x100: /* check SPU memory */
        p->value = 0x100000;
        break;
    default:
        break; /* acknowledged, no result */
    }
}

static void iop_module_call(unsigned rpc, unsigned fno, void *send, int ssize, void *recv, int rsize)
{
    if (rpc == 0x900 && send && ssize >= (int)sizeof(rssd_packet)) {
        rssd_packet p;
        memcpy(&p, send, sizeof p);
        STUB_LOG("rssd cmd 0x%x args %x %x %x %x data %d", fno, p.arg[0], p.arg[1], p.arg[2],
                 p.arg[3], p.data_size);
        rssd_exec(fno, &p, ssize > (int)sizeof p ? (const char *)send + sizeof p : NULL);
        if (recv && rsize > 0)
            memcpy(recv, &p, rsize < (int)sizeof p ? rsize : (int)sizeof p);
        return;
    }
    if (recv && rsize > 0)
        memset(recv, 0, rsize);
}

int sceSifCallRpc(sceSifClientData *c, unsigned fno, unsigned mode, void *send, int ssize,
                  void *recv, int rsize, void (*end)(void *), void *ep)
{
    STUB_LOG("client=%p rpc=0x%x fno=0x%x mode=%u send=%d recv=%d", (void *)c, c->command, fno,
             mode, ssize, rsize);
    /* the IOP's queued calls to the EE go out after the reply, so hold the
     * loop thread's wakeup until the end callback has run */
    int lt = loop_thread;
    loop_thread = -1;
    iop_module_call(c->command, fno, send, ssize, recv, rsize);
    if (end)
        end(ep);
    loop_thread = lt;
    if (qhead != qtail && loop_thread > 0)
        WakeupThread(loop_thread);
    return 0;
}
