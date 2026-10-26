/* Original RSSD rssdfunc.c translation unit. */

typedef struct SsdWaveBank {
    unsigned char unmodeled_000[12];
    unsigned int metadata_00c;
    unsigned int metadata_010;
} SsdWaveBank;

typedef struct RssdWaveCommand {
    int command;
    int callback;
    int result;
    int data_size;
    SsdWaveBank *bank;
    unsigned int metadata_014;
    unsigned int metadata_018;
    int parameter3;
    unsigned char data[0];
} RssdWaveCommand;

extern void SsdMemoryCopy(void *, const void *, unsigned int);

extern void SsdSetWaveTransCallback(void (*)(int), void *);

extern int SsdAddWaveData(void *, unsigned int, unsigned int);

extern SsdWaveBank *SsdGetWSPtrBank(int);

extern void RssdSpuDmaTransferCallback(int);

typedef struct RssdCommand {
    int command;
    int callback;
    int result;
    int data_size;
    int parameter[4];
    unsigned char data[0];
} RssdCommand;

extern int SsdPlaySequence(void *, int, int);

extern int SsdAddSequenceData(void *);

extern int SsdAddSeqEffectData(void *);

extern int SsdAddEffectData(void *);

extern int SsdGetMemoryFreeSize(void);

extern int SsdGetMemoryBlockCount(void);

extern void SsdInitSampling(int, int, int);

extern void SsdSetSampleDmaCallback(void (*)(int));

extern void SsdSetSampleKeyoffCallback(void (*)(int));

extern void SsdTransferSampling(int, void *, int);

extern void RssdCallFunc(int, void *, void *, int);

extern void iRssdCallFunc(int, void *, void *, int);

void RssdSamplingDmaCallbackProc(int);

void RssdSamplingKeyoffCallbackProc(int);

extern void SsdSpuDmaRead(unsigned int, void *, unsigned int, void (*)(int), void *);

extern void RssdSpuReadCallback(int);

typedef struct RssdInitNotification {
    int command;
    int callback;
    int result;
    int data_size;
    void *runtime_buffer;
    int runtime_size;
    unsigned int protocol_value2;
    unsigned int protocol_value3;
} RssdInitNotification;

typedef struct RssdRpcClient {
    unsigned char unmodeled_000[36];
    void *server;
    unsigned char unmodeled_028[32];
} RssdRpcClient;

extern int sceSdInit(int);

extern void CpuDisableIntr(void);

extern void CpuEnableIntr(void);

extern void *AllocSysMemory(int, int, void *);

extern void SsdInit(void *, int, int);

extern int sceSifBindRpc(RssdRpcClient *, unsigned int, int);

extern int sceSifCheckStatRpc(RssdRpcClient *);

typedef struct RssdReadCommand {
    int command;
    int callback;
    int result;
    int data_size;
    unsigned int spu_address;
    int total_size;
    int parameter2;
    int parameter3;
} RssdReadCommand;

extern int printf(const char *, ...);

extern void RssdSetFuncCallback(void (*)(void *), void *);

extern void RssdSpuReadNext(void *);

extern int SsdNextWaveData(void *, unsigned int, unsigned int);

/* The transfer words are shared by SPU reads and wave uploads. */
typedef struct RssdWorkState {
    unsigned char unmodeled_000[128];
    RssdRpcClient client;
    union {
        struct {
            RssdWaveCommand *transfer_buffer;
            unsigned int spu_address;
            int chunk_size;
            int remaining;
        } read;
        unsigned int parameter[4];
    } transfer;
} RssdWorkState;
extern RssdWorkState RssdWork;

void RssdInit(RssdCommand *request)
{
    int size;
    void *runtime;
    RssdInitNotification notification;
    sceSdInit(0);
    size = request->parameter[0];
    CpuDisableIntr();
    runtime = AllocSysMemory(0, size, 0);
    RssdWork.transfer.read.transfer_buffer = AllocSysMemory(0, 0x10020, 0);
    CpuEnableIntr();
    SsdInit(runtime, size, 0);
    sceSifBindRpc(&RssdWork.client, 0x901, 0);
    while (sceSifCheckStatRpc(&RssdWork.client))
        ;
    if (RssdWork.client.server) {
        notification.runtime_buffer = runtime;
        notification.runtime_size = size;
        notification.protocol_value2 = 0x6001f4;
        notification.protocol_value3 = 0x10250;
        RssdCallFunc(0x1000, &notification, 0, 0);
    }
}

void RssdSpuDirectRead(RssdReadCommand *request)
{
    int total = request->total_size;
    unsigned int address = request->spu_address;
    int size = total;
    if (size > 0x10000)
        size = 0x10000;
    SsdSpuDmaRead(address, RssdWork.transfer.read.transfer_buffer, size, RssdSpuReadCallback, 0);
    RssdWork.transfer.read.spu_address = address;
    RssdWork.transfer.read.chunk_size = size;
    RssdWork.transfer.read.remaining = total - size;
    printf("Rssd spu read  SpuPtr=%06x  TransSize=%x\n", address, total);
}

void RssdSpuReadCallback(int status)
{
    RssdCommand request;
    request.parameter[0] = RssdWork.transfer.read.spu_address;
    request.parameter[1] = RssdWork.transfer.read.chunk_size;
    request.parameter[2] = RssdWork.transfer.read.remaining;
    if (status == -2) {
        RssdCallFunc(27, &request, 0, 0);
    } else {
        RssdSetFuncCallback(RssdSpuReadNext, &RssdWork);
        iRssdCallFunc(27, &request, RssdWork.transfer.read.transfer_buffer, RssdWork.transfer.read.chunk_size);
    }
}

void RssdSpuReadNext(void *argument)
{
    unsigned int size;
    unsigned int *remaining_word = &RssdWork.transfer.parameter[3];
    int remaining = *remaining_word;
    unsigned int address;
    if (!remaining)
        return;
        address = remaining_word[-2] + remaining_word[-1];
        RssdWork.transfer.parameter[1] = address;
        if (remaining <= 0x10000)
            size = remaining;
        else
            size = 0x10000;
        {
            unsigned int *counts = remaining_word - 1;
            remaining_word[-1] = size;
            counts[1] -= size;
            SsdSpuDmaRead(address, (void *)counts[-2],
                          size, RssdSpuReadCallback, 0);
        }
}

void RssdAddWaveData(RssdWaveCommand *request)
{
    unsigned char *data;
    int bank_id;
    SsdWaveBank *bank;
    SsdMemoryCopy(RssdWork.transfer.read.transfer_buffer, request, request->data_size + 32);
    data = RssdWork.transfer.read.transfer_buffer->data;
    SsdSetWaveTransCallback(RssdSpuDmaTransferCallback, 0);
    bank_id = SsdAddWaveData(data, request->metadata_014, request->metadata_018);
    bank = 0;
    if (bank_id >= 0)
        bank = SsdGetWSPtrBank(bank_id);
    request->result = bank_id;
    request->bank = bank;
    if (bank) {
        request->metadata_014 = bank->metadata_00c;
        request->metadata_018 = bank->metadata_010;
    }
}

void RssdNextWaveData(RssdWaveCommand *request)
{
    unsigned char *data;
    int bank_id;
    SsdWaveBank *bank;
    SsdMemoryCopy(RssdWork.transfer.read.transfer_buffer, request, request->data_size + 32);
    data = RssdWork.transfer.read.transfer_buffer->data;
    SsdSetWaveTransCallback(RssdSpuDmaTransferCallback, 0);
    bank_id = SsdNextWaveData(data, request->metadata_014, request->metadata_018);
    bank = 0;
    if (bank_id >= 0)
        bank = SsdGetWSPtrBank(bank_id);
    request->result = bank_id;
    request->bank = bank;
    if (bank) {
        request->metadata_014 = bank->metadata_00c;
        request->metadata_018 = bank->metadata_010;
    }
}

void RssdSpuDmaTransferCallback(int status)
{
    RssdCommand request;
    request.parameter[0] = status;
    if (status == -2) {
        RssdCallFunc(37, &request, 0, 0);
    } else {
        iRssdCallFunc(37, &request, 0, 0);
    }
}

void RssdPlaySequenceData(RssdCommand *request)
{
    request->result = SsdPlaySequence(request->data, request->parameter[1], request->parameter[2]);
}

void RssdAddSequenceData(RssdCommand *request)
{
    request->result = SsdAddSequenceData(request->data);
    request->parameter[0] = SsdGetMemoryFreeSize();
    request->parameter[1] = SsdGetMemoryBlockCount();
}

void RssdAddSeqEffectData(RssdCommand *request)
{
    request->result = SsdAddSeqEffectData(request->data);
    request->parameter[0] = SsdGetMemoryFreeSize();
    request->parameter[1] = SsdGetMemoryBlockCount();
}

void RssdAddEffectData(RssdCommand *request)
{
    request->result = SsdAddEffectData(request->data);
    request->parameter[0] = SsdGetMemoryFreeSize();
    request->parameter[1] = SsdGetMemoryBlockCount();
}

void RssdInitSampling(RssdCommand *request)
{
    SsdInitSampling(request->parameter[0], request->parameter[1], request->parameter[2]);
    SsdSetSampleDmaCallback(RssdSamplingDmaCallbackProc);
    SsdSetSampleKeyoffCallback(RssdSamplingKeyoffCallbackProc);
}

void RssdTransferSampling(RssdCommand *request)
{
    SsdTransferSampling(request->parameter[0], request->data, request->parameter[2]);
}

void RssdTransferSamplingNext(void)
{
}

void RssdSamplingDmaCallbackProc(int status)
{
    RssdCommand request;
    request.parameter[0] = status;
    if (status == -2)
        RssdCallFunc(218, &request, 0, 0);
    else
        iRssdCallFunc(218, &request, 0, 0);
}

void RssdSamplingKeyoffCallbackProc(int status)
{
    RssdCommand request;
    request.parameter[0] = status;
    if (status == -2)
        RssdCallFunc(219, &request, 0, 0);
    else
        iRssdCallFunc(219, &request, 0, 0);
}
