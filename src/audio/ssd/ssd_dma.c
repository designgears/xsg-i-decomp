typedef void (*SsdDmaCallback)(int, void *);
typedef struct SsdDmaBuffer SsdDmaBuffer;
struct SsdDmaBuffer {
    unsigned char operation;
    unsigned char unmodeled_001[3];
    unsigned int progress;
    void *memory;
    unsigned int spu_address;
    unsigned int size;
    SsdDmaCallback callback;
    void *callback_argument;
    /* The DMA interrupt and producer share the queue links. */
    SsdDmaBuffer *volatile next;
};
typedef struct SsdDmaWork {
    unsigned int flags;
    unsigned char unmodeled_004[344];
    unsigned int wave_interrupt_count;
    unsigned int stream_interrupt_count;
    unsigned int irq_interrupt_count;
    unsigned char unmodeled_168[20];
    SsdDmaBuffer *buffers;
    SsdDmaBuffer *queue;
    unsigned char unmodeled_184[16];
    SsdDmaCallback pcm_callback;
    void *pcm_argument;
    SsdDmaCallback vag_callback;
    void *vag_argument;
    SsdDmaCallback irq_callback;
    void *irq_argument;
} SsdDmaWork;
extern SsdDmaWork SsdWork;
extern void *SsdNewPtr(unsigned int);
extern void CpuDisableIntr(void);
extern void CpuEnableIntr(void);
extern void SsdSeqErrorProc(int, void *);
extern int sceSdVoiceTrans(int, int, void *, unsigned int, unsigned int);
void SsdResetDmaManager(void);
void SsdDmaStartBuffer(SsdDmaWork *);
SsdDmaBuffer *SsdGetDmaQueueBuffer(SsdDmaWork *);
void SsdDmaStartTransfer(SsdDmaWork *, unsigned int, void *, unsigned int,
                         SsdDmaCallback, void *, int);

void SsdInitDmaManager(void)
{
    SsdWork.buffers = SsdNewPtr(256);
    SsdResetDmaManager();
}
void SsdSetPcmStreamCallback(SsdDmaCallback callback)
{
    SsdWork.pcm_callback = callback;
}
void SsdSetVagStreamCallback(SsdDmaCallback callback)
{
    SsdWork.vag_callback = callback;
}
void SsdSetIrqCallback(SsdDmaCallback callback)
{
    SsdWork.irq_callback = callback;
}
void SsdSpuDmaWrite(unsigned int spu_address, void *memory, unsigned int size,
                    SsdDmaCallback callback, void *argument)
{
    SsdDmaStartTransfer(&SsdWork, spu_address, memory, size, callback, argument, 1);
}
void SsdSpuDmaRead(unsigned int spu_address, void *memory, unsigned int size,
                   SsdDmaCallback callback, void *argument)
{
    SsdDmaStartTransfer(&SsdWork, spu_address, memory, size, callback, argument, 2);
}
void SsdDmaStartTransfer(SsdDmaWork *work, unsigned int spu_address,
                         void *memory, unsigned int size,
                         SsdDmaCallback callback, void *argument, int operation)
{
    unsigned short flags = work->flags;
    SsdDmaBuffer *buffer = SsdGetDmaQueueBuffer(work);
    SsdDmaBuffer *tail;
    if (!(flags & 0x30)) {
        while (!buffer)
            buffer = SsdGetDmaQueueBuffer(work);
        CpuDisableIntr();
    }
    if (!buffer) {
        SsdSeqErrorProc(-225, memory);
        return;
    }
    buffer->operation = operation & 15;
    buffer->progress = 0;
    buffer->memory = memory;
    buffer->spu_address = spu_address;
    buffer->size = size;
    buffer->callback = callback;
    buffer->next = 0;
    if (!work->queue) {
        work->queue = buffer;
        SsdDmaStartBuffer(work);
    } else {
        tail = work->queue;
        while (tail->next)
            tail = tail->next;
        tail->next = buffer;
    }
    if (!(flags & 0x30))
        CpuEnableIntr();
}
int SsdSpuDmaCompleted(int wait)
{
    if (wait & 1) {
        /* The DMA completion interrupt clears bit 1 while this thread polls. */
        volatile unsigned short *status = (volatile unsigned short *)&SsdWork.flags;
        while (*status & 2)
            ;
    }
    if (SsdWork.queue != 0) {
        return SsdWork.queue->operation;
    }
    return 0;
}
SsdDmaBuffer *SsdGetDmaQueueBuffer(SsdDmaWork *work)
{
    SsdDmaBuffer *buffer = work->buffers;
    int i;
    for (i = 0; i < 6; i++, buffer++) {
        if (!buffer->operation)
            return buffer;
    }
    return 0;
}
int SsdWaveTransferInterruptProcess(int channel, void *argument)
{
    SsdDmaWork *work = argument;
    SsdDmaBuffer *buffer;
    work->wave_interrupt_count++;
    work->flags |= 0x10;
    buffer = work->queue;
    if (buffer) {
        if (buffer->callback)
            buffer->callback(2, buffer->callback_argument);
        buffer->operation = 0;
        work->flags &= ~2;
        work->queue = buffer->next;
        if (work->queue)
            SsdDmaStartBuffer(work);
    }
    work->flags &= ~0x10;
    return 0;
}
int SsdStreamInterruptProcess(int channel, void *argument)
{
    SsdDmaWork *work = argument;
    work->stream_interrupt_count++;
    work->flags |= 0x10;
    if (work->pcm_callback)
        work->pcm_callback(channel, work->pcm_argument);
    work->flags &= ~0x10;
    return 0;
}
void SsdDmaStartBuffer(SsdDmaWork *work)
{
    SsdDmaBuffer *buffer = work->queue;
    work->flags |= 2;
    switch (buffer->operation) {
    case 0:
        break;
    case 1:
        sceSdVoiceTrans(0, 0, buffer->memory, buffer->spu_address, buffer->size);
        break;
    case 2:
        sceSdVoiceTrans(0, 1, buffer->memory, buffer->spu_address, buffer->size);
        break;
    }
}
int SsdIrqInterruptProcess(int channel, void *argument)
{
    SsdDmaWork *work = argument;
    channel--;
    work->flags |= 0x20;
    work->irq_interrupt_count++;
    if (work->irq_callback)
        work->irq_callback(channel, work->irq_argument);
    work->flags &= ~0x20;
    return 0;
}
void SsdResetDmaManager(void)
{
    SsdDmaBuffer *buffer = SsdWork.buffers;
    int i;
    for (i = 7; i >= 0; i--, buffer++)
        buffer->operation = 0;
    SsdWork.queue = 0;
    SsdWork.flags &= ~2;
}
