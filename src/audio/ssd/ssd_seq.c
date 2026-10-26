/* Original SSD object: ssd_seq.c. Partial layouts describe evidenced fields. */
typedef struct SsdMidiDevice SsdMidiDevice;
typedef void (*SsdSequenceStopCallback)();
typedef union SsdFixed16 {
    int raw;
    struct {
        unsigned short fraction;
        short integer;
    } parts;
} SsdFixed16;

typedef struct SsdSequenceParameter {
    int value;
    int step;
    int target;
    short remaining;
    unsigned char unmodeled_00e[2];
} SsdSequenceParameter;

typedef struct SsdTempoParameter {
    SsdFixed16 value;
    int step;
    int target;
    short remaining;
    unsigned char unmodeled_00e[2];
} SsdTempoParameter;

typedef struct SsdMusicData {
    unsigned char unmodeled_000[14];
    unsigned short fileId;
    unsigned char unmodeled_010[9];
    unsigned char options;
    unsigned char unmodeled_01a[2];
    unsigned short bank;
    unsigned char trackCount;
    unsigned char priority;
    unsigned char unmodeled_020;
    signed char note;
    signed char pan;
    unsigned char timingFormat;
    unsigned char timingPeriod;
    unsigned char timingParameters[2];
} SsdMusicData;

typedef struct SsdMidiChannel {
    unsigned char unmodeled_000[24];
    int effective_volume;
    unsigned char unmodeled_01c[4];
    short note_fader;
    unsigned char volume_fader;
    signed char panpot_fader;
    unsigned char unmodeled_024[5];
    signed char volume, expression;
} SsdMidiChannel;

typedef struct SsdMusicTrack {
    short flags;
    unsigned char unmodeled_002[5];
    unsigned char attributes;
    unsigned char unmodeled_008[28];
    struct SsdMidiChannel *channel;
    unsigned char unmodeled_028[136];
} SsdMusicTrack;

typedef struct SsdPatchChunk {
    unsigned short type;
    unsigned short size;
    unsigned char unmodeled_004[2];
    unsigned char patchCount;
    unsigned char unmodeled_007;
    unsigned short patchOffsets[0];
} SsdPatchChunk;

typedef struct SsdSequence {
    unsigned int signature;
    struct SsdSequence *next;
    SsdMusicData *data;
    struct SsdMusicTrack *tracks;
    struct SsdPatchChunk *patchChunk;
    struct SsdMidiDevice *midiDevice;
    char *metadata;
    int sequenceId;
    short flags;
    short state;
    unsigned short bank;
    unsigned char trackCount;
    unsigned char signal;
    unsigned char controlMode;
    unsigned char priority;
    unsigned char controlState;
    unsigned char options;
    unsigned char outputVolume;
    unsigned char isEffect;
    signed char note;
    signed char pan;
    SsdFixed16 bpm;
    unsigned char unmodeled_034[8];
    short sequenceCounter;
    unsigned char unmodeled_03e[2];
    int tickAccumulator;
    int tickRate;
    int timingState;
    unsigned int play_ticks, play_time;
    int randomState;
    short playbackState[4];
    short beatsPerMeasure;
    short ticksPerQuarter;
    short timeNumerator;
    short timeDenominator;
    unsigned char timingMode;
    unsigned char unmodeled_069[2];
    signed char timingFormat;
    short timingPeriod;
    unsigned short targetTimingPeriod;
    signed char timingParameters[2];
    unsigned char unmodeled_072[10];
    SsdTempoParameter masterTempo;
    SsdSequenceParameter masterNote;
    SsdSequenceParameter masterVolume;
    SsdSequenceParameter masterPan;
    SsdSequenceStopCallback stop_callback;
} SsdSequence;

typedef struct SsdGlobalWork {
    unsigned char unmodeled_000[0x168];
    SsdSequence *sequenceHead;
} SsdGlobalWork;

typedef struct SsdTimeCode {
    unsigned char unmodeled_000[2];
    unsigned short hours;
    unsigned char minutes, seconds, frames, subframes;
    unsigned int play_ticks;
    unsigned short ticks;
} SsdTimeCode;

typedef struct SsdTrackPatch {
    unsigned char unmodeled_000[2];
    unsigned char track;
    unsigned char unmodeled_003;
    unsigned short note;
    unsigned char volume;
    unsigned char panpot;
} SsdTrackPatch;

typedef struct SsdTrackPatchSet {
    unsigned char unmodeled_000[2];
    unsigned char count;
    unsigned char unmodeled_003;
} SsdTrackPatchSet;

SsdSequence *SsdGetSequencePtr(int sequenceId);
void SsdGetTimeCode(unsigned int ticks, SsdTimeCode *timeCode);
extern void SsdSetChannelVoiceEvent(SsdMidiChannel *channel, int events);
extern SsdGlobalWork SsdWork;
extern void CpuDisableIntr(void);
extern void CpuEnableIntr(void);
extern void SsdSetMidiDeviceNote(struct SsdMidiDevice *device, int note);
extern void SsdSetMidiDevicePriority(struct SsdMidiDevice *device, int priority, ...);
extern void SsdResetMidiDeviceParam(struct SsdMidiDevice *device, int bank);
extern void SsdResetChannelAll(struct SsdMidiDevice *device, int bank);
extern int SsdGetRSRandom(void);
extern void SsdSetMidiDevicePanpot(struct SsdMidiDevice *device, int pan);
extern void SsdCutoffChannel(struct SsdMidiChannel *channel);
extern void SsdSetMidiDeviceVolume(SsdMidiDevice *device, int volume);
void SsdStartingSequence(SsdSequence *sequence);
void SsdStoppingSequence(SsdSequence *sequence);
/* The driver stores error details as a 32-bit integer diagnostic word. */
extern void SsdErrorProc(int error, int detail);
extern void SsdResetChannelFader(SsdMidiDevice *device, int reset);
extern void SsdDisposePtr(void *pointer);
extern void SsdDisposeMidiDevice(SsdMidiDevice *device);
extern void SsdCutoffDevive(struct SsdMidiDevice *device);
extern void SsdSpuDmaCompleted(int wait);
extern int SsdSetReverbParameter(int mode, int depth, int delay, int feedback);
extern void SsdReverbClearCompleted(int core, int wait);
extern void SsdEnableReverbParam(int core);
void SsdSetSeqReverb(SsdSequence *sequence);
void SsdSetTrackPatchSet(int id, const SsdTrackPatchSet *patches);
void SsdGetSeqPlayTime(int sequence_id, SsdTimeCode *out);
void SsdSetSeqVolumeFader(int sequenceId, int trackIndex, int volume);
void SsdSetSeqPanpotFader(int id, int track, signed char fader);
void SsdLinkSequence(SsdSequence *sequence);
void SsdSetSeqStopCallback(int sequence_id, SsdSequenceStopCallback callback);
void SsdSetSeqMasterNote(int sequenceId, int note, int duration);
void SsdSetSeqMasterTempo(int id, int bpm, int ticks);
struct SsdSequence *SsdGetSeqLyric(int sequenceId);
struct SsdSequence *SsdSetSeqABRepeat(int sequenceId);
void SsdGetPlayMeasure(int sequence_id);
void SsdSetSeqNoteFader(int id, int track, short fader);
void SsdInitSequence(SsdSequence *sequence);
void SsdSetSeqMasterPanpot(int sequenceId, int pan, int duration);
void SsdSetSeqSignal(int sequenceId, int signal);
void SsdSetSeqMute(int sequenceId, int muteMask);
void SsdSetSeqMasterVolume(int id, int volume, int ticks);
int SsdUnlinkSequence(SsdSequence *sequence);
int SsdGetSeqCuePoint(void);
void SsdDisposeSequenceBuffer(SsdSequence *sequence);
int SsdGetSeqPlayStatus(int sequenceId);
void SsdSetSeqPatchNumber(int sequenceId, int patchNumber);

void SsdInitSequence(SsdSequence *sequence)
{
    SsdMusicData *data = sequence->data;
    sequence->state = 0;
    sequence->flags = 0;
    if (data != 0) {
        int note;
        int pan;
        sequence->sequenceId = data->fileId;
        sequence->bank = data->bank;
        sequence->trackCount = data->trackCount;
        note = data->note;
        pan = data->pan;
        sequence->note = note;
        sequence->pan = pan;
        sequence->timingFormat = data->timingFormat;
        sequence->timingPeriod = sequence->targetTimingPeriod = data->timingPeriod << 8;
        sequence->timingParameters[0] = data->timingParameters[0];
        sequence->timingParameters[1] = data->timingParameters[1];
        sequence->options = data->options;
        SsdSetMidiDevicePriority(sequence->midiDevice, data->priority, note, pan);
    } else {
        sequence->sequenceId = 0;
        sequence->trackCount = 0;
        sequence->bank = 0;
    }
    sequence->bpm.raw = 0x780000;
    sequence->controlMode = 0;
    sequence->sequenceCounter = 0;
    sequence->tickAccumulator = 0;
    sequence->tickRate = 25165;
    sequence->timingState = 0;
    sequence->play_ticks = 0;
    sequence->play_time = 0;
    sequence->playbackState[0] = 0;
    sequence->playbackState[1] = 0;
    sequence->playbackState[2] = 0;
    sequence->playbackState[3] = 0;
    sequence->beatsPerMeasure = 4;
    sequence->ticksPerQuarter = 96;
    sequence->timeNumerator = 4;
    sequence->timeDenominator = 4;
    sequence->timingMode = 0;
    sequence->masterTempo.value.raw = 0x1000000;
    sequence->masterTempo.remaining = 0;
    sequence->masterNote.value = 0;
    sequence->masterNote.remaining = 0;
    sequence->masterVolume.value = 0x7f0000;
    sequence->masterVolume.remaining = 0;
    sequence->masterPan.value = 0;
    sequence->masterPan.remaining = 0;
    SsdResetMidiDeviceParam(sequence->midiDevice, sequence->bank);
    SsdResetChannelAll(sequence->midiDevice, sequence->bank);
    sequence->randomState = SsdGetRSRandom();
}

void SsdLinkSequence(SsdSequence *sequence)
{
    CpuDisableIntr();
    sequence->next = SsdWork.sequenceHead;
    SsdWork.sequenceHead = sequence;
    CpuEnableIntr();
}

int SsdUnlinkSequence(SsdSequence *sequence)
{
    SsdSequence *current;
    SsdSequence *previous;

    CpuDisableIntr();
    current = SsdWork.sequenceHead;
    previous = 0;
    while (current != 0) {
        if (current == sequence) {
            break;
        }
        previous = current;
        current = previous->next;
    }
    if (current == 0) {
        CpuEnableIntr();
        SsdErrorProc(-17, (int)sequence);
        return -17;
    }
    if (previous != 0) {
        previous->next = sequence->next;
    } else {
        SsdWork.sequenceHead = sequence->next;
    }
    CpuEnableIntr();
    return 0;
}

void SsdDisposeSequenceBuffer(SsdSequence *sequence)
{
    if (sequence->tracks != 0)
        SsdDisposePtr(sequence->tracks);
    if (sequence->data != 0)
        SsdDisposePtr(sequence->data);
    if (sequence->midiDevice != 0)
        SsdDisposeMidiDevice(sequence->midiDevice);
    SsdDisposePtr(sequence);
}

int SsdGetSeqPlayStatus(int sequenceId)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    int flags = 0;
    if (sequence == 0) {
        flags = -17;
    } else if (sequence->flags & 0x8000) {
        flags = 1;
        if (sequence->flags & 0x100) {
            flags = 2;
        }
        if (sequence->masterTempo.remaining != 0) {
            flags |= 0x10000;
        }
        if (sequence->masterNote.remaining != 0) {
            flags |= 0x20000;
        }
        if (sequence->masterVolume.remaining != 0) {
            flags |= 0x40000;
        }
        if (sequence->masterPan.remaining != 0) {
            flags |= 0x80000;
        }
    }
    return flags;
}

void SsdSetSeqMasterTempo(int id, int bpm, int ticks)
{
    SsdSequence *sequence = SsdGetSequencePtr(id);
    int difference;
    if (sequence == 0)
        return;
    if (bpm == 0)
        bpm = 256;
    CpuDisableIntr();
    sequence->masterTempo.target = bpm;
    if (ticks == 0) {
        sequence->masterTempo.value.raw = (unsigned int)bpm << 16;
        sequence->tickRate = (((sequence->bpm.parts.integer * 53687) >> 8)
                                * sequence->masterTempo.value.parts.integer) >> 8;
        sequence->masterTempo.remaining = 0;
    } else {
        difference = ((unsigned int)bpm << 16) - sequence->masterTempo.value.raw;
        if (difference != 0) {
            sequence->masterTempo.remaining = ticks;
            sequence->masterTempo.step = difference / ticks;
        }
    }
    CpuEnableIntr();
}

void SsdSetSeqMasterNote(int sequenceId, int note, int duration)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    int target = (unsigned int)note << 16;
    if (sequence != 0) {
        sequence->masterNote.target = target;
        if (duration == 0) {
            sequence->masterNote.value = target;
            sequence->masterNote.remaining = 0;
            SsdSetMidiDeviceNote(sequence->midiDevice, note);
        } else {
            int difference = (unsigned int)target - (unsigned int)sequence->masterNote.value;
            if (difference != 0) {
                sequence->masterNote.step = difference / duration;
                sequence->masterNote.remaining = duration;
            }
        }
    }
}

void SsdSetSeqMasterVolume(int id, int volume, int ticks)
{
    SsdSequence *sequence = SsdGetSequencePtr(id);
    int target;
    int difference;
    if (sequence == 0)
        return;
    if (!(sequence->flags & 0x8100))
        return;
    target = (unsigned int)volume << 16;
    sequence->masterVolume.target = target;
    if (ticks == 0) {
        sequence->masterVolume.value = target;
        sequence->masterVolume.remaining = 0;
        SsdSetMidiDeviceVolume(sequence->midiDevice, volume);
    } else {
        difference = target - sequence->masterVolume.value;
        if (difference == 0)
            return;
        sequence->masterVolume.remaining = ticks;
        sequence->masterVolume.step = difference / ticks;
    }
    if (volume != 0) {
        SsdStartingSequence(sequence);
    } else {
        sequence->flags |= 0x200;
        if (ticks == 0)
            SsdStoppingSequence(sequence);
    }
}

void SsdSetSeqMasterPanpot(int sequenceId, int pan, int duration)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    int target = (unsigned int)pan << 16;
    if (sequence != 0) {
        sequence->masterPan.target = target;
        if (duration == 0) {
            sequence->masterPan.value = target;
            sequence->masterPan.remaining = 0;
            SsdSetMidiDevicePanpot(sequence->midiDevice, pan);
        } else {
            int difference = (unsigned int)target - (unsigned int)sequence->masterPan.value;
            if (difference != 0) {
                sequence->masterPan.step = difference / duration;
                sequence->masterPan.remaining = duration;
            }
        }
    }
}

void SsdSetSeqReverb(SsdSequence *sequence)
{
    int first;
    int second;
    SsdSpuDmaCompleted(1);
    second = 0;
    first = 0;
    if (sequence->options & 1) {
        first = SsdSetReverbParameter(sequence->timingFormat, sequence->timingPeriod,
                                     sequence->timingParameters[0], sequence->timingParameters[1]);
    }
    if (sequence->options & 2) {
        second = SsdSetReverbParameter(sequence->timingFormat + 256, sequence->timingPeriod,
                                      sequence->timingParameters[0], sequence->timingParameters[1]);
    }
    if (first != 0) {
        SsdReverbClearCompleted(0, 1);
        SsdEnableReverbParam(0);
    }
    if (second != 0) {
        SsdReverbClearCompleted(1, 1);
        SsdEnableReverbParam(1);
    }
}

void SsdSetSeqMute(int sequenceId, int muteMask)
{
    SsdSequence *sequence;
    sequence = SsdGetSequencePtr(sequenceId);
    if (sequence != 0) {
        int trackIndex = 0;
        SsdMusicTrack *track = sequence->tracks + 1;
        int remaining = sequence->trackCount - 1;
        if (remaining != 0) {
            int firstTrackBit = 1;
            do {
                unsigned int flagBits = (unsigned short)track->flags;
                remaining--;
                if (track->flags != 0) {
                    if (muteMask & (firstTrackBit << trackIndex)) {
                        if (!(flagBits & 2)) {
                            track->flags = flagBits | 2;
                            track->attributes |= 2;
                            SsdCutoffChannel(track->channel);
                        }
                    } else if (flagBits & 2) {
                        unsigned int unmutedFlags = flagBits & 0xfffd;
                        track->flags = unmutedFlags;
                        track->attributes &= ~2;
                    }
                } else {
                    if (muteMask & (firstTrackBit << trackIndex)) {
                        track->attributes |= 2;
                    } else {
                        track->attributes &= ~2;
                    }
                }
                trackIndex++;
                track++;
            } while (remaining != 0);
        }
    }
}

void SsdSetSeqNoteFader(int id, int track, short fader)
{
    SsdSequence *sequence = SsdGetSequencePtr(id);
    SsdMusicTrack *record;
    SsdMidiChannel *channel;
    if (sequence != 0) {
        record = &sequence->tracks[track];
        channel = record->channel;
        channel->note_fader = fader;
        SsdSetChannelVoiceEvent(channel, 16);
    }
}

void SsdSetSeqVolumeFader(int sequenceId, int trackIndex, int volume)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    if (sequence != 0) {
        SsdMusicTrack *track = sequence->tracks + trackIndex;
        SsdMidiChannel *channel = track->channel;
        int output = channel->volume * channel->expression * volume / (127 * 127);
        channel->volume_fader = volume;
        channel->effective_volume = output;
        SsdSetChannelVoiceEvent(channel, 32);
    }
}

void SsdSetSeqPanpotFader(int id, int track, signed char fader)
{
    SsdSequence *sequence = SsdGetSequencePtr(id);
    SsdMusicTrack *record;
    SsdMidiChannel *channel;
    if (sequence != 0) {
        record = &sequence->tracks[track];
        channel = record->channel;
        channel->panpot_fader = fader;
        SsdSetChannelVoiceEvent(channel, 32);
    }
}

void SsdSetSeqPatchNumber(int sequenceId, int patchNumber)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    if (sequence != 0) {
        SsdPatchChunk *chunk = sequence->patchChunk;
        if (chunk != 0) {
            if (chunk->patchCount == 0) {
                SsdErrorProc(-48, 0);
            } else if (chunk->patchCount - 1 < patchNumber) {
                SsdErrorProc(-49, 0);
            } else {
                void *patchSet;
                if (patchNumber >= 0) {
                    unsigned short offset = chunk->patchOffsets[patchNumber];
                    patchSet = 0;
                    if (offset != 0) {
                        patchSet = (unsigned char *)chunk + offset;
                    }
                } else {
                    patchSet = 0;
                }
                SsdSetTrackPatchSet(sequenceId, patchSet);
            }
        }
    }
}

void SsdSetTrackPatchSet(int id, const SsdTrackPatchSet *patches)
{
    SsdSequence *sequence = SsdGetSequencePtr(id);
    const SsdTrackPatch *entry;
    int index;
    int count;
    if (sequence != 0) {
        SsdResetChannelFader(sequence->midiDevice, 1);
        if (patches != 0) {
            count = patches->count;
            entry = (const SsdTrackPatch *)(patches + 1);
            CpuDisableIntr();
            for (index = 0; index < count; index++, entry++) {
                SsdMusicTrack *track = &sequence->tracks[entry->track];
                SsdMidiChannel *channel = track->channel;
                int volume = channel->volume;
                int expression = channel->expression;
                int fader = entry->volume;
                volume *= expression;
                volume *= fader;
                volume /= 127 * 127;
                channel->note_fader = entry->note;
                channel->volume_fader = fader;
                channel->panpot_fader = entry->panpot;
                channel->effective_volume = volume;
                SsdSetChannelVoiceEvent(channel, 48);
            }
            CpuEnableIntr();
        }
    }
}

struct SsdSequence *SsdSetSeqABRepeat(int sequenceId)
{
    return SsdGetSequencePtr(sequenceId);
}

void SsdGetSeqPlayTime(int sequence_id, SsdTimeCode *out)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequence_id);
    if (sequence != 0) {
        SsdGetTimeCode(sequence->play_time >> 8, out);
        out->play_ticks = sequence->play_ticks;
    }
}

void SsdGetTimeCode(unsigned int ticks, SsdTimeCode *timeCode)
{
    unsigned int seconds = ticks / 500;
    unsigned int frameUnits;
    unsigned int minutes;
    unsigned int hours;
    ticks %= 500;
    frameUnits = ticks * 30;
    minutes = seconds / 60;
    hours = minutes / 60;

    timeCode->subframes = 0;
    timeCode->ticks = ticks;
    timeCode->seconds = seconds - minutes * 60;
    timeCode->frames = frameUnits / 500;
    timeCode->hours = hours;
    timeCode->minutes = minutes - hours * 60;
}

void SsdGetPlayMeasure(int sequence_id)
{
    SsdGetSequencePtr(sequence_id);
}

struct SsdSequence *SsdGetSeqLyric(int sequenceId)
{
    return SsdGetSequencePtr(sequenceId);
}

int SsdGetSeqCuePoint(void)
{
    return 0;
}

void SsdSetSeqSignal(int sequenceId, int signal)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequenceId);
    if (sequence != 0) {
        sequence->signal = signal;
    }
}

void SsdSetSeqStopCallback(int sequence_id, SsdSequenceStopCallback callback)
{
    SsdSequence *sequence = SsdGetSequencePtr(sequence_id);
    if (sequence != 0) {
        sequence->stop_callback = callback;
    }
}

SsdSequence *SsdGetSequencePtr(int sequenceId)
{
    SsdSequence *sequence = SsdWork.sequenceHead;
    while (sequence != 0) {
        if (sequence->sequenceId == sequenceId) {
            break;
        }
        sequence = sequence->next;
    }
    return sequence;
}

void SsdStartingSequence(SsdSequence *sequence)
{
    if ((sequence->flags & 0x8000) == 0) {
        SsdSetSeqReverb(sequence);
        sequence->flags = (sequence->flags & ~0x300) | 0x8000;
    }
}

void SsdStoppingSequence(SsdSequence *sequence)
{
    if (sequence->flags & 0x8000) {
        CpuDisableIntr();
        SsdCutoffDevive(sequence->midiDevice);
        sequence->flags = (sequence->flags & 0x7dff) | 0x100;
        CpuEnableIntr();
    }
}
