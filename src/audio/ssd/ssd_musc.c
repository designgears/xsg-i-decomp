/* Original SSD object: ssd_musc.c. Partial layouts describe evidenced fields. */
typedef struct SsdMidiChannel {
    unsigned char unmodeled_000[252];
} SsdMidiChannel;

typedef struct SsdMidiDevice {
    unsigned char unmodeled_000[32];
    SsdMidiChannel channels[0];
} SsdMidiDevice;

typedef struct SsdMusicChunk {
    unsigned short type;
    unsigned short size;
    unsigned char trackParameters[2];
    unsigned char midiChannel;
    unsigned char attributes;
    unsigned char events[0];
} SsdMusicChunk;

typedef struct SsdMusicData {
    unsigned int signature;
    unsigned char unmodeled_004[4];
    int size;
    unsigned char unmodeled_00c[2];
    unsigned short fileId;
    unsigned char unmodeled_010[8];
    unsigned char priority;
    unsigned char unmodeled_019[5];
    unsigned char trackCount;
    unsigned char unmodeled_01f[2];
    unsigned char midiSettings[6];
    unsigned char midiChannelCount;
    SsdMusicChunk chunks[0];
} SsdMusicData;

typedef struct SsdTrackParameter {
    int value;
    unsigned char unmodeled_004[8];
    unsigned short remaining;
    unsigned char unmodeled_00e[2];
} SsdTrackParameter;

typedef struct SsdMusicTrack {
    unsigned short flags;
    unsigned short tickCount;
    unsigned char trackIndex;
    unsigned char midiChannel;
    unsigned char unmodeled_006;
    unsigned char attributes;
    unsigned char noteState;
    unsigned char program;
    unsigned short bank;
    int delta;
    unsigned char unmodeled_010[4];
    int timestamp;
    unsigned char *events;
    unsigned char *position;
    int eventState;
    SsdMidiChannel *channel;
    int gate;
    unsigned short noteDuration;
    unsigned char noteMode;
    unsigned char unmodeled_02f;
    int trackState[3];
    unsigned char priority;
    unsigned char baseKey;
    unsigned char currentKey;
    unsigned char previousKey;
    SsdTrackParameter pitch;
    SsdTrackParameter pan;
    SsdTrackParameter volume;
    SsdTrackParameter expression;
    unsigned char unmodeled_080[48];
} SsdMusicTrack;

typedef struct SsdSequence {
    unsigned int signature;
    struct SsdSequence *next;
    SsdMusicData *data;
    struct SsdMusicTrack *tracks;
    struct SsdMusicChunk *chunk4;
    struct SsdMidiDevice *midiDevice;
    char *metadata;
    int sequenceId;
    short flags;
    unsigned char unmodeled_022[2];
    unsigned short bank;
    unsigned char trackCount;
    unsigned char auxiliaryMode;
    unsigned char unmodeled_028;
    unsigned char priority;
    unsigned char controlState;
    unsigned char unmodeled_02b;
    unsigned char outputVolume;
    unsigned char isEffect;
    unsigned char unmodeled_02e[110];
    int playbackState;
    unsigned char unmodeled_0a0[28];
    int auxiliaryState[2];
} SsdSequence;

typedef struct SsdGlobalWork {
    unsigned char unmodeled_000[0x158];
    int timestamp;
    unsigned char unmodeled_15c[12];
    SsdSequence *sequenceHead;
    unsigned char unmodeled_16c[112];
    unsigned char sequenceMasterVolume, effectMasterVolume;
} SsdGlobalWork;

extern SsdSequence *SsdGetSequencePtr(int sequenceId);
/* The driver stores error details as a 32-bit integer diagnostic word. */
extern void SsdErrorProc(int error, int detail);
extern void SsdSetSeqMasterVolume(int sequenceId, int volume, int fade);
extern SsdGlobalWork SsdWork;
int SsdAddSequenceData(SsdMusicData *data);
extern void SsdSetMidiDeviceOutputVolume(struct SsdMidiDevice *device, int volume);
extern void *SsdGetWSPtrBank(int bank);
void SsdStartSequence(int sequenceId, int volume, int fade);
void SsdStopSequence(int sequenceId);
extern short SsdCheckSumMusicData(struct SsdMusicData *data);
extern void SsdInitSequence(SsdSequence *sequence);
void SsdInitMusicTrackRec(SsdSequence *sequence);
extern void SsdStoppingSequence(struct SsdSequence *sequence);
extern short SsdCheckData(struct SsdMusicData *data, unsigned int signature);
extern void SsdWarringProc(int error, int sequenceId);
extern void *SsdNewPtr(int size);
extern void SsdMemoryCopy(void *destination, const void *source, int size);
int SsdGetSeqWorkSize(SsdMusicData *data);
extern int SsdGetMemoryFreeSize(void);
extern void SsdDisposeSequenceBuffer(SsdSequence *sequence);
int SsdCheckMidiChannel(SsdSequence *sequence);
extern struct SsdMidiDevice *SsdNewMidiDevice(int channels, int flags, unsigned char *settings);
void SsdSetDataAddress(SsdSequence *sequence);
extern void SsdSetSeqReverb(SsdSequence *sequence);
extern void SsdLinkSequence(SsdSequence *sequence);
extern void SsdUnlinkSequence(SsdSequence *sequence);
void SsdResumeSequence(int sequenceId, int volume, int fade);
int SsdAddSeqEffectData(struct SsdMusicData *data);
int SsdPlaySequence(void *data, int volume, int fade);
int SsdCheckSequence(int sequenceId);
int SsdDisposeSequence(int sequenceId);
void SsdPauseSequence(int sequenceId, int fade);

int SsdPlaySequence(void *data, int volume, int fade)
{
    int sequenceId = SsdAddSequenceData(data);
    if (sequenceId >= 0) {
        SsdStartSequence(sequenceId, volume, fade);
    }
    return sequenceId;
}

int SsdAddSeqEffectData(struct SsdMusicData *data)
{
    int sequenceId = SsdAddSequenceData(data);
    if (sequenceId >= 0) {
        SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
        sequence->outputVolume = SsdWork.effectMasterVolume;
        sequence->isEffect = 1;
        SsdSetMidiDeviceOutputVolume(sequence->midiDevice, sequence->outputVolume);
    }
    return sequenceId;
}

int SsdAddSequenceData(SsdMusicData *data)
{
    SsdMusicData *source;
    SsdSequence *sequence;
    int sequenceId;
    int size;
    int channels;
    short error = SsdCheckData(data, 0x6d646d73);

    source = data;
    if (error != 0) {
        SsdErrorProc(error, (int)data);
        return error;
    }
    sequenceId = data->fileId;
    if (SsdGetSequencePtr(sequenceId) != 0) {
        SsdWarringProc(-25, sequenceId);
        return -25;
    }
    size = data->size;
    data = SsdNewPtr(size);
    SsdMemoryCopy(data, source, size);
    size = SsdGetSeqWorkSize(data);
    if (SsdGetMemoryFreeSize() < size) {
        SsdErrorProc(-32, (int)data);
        return -32;
    }
    sequence = SsdNewPtr(196);
    if (sequence == 0) {
        SsdErrorProc(-36, (int)data);
        return -36;
    }
    sequence->signature = 0x7165736d;
    sequence->next = 0;
    sequence->data = data;
    sequence->tracks = 0;
    sequence->tracks = SsdNewPtr(data->trackCount * 176);
    if (sequence->tracks == 0) {
        SsdErrorProc(-37, (int)data);
        SsdDisposeSequenceBuffer(sequence);
        return -37;
    }
    channels = data->midiChannelCount;
    if (channels == 0) {
        channels = SsdCheckMidiChannel(sequence);
    }
    sequence->midiDevice = SsdNewMidiDevice(channels, 0, data->midiSettings);
    if (sequence->midiDevice == 0) {
        SsdErrorProc(-40, (int)data);
        SsdDisposeSequenceBuffer(sequence);
        return -40;
    }
    SsdSetDataAddress(sequence);
    SsdInitSequence(sequence);
    SsdSetSeqReverb(sequence);
    sequence->priority = data->priority;
    sequence->controlState = 0;
    sequence->outputVolume = SsdWork.sequenceMasterVolume;
    sequence->isEffect = 0;
    SsdSetMidiDeviceOutputVolume(sequence->midiDevice, sequence->outputVolume);
    sequence->auxiliaryState[0] = 0;
    sequence->auxiliaryMode = 0;
    sequence->auxiliaryState[1] = 0;
    SsdLinkSequence(sequence);
    return sequenceId;
}

int SsdDisposeSequence(int sequenceId)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    short error;

    if (sequence == 0) {
        SsdErrorProc(-17, sequenceId);
        return -17;
    }
    error = SsdCheckData(sequence->data, 0x6d646d73);
    if (error != 0) {
        SsdErrorProc(error, sequenceId);
        return error;
    }
    if (sequence->flags & 0x8000) {
        SsdStopSequence(sequenceId);
    }
    SsdUnlinkSequence(sequence);
    SsdDisposeSequenceBuffer(sequence);
    return 0;
}

int SsdCheckSequence(int sequenceId)
{
    SsdSequence *sequence = SsdWork.sequenceHead;
    while (sequence != 0) {
        if (sequence->sequenceId == sequenceId) {
            return sequenceId;
        }
        sequence = sequence->next;
    }
    return -17;
}

void SsdStartSequence(int sequenceId, int volume, int fade)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    short error;

    if (sequence == 0) {
        SsdErrorProc(-5, 0);
        return;
    }
    if (sequence->flags & 0x8000) {
        SsdStopSequence(sequenceId);
    }
    error = SsdCheckSumMusicData(sequence->data);
    if (error != 0) {
        SsdErrorProc(error, (int)sequence->data);
        return;
    }
    SsdInitSequence(sequence);
    SsdInitMusicTrackRec(sequence);
    sequence->playbackState = 0;
    sequence->flags |= 0x100;
    SsdSetSeqMasterVolume(sequenceId, volume, fade);
}

void SsdResumeSequence(int sequenceId, int volume, int fade)
{
    if (SsdGetSequencePtr(sequenceId) == 0) {
        SsdErrorProc(-5, 0);
    } else {
        SsdSetSeqMasterVolume(sequenceId, volume, fade);
    }
}

void SsdStopSequence(int sequenceId)
{
    struct SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    if (sequence == 0) {
        SsdErrorProc(-5, 0);
    } else {
        SsdStoppingSequence(sequence);
    }
}

void SsdPauseSequence(int sequenceId, int fade)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    if (sequence == 0) {
        SsdErrorProc(-5, 0);
    } else if (!(sequence->flags & 0x100)) {
        SsdSetSeqMasterVolume(sequenceId, 0, fade);
    }
}

void SsdInitMusicTrackRec(SsdSequence *sequence)
{
    SsdMusicTrack *track = sequence->tracks;
    int index;
    int *timestamp;
    int initialPriority;
    int initialKey;
    int initialPitch;
    int initialVolume;
    int initialExpression;
    int initialProgram;
    SsdGetWSPtrBank(sequence->bank);
    index = 0;
    timestamp = &SsdWork.timestamp;
    initialPriority = 15;
    initialKey = 60;
    initialPitch = 0x600000;
    initialVolume = 0x640000;
    initialExpression = 0x3f0000;
    initialProgram = 255;

    do {
        unsigned short initialFlags = track->flags;
        track->timestamp = *timestamp;
        track->delta = 0;
        track->noteState = 0;
        track->flags = initialFlags | 0x8400 | track->attributes;
        track->tickCount = 0;
        track->bank = sequence->bank;
        track->position = track->events;
        track->eventState = 0;
        track->gate = 0;
        track->noteDuration = 0;
        track->noteMode = 0;
        track->trackState[0] = 0;
        track->trackState[1] = 0;
        track->trackState[2] = 0;
        track->priority = initialPriority;
        track->baseKey = initialKey;
        track->currentKey = initialKey;
        track->previousKey = initialKey;
        track->pitch.remaining = 0;
        track->pitch.value = initialPitch;
        track->pan.remaining = 0;
        track->pan.value = 0;
        track->volume.remaining = 0;
        track->volume.value = initialVolume;
        track->expression.remaining = 0;
        track->expression.value = initialExpression;
        track->program = initialProgram;
        track++;
        index++;
    } while (index < sequence->trackCount);
}

int SsdCheckMidiChannel(SsdSequence *sequence)
{
    int scanning = 1;
    int highestChannel = 0;
    SsdMusicChunk *chunk = sequence->data->chunks;

    do {
        if (chunk->type != 0) {
            if (chunk->type == 3) {
                if (highestChannel < chunk->midiChannel) {
                    highestChannel = chunk->midiChannel;
                }
            }
        } else {
            scanning = 0;
        }
        chunk = (SsdMusicChunk *)((unsigned char *)chunk + chunk->size);
    } while (scanning);
    return highestChannel + 1;
}

void SsdSetDataAddress(SsdSequence *sequence)
{
    int scanning = 1;
    int trackIndex = 0;
    SsdMusicChunk *chunk = sequence->data->chunks;
    SsdMusicTrack *track = sequence->tracks;

    do {
        switch (chunk->type) {
        case 2:
            sequence->metadata = (char *)chunk->trackParameters;
            break;
        case 3:
            track->trackIndex = trackIndex;
            track->events = chunk->events;
            track->midiChannel = chunk->midiChannel;
            track->channel = &sequence->midiDevice->channels[chunk->midiChannel];
            track->attributes = chunk->attributes;
            trackIndex++;
            track->flags = 0;
            track++;
            break;
        case 4:
            sequence->chunk4 = chunk;
            break;
        case 0:
            scanning = 0;
            break;
        }
        chunk = (SsdMusicChunk *)((unsigned char *)chunk + chunk->size);
    } while (scanning);
}

int SsdGetSeqWorkSize(SsdMusicData *data)
{
    if (data->signature != 0x6d646d73) {
        return -1;
    }
    return data->trackCount * 176 + 292;
}
