/* Original SSD object: ssd_sec.c. Partial layouts describe evidenced fields. */
typedef struct SsdVoiceRange {
    signed char first;
    signed char last;
} SsdVoiceRange;

typedef struct SsdChannel {
    unsigned int flags;
    unsigned char effects, ordinal, allocation_mode, program_type, priority;
    SsdVoiceRange voice_range;
    unsigned char unmodeled_00b[13];
    int volume;
    unsigned char unmodeled_01c[4];
    short fader_note;
    signed char fader_volume, fader_panpot;
    unsigned char unmodeled_024[5];
    signed char channel_volume, expression;
    unsigned char unmodeled_02b[209];
} SsdChannel;

typedef struct SsdRamp {
    int value;
    int step;
    int target;
    short remaining;
    unsigned char _unmodeled_00e[2];
} SsdRamp;

typedef struct SsdEffectTrack {
    short flags;
    unsigned short effects;
    unsigned char index;
    unsigned char channel_index;
    unsigned char voice_index;
    unsigned char _unmodeled_007;
    unsigned char priority;
    unsigned char loop_depth;
    unsigned short bank_id;
    unsigned int effect_id;
    int mode;
    unsigned int timestamp;
    unsigned char *begin;
    unsigned char *stream;
    unsigned char *jump;
    SsdChannel *channel;
    int gate;
    short gate_ticks;
    unsigned char gate_state;
    unsigned char _unmodeled_02f;
    int wait;
    int delta;
    int duration;
    unsigned char gate_ratio;
    unsigned char octave;
    unsigned char note;
    unsigned char last_note;
    SsdRamp velocity;
    SsdRamp pitch;
    SsdRamp volume;
    SsdRamp pan;
    unsigned char _unmodeled_080[48];
} SsdEffectTrack;

typedef struct SsdMidiDevice {
    unsigned char _unmodeled_000[32];
    SsdChannel channels[1];
} SsdMidiDevice;

typedef struct SsdEffectSequence {
    unsigned int magic;
    struct SsdEffectSequence *next;
    void *data;
    SsdEffectTrack *tracks;
    void *label_chunk;
    SsdMidiDevice *device;
    void *metadata;
    int file_id;
    short flags;
    short active_tracks;
    unsigned short bank_id;
    unsigned char track_count;
    unsigned char patch_count;
    unsigned char _unmodeled_028;
    unsigned char file_mode;
    unsigned char sequence_kind;
    unsigned char reverb;
    unsigned char output_volume;
    unsigned char volume_group;
    unsigned char _unmodeled_02e[150];
} SsdEffectSequence;

typedef struct SsdEffectData {
    unsigned int magic;
    unsigned char _unmodeled_004[4];
    unsigned int size;
    unsigned char _unmodeled_00c[2];
    unsigned short file_id;
    unsigned char _unmodeled_010[10];
    unsigned short effect_count;
    unsigned short bank_id;
    unsigned char _unmodeled_01e[18];
    struct SsdEffectData *next;
    unsigned short effect_offsets[0];
} SsdEffectData;

typedef struct SsdGlobalEffects {
    unsigned char unmodeled_000[0x158];
    unsigned int ticks;
    unsigned char unmodeled_15c[16];
    SsdEffectSequence *effect_sequence;
    SsdEffectData *effect_sets;
    unsigned char unmodeled_174[105];
    unsigned char effect_output_volume;
} SsdGlobalEffects;

typedef struct SsdWaveSample {
    unsigned int body_offset;
    unsigned int loop_offset;
    unsigned char volume;
    unsigned char pan;
    short base_pitch;
    unsigned short adsr1;
    unsigned short adsr2;
    char name[16];
} SsdWaveSample;

typedef struct SsdWaveBank {
    unsigned int signature;
    unsigned short bank_id;
    unsigned char sample_count;
    unsigned char program_count;
    unsigned char _unmodeled_008[4];
    unsigned int spu_address;
    unsigned int body_size;
    unsigned char _unmodeled_014[4];
    SsdWaveSample *samples;
    void *programs;
    unsigned char _unmodeled_020[4];
    struct SsdWaveBank *next;
} SsdWaveBank;

typedef struct SsdEffectEntry {
    unsigned char flags;
    unsigned char volume;
    unsigned char priority;
    unsigned char track_count;
    unsigned char _unmodeled_004[4];
    unsigned short track_offsets[0];
} SsdEffectEntry;

typedef struct SsdEffectPlayParam {
    unsigned int effect_id;
    int mode;
    unsigned short bank_id;
    unsigned char start_track;
    unsigned char track_count;
    unsigned char first_track;
    unsigned char priority;
    unsigned char volume;
    unsigned char pan;
    unsigned char flags;
    SsdEffectEntry *effect_data;
} SsdEffectPlayParam;

extern SsdGlobalEffects SsdWork;
extern void CpuDisableIntr(void);
extern void CpuEnableIntr(void);
void SsdFadeoutStopEffectTrack(SsdEffectTrack *track, int duration);
void SsdFadeoutEffect(unsigned int effect_id, int duration, int mode);
void SsdStopEffect(int effect, int mode);
void SsdPlayWaveParam(unsigned int wave, int volume, int pan, int mode);
extern signed char SsdVolumeTable[];
extern SsdWaveBank *SsdGetWSPtrBank(int bank_id);
/* The driver stores error details as a 32-bit integer diagnostic word. */
extern void SsdErrorProc(int error, int detail);
extern void SsdResetChannel(SsdChannel *channel, int bank_id);
extern void SsdSetChannelWave(SsdChannel *channel, int sample_index);
extern void SsdCutoffChannel(SsdChannel *channel);
int SsdSetEffectParam(unsigned int effect_id, int volume, int pan, int mode);
void SsdPlayEffectParam(unsigned int effect_id, int volume, int pan, int mode);
int SsdSaechSeqFreeTrack(SsdEffectPlayParam *play);
int SsdPlaySeqEffectNormal(SsdEffectPlayParam *play);
extern void SsdSetChannelVoiceEvent(SsdChannel *channel, int events);
void SsdFadeoutEffectFileID(int file_id, int duration);
SsdEffectTrack *SsdPlayWaveEffectNormal(SsdEffectPlayParam *play);
extern void *SsdNewPtr(unsigned int size);
extern SsdMidiDevice *SsdNewMidiDevice(int count, int allocation_mode,
                                     const SsdVoiceRange *voice_range);
extern void SsdInitSequence(SsdEffectSequence *sequence);
extern void SsdSetMidiDevicePriority(SsdMidiDevice *device, int priority);
extern void SsdSetMidiDeviceOutputVolume(SsdMidiDevice *device, int volume);
SsdEffectData *SsdGetFileIDToEffectData(int file_id);
void SsdStopEffectFileID(int file_id);
int SsdUnlinkEffectSet(SsdEffectData *effect);
extern void SsdDisposePtr(void *allocation);
extern int SsdCheckData(const void *data, unsigned int magic);
int SsdCheckEffectData(int file_id);
extern void SsdMemoryCopy(void *destination, const void *source, unsigned int size);
void SsdLinkEffectSet(SsdEffectData *effect);
int SsdCheckPlayEffectFileID(int file_id);
void SsdStopWave(int effect, int mode);
void SsdContinueOneEffect(unsigned int effect_id, int duration, int mode);
void SsdPlayWaveNormal(unsigned int wave, int mode);
int SsdCheckPlayEffectAll(void);
void SsdSetPlayEffectParam(unsigned int effect, int volume, int pan, int mode);
void SsdStopAllEffect(void);
void SsdPlayEffectNormal(unsigned int effect, int mode);
SsdEffectSequence *SsdMakeEffectSequenceRec(int track_count, int first_voice);
void SsdDisposeEffectData(int file_id);
int SsdCheckPlayEffect(unsigned int effect_id, int mode);
int SsdAddEffectData(SsdEffectData *effect);

SsdEffectSequence *SsdMakeEffectSequenceRec(int track_count, int first_voice)
{
    SsdEffectSequence *sequence = SsdNewPtr(sizeof(*sequence));
    SsdEffectTrack *track = SsdNewPtr(track_count * sizeof(*track));
    int index;

    sequence->device = SsdNewMidiDevice(track_count, 2, 0);
    sequence->data = 0;
    SsdInitSequence(sequence);
    SsdSetMidiDevicePriority(sequence->device, 3);
    sequence->magic = 0x71657373;
    sequence->metadata = 0;
    sequence->file_id = 0xf000;
    sequence->file_mode = 1;
    sequence->sequence_kind = 1;
    sequence->tracks = track;
    sequence->track_count = track_count;
    sequence->output_volume = SsdWork.effect_output_volume;
    sequence->volume_group = 1;
    SsdSetMidiDeviceOutputVolume(sequence->device, sequence->output_volume);
    for (index = 0; index < track_count; ++first_voice) {
        SsdChannel *channel;

        track->flags = 0;
        track->effects = 0;
        track->index = index;
        track->channel_index = index;
        track->voice_index = first_voice;
        track->begin = 0;
        channel = &sequence->device->channels[index];
        track->channel = channel;
        channel->voice_range.first = channel->voice_range.last = first_voice;
        channel->allocation_mode = 2;
        ++track;
        ++index;
    }
    return sequence;
}

int SsdAddEffectData(SsdEffectData *effect)
{
    int error = (short)SsdCheckData(effect, 0x73646573);
    const SsdEffectData *source;
    unsigned int size;

    source = effect;
    if (error == 0) {
        if (SsdCheckEffectData(effect->file_id) >= 0)
            return effect->file_id;
        size = effect->size;
        effect = SsdNewPtr(size);
        SsdMemoryCopy(effect, source, size);
        error = -11;
        if (effect->magic == 0x73646573) {
            SsdLinkEffectSet(effect);
            return effect->file_id;
        }
    }
    SsdErrorProc(error, (int)effect);
    return -1;
}

void SsdDisposeEffectData(int file_id)
{
    SsdEffectData *effect = SsdGetFileIDToEffectData(file_id);

    if (effect != 0) {
        if (effect->magic != 0x73646573) {
            SsdErrorProc(-11, (int)effect);
        } else {
            SsdStopEffectFileID(file_id);
            SsdUnlinkEffectSet(effect);
            SsdDisposePtr(effect);
        }
    }
}

void SsdLinkEffectSet(SsdEffectData *effect)
{
    CpuDisableIntr();
    effect->next = SsdWork.effect_sets;
    SsdWork.effect_sets = effect;
    CpuEnableIntr();
}

int SsdUnlinkEffectSet(SsdEffectData *effect)
{
    SsdEffectData *current;
    SsdEffectData *previous;

    CpuDisableIntr();
    current = SsdWork.effect_sets;
    previous = 0;
    while (current != 0) {
        if (current == effect)
            break;
        previous = current;
        current = previous->next;
    }
    if (current == 0) {
        CpuEnableIntr();
        SsdErrorProc(-18, (int)effect);
        return -18;
    }
    if (previous != 0)
        previous->next = effect->next;
    else
        SsdWork.effect_sets = effect->next;
    CpuEnableIntr();
    return 0;
}

int SsdCheckEffectData(int file_id)
{
    SsdEffectData *effect = SsdWork.effect_sets;

    while (effect != 0) {
        if (effect->file_id == file_id)
            return file_id;
        effect = effect->next;
    }
    return -1;
}

void SsdPlayEffectNormal(unsigned int effect, int mode)
{
    SsdPlayEffectParam(effect, 127, 63, mode);
}

void SsdSetPlayEffectParam(unsigned int effect, int volume, int pan, int mode)
{
    int result = SsdSetEffectParam(effect, volume, pan, mode);

    if (result == 0)
        SsdPlayEffectParam(effect, volume, pan, mode);
}

void SsdPlayEffectParam(unsigned int effect_id, int volume, int pan, int mode)
{
    SsdEffectData *effect = SsdWork.effect_sets;
    unsigned short file_id = effect_id >> 16;
    unsigned short index;
    unsigned int offset;
    unsigned short *offsets;
    unsigned short *selected_offset;
    SsdEffectEntry *entry;
    SsdEffectPlayParam play;
    if (effect == 0) {
        SsdErrorProc(-18, effect_id);
        return;
    }
    while (effect->file_id != file_id) {
        effect = effect->next;
        if (effect == 0) {
            SsdErrorProc(-18, effect_id);
            return;
        }
    }
    index = effect_id;
    if (index >= effect->effect_count) {
        SsdErrorProc(-19, effect_id);
        return;
    }
    offsets = effect->effect_offsets;
    if (effect->magic != 0x73646573)
        return;
    selected_offset = offsets + index;
    offset = *selected_offset;
    if (offset == 0)
        return;
    /* SED offset table entries are byte offsets from the file header. */
    entry = (SsdEffectEntry *)((unsigned char *)effect + offset);
    play.track_count = entry->track_count;
    if (play.track_count == 0)
        return;
    play.effect_id = effect_id;
    play.mode = mode;
    play.bank_id = effect->bank_id;
    play.start_track = 0;
    play.first_track = 0;
    play.priority = entry->priority;
    play.volume = volume;
    play.pan = pan;
    play.flags = entry->flags;
    play.effect_data = entry;
    if (SsdSaechSeqFreeTrack(&play) == 0)
        SsdPlaySeqEffectNormal(&play);
}

int SsdPlaySeqEffectNormal(SsdEffectPlayParam *play)
{
    SsdEffectSequence *sequence;
    SsdEffectTrack *track;
    SsdEffectEntry *entry;
    unsigned short *offsets;
    unsigned int timestamp;
    int mapped_volume;
    int output_volume;
    int remaining;

    if (SsdGetWSPtrBank(play->bank_id) == 0) {
        SsdErrorProc(-20, play->effect_id);
        return 0;
    }
    remaining = play->track_count;
    if (remaining == 0)
        return 0;
    sequence = SsdWork.effect_sequence;
    track = &sequence->tracks[play->first_track];
    if (track->flags != 0)
        SsdStopEffect(track->effect_id, track->mode);
    entry = play->effect_data;
    timestamp = SsdWork.ticks;
    mapped_volume = SsdVolumeTable[play->volume];
    CpuDisableIntr();
    output_volume = mapped_volume * 100 * 127 / (127 * 127);
    offsets = entry->track_offsets;
    do {
        unsigned short offset = *offsets++;

        if (offset != 0) {
            SsdChannel *channel;
            unsigned char *events;
            unsigned short bank_id;

            track->timestamp = timestamp;
            track->effect_id = play->effect_id;
            track->mode = play->mode;
            track->priority = entry->priority;
            track->flags = (short)0x8400;
            track->effects = 0;
            bank_id = play->bank_id;
            events = (unsigned char *)entry + offset;
            track->begin = events;
            track->stream = events;
            track->jump = 0;
            track->gate = 0;
            track->gate_ticks = 0;
            track->gate_state = 0;
            track->wait = 0;
            track->delta = 0;
            track->duration = 0;
            track->gate_ratio = 15;
            track->octave = 60;
            track->note = 60;
            track->last_note = 60;
            track->velocity.remaining = 0;
            track->bank_id = bank_id;
            track->velocity.value = entry->volume << 16;
            track->pitch.remaining = 0;
            track->pitch.value = 0;
            track->volume.value = 0x640000;
            track->volume.remaining = 0;
            track->pan.value = 0x3f0000;
            track->pan.remaining = 0;
            track->loop_depth = 255;
            channel = track->channel;
            SsdResetChannel(channel, play->bank_id);
            SsdSetChannelWave(channel, 0);
            channel->fader_note = 0;
            channel->fader_volume = mapped_volume;
            channel->fader_panpot = play->pan;
            channel->channel_volume = 100;
            channel->volume = output_volume;
        } else {
            track->flags = 0;
        }
        --remaining;
        ++track;
    } while (remaining != 0);
    sequence->flags = (short)0x8000;
    CpuEnableIntr();
    return play->first_track;
}

void SsdPlayWaveNormal(unsigned int wave, int mode)
{
    SsdPlayWaveParam(wave, 127, 63, mode);
}

void SsdPlayWaveParam(unsigned int wave, int volume, int pan, int mode)
{
    SsdEffectPlayParam play;
    int result;

    play.effect_id = wave;
    play.mode = mode;
    play.bank_id = wave >> 16;
    play.start_track = wave;
    play.track_count = 1;
    play.priority = 0;
    play.volume = volume;
    play.pan = pan;
    play.flags = 0;
    play.effect_data = 0;
    result = SsdSaechSeqFreeTrack(&play);
    if (result == 0)
        SsdPlayWaveEffectNormal(&play);
}

void SsdStopWave(int effect, int mode)
{
    SsdStopEffect(effect, mode);
}

SsdEffectTrack *SsdPlayWaveEffectNormal(SsdEffectPlayParam *play)
{
    SsdWaveBank *bank = SsdGetWSPtrBank(play->bank_id);
    SsdEffectSequence *sequence;
    SsdEffectTrack *track;
    SsdChannel *channel;
    int sample_index;
    int mapped_volume;
    unsigned short bank_id;
    unsigned int timestamp;
    int output_volume;
    int invalid_sample;

    if (bank == 0) {
        SsdErrorProc(-20, play->effect_id);
        return 0;
    }
    sample_index = play->start_track;
    invalid_sample = 1;
    if (bank->sample_count >= sample_index) {
        SsdWaveSample *sample = bank->samples + sample_index;
        invalid_sample = sample->volume == 0;
    }
    if (invalid_sample) {
        SsdErrorProc(-21, play->effect_id);
        return 0;
    }
    if (play->track_count == 0)
        return 0;
    sequence = SsdWork.effect_sequence;
    track = &sequence->tracks[play->first_track];
    if (track->flags != 0)
        SsdStopEffect(track->effect_id, track->mode);
    mapped_volume = SsdVolumeTable[play->volume];
    CpuDisableIntr();
    timestamp = SsdWork.ticks;
    channel = track->channel;
    track->timestamp = timestamp;
    track->effect_id = play->effect_id;
    track->mode = play->mode;
    track->priority = 0;
    track->effects = 0;
    bank_id = play->bank_id;
    track->gate_ratio = 15;
    track->octave = 60;
    track->note = 60;
    track->last_note = 60;
    track->velocity.value = 0x7f0000;
    track->volume.value = 0x640000;
    track->pan.value = 0x3f0000;
    track->loop_depth = 255;
    track->begin = 0;
    track->stream = 0;
    track->jump = 0;
    track->gate = 0;
    track->gate_ticks = 0;
    track->gate_state = 0;
    track->wait = 0;
    track->delta = 0;
    track->duration = 0;
    track->velocity.remaining = 0;
    track->volume.remaining = 0;
    track->pan.remaining = 0;
    track->pitch.remaining = 0;
    track->pitch.value = 0;
    track->flags = (short)0x8420;
    track->bank_id = bank_id;
    SsdResetChannel(channel, play->bank_id);
    SsdSetChannelWave(channel, sample_index);
    output_volume = mapped_volume * 100 * 127 / (127 * 127);
    channel->fader_note = 0;
    channel->fader_volume = mapped_volume;
    channel->fader_panpot = play->pan;
    channel->channel_volume = 100;
    channel->volume = output_volume;
    sequence->flags |= 0x8000;
    CpuEnableIntr();
    return track;
}

int SsdSetEffectParam(unsigned int effect_id, int volume, int pan, int mode)
{
    SsdEffectSequence *sequence = SsdWork.effect_sequence;
    int changed = 0;
    SsdEffectTrack *track = sequence->tracks;
    int remaining = sequence->track_count;
    int mapped_volume = SsdVolumeTable[volume];
    SsdChannel *channel;

    CpuDisableIntr();
    if (mode == 0) {
        do {
            if ((track->flags & 0x8000) && track->effect_id == effect_id) {
                int update_events = 0x30;
                channel = track->channel;
                changed = 1;
                channel->fader_volume = mapped_volume;
                channel->fader_panpot = pan;
                channel->volume = channel->channel_volume * channel->expression *
                                  mapped_volume / (127 * 127);
                SsdSetChannelVoiceEvent(channel, update_events);
            }
            ++track;
        } while (--remaining != 0);
    } else {
        do {
            if ((track->flags & 0x8000) && track->effect_id == effect_id &&
                track->mode == mode) {
                int update_events = 0x30;
                channel = track->channel;
                changed = 1;
                channel->fader_volume = mapped_volume;
                channel->fader_panpot = pan;
                channel->volume = channel->channel_volume * channel->expression *
                                  mapped_volume / (127 * 127);
                SsdSetChannelVoiceEvent(channel, update_events);
            }
            ++track;
        } while (--remaining != 0);
    }
    CpuEnableIntr();
    return changed;
}

int SsdSaechSeqFreeTrack(SsdEffectPlayParam *play)
{
    SsdEffectSequence *sequence;
    int needed;
    int search_limit;
    int first_index;
    SsdEffectTrack *first;

    if (play->flags & 4)
        SsdStopEffect(play->effect_id, 0);
    else
        SsdStopEffect(play->effect_id, play->mode);
    sequence = SsdWork.effect_sequence;
    needed = play->track_count;
    if (play->flags & 0x20) {
        search_limit = 24 - needed;
        first_index = search_limit;
    } else {
        first_index = sequence->track_count - needed;
        search_limit = first_index - 24;
    }
    first = &sequence->tracks[first_index];
    for (;;) {
        int position = search_limit;
        SsdEffectTrack *track = first;
        SsdEffectTrack *victim;
        unsigned int oldest_time;

        while (position >= 0) {
            int remaining = needed;

            if (track->flags == 0) {
                SsdEffectTrack *next = track + 1;

                --remaining;
                while (remaining != 0) {
                    if (next->flags != 0)
                        break;
                    --remaining;
                    ++next;
                }
                if (remaining == 0) {
                    play->first_track = track->index;
                    return 0;
                }
            }
            position -= remaining;
            track -= remaining;
        }
        track = 0;
        oldest_time = ~0U;
        position = sequence->track_count;
        victim = &sequence->tracks[position - 1];
        while (position != 0) {
            if (victim->flags != 0 && victim->timestamp < oldest_time &&
                victim->priority <= play->priority) {
                oldest_time = victim->timestamp;
                track = victim;
            }
            --position;
            --victim;
        }
        if (track == 0)
            return -1;
        SsdStopEffect(track->effect_id, track->mode);
    }
}

void SsdStopAllEffect(void)
{
    SsdEffectSequence *sequence = SsdWork.effect_sequence;
    SsdEffectTrack *track = sequence->tracks;
    int remaining = sequence->track_count;

    CpuDisableIntr();
    while (remaining != 0) {
        --remaining;
        if (track->flags & 0x8000) {
            SsdCutoffChannel(track->channel);
            track->flags = 0;
        }
        ++track;
    }
    CpuEnableIntr();
}

void SsdStopEffect(int effect, int mode)
{
    SsdFadeoutEffect(effect, 0, mode);
}

void SsdContinueOneEffect(unsigned int effect_id, int duration, int mode)
{
    SsdEffectSequence *sequence = SsdWork.effect_sequence;
    SsdEffectTrack *track = sequence->tracks;
    int remaining = sequence->track_count;

    CpuDisableIntr();
    if (mode == 0) {
        while (remaining != 0) {
            --remaining;
            if ((track->flags & 0x8000) && track->effect_id != effect_id)
                SsdFadeoutStopEffectTrack(track, duration);
            ++track;
        }
    } else {
        while (remaining != 0) {
            --remaining;
            if ((track->flags & 0x8000) &&
                (track->effect_id != effect_id || track->mode != mode))
                SsdFadeoutStopEffectTrack(track, duration);
            ++track;
        }
    }
    CpuEnableIntr();
}

void SsdStopEffectFileID(int file_id)
{
    SsdFadeoutEffectFileID(file_id, 0);
}

void SsdFadeoutEffect(unsigned int effect_id, int duration, int mode)
{
    SsdEffectSequence *sequence = SsdWork.effect_sequence;
    SsdEffectTrack *track = sequence->tracks;
    int remaining = sequence->track_count;

    CpuDisableIntr();
    if (mode == 0) {
        while (remaining != 0) {
            --remaining;
            if ((track->flags & 0x8000) && track->effect_id == effect_id)
                SsdFadeoutStopEffectTrack(track, duration);
            ++track;
        }
    } else {
        while (remaining != 0) {
            --remaining;
            if ((track->flags & 0x8000) && track->effect_id == effect_id &&
                track->mode == mode)
                SsdFadeoutStopEffectTrack(track, duration);
            ++track;
        }
    }
    CpuEnableIntr();
}

void SsdFadeoutEffectFileID(int file_id, int duration)
{
    SsdEffectSequence *sequence = SsdWork.effect_sequence;
    SsdEffectTrack *track = sequence->tracks;
    int remaining = sequence->track_count;

    CpuDisableIntr();
    while (remaining != 0) {
        --remaining;
        if ((track->flags & 0x8000) && (track->effect_id >> 16) == file_id)
            SsdFadeoutStopEffectTrack(track, duration);
        ++track;
    }
    CpuEnableIntr();
}

void SsdFadeoutStopEffectTrack(SsdEffectTrack *track, int duration)
{
    if (duration != 0) {
        int step = track->velocity.value / duration;

        track->effects |= 0x20;
        track->velocity.target = 0;
        track->velocity.remaining = duration;
        track->velocity.step = -step;
    } else {
        track->flags = 0;
        SsdCutoffChannel(track->channel);
    }
}

int SsdCheckPlayEffectAll(void)
{
    SsdEffectSequence *sequence = SsdWork.effect_sequence;
    SsdEffectTrack *track = sequence->tracks;
    int remaining = sequence->track_count;

    do {
        if (track->flags & 0x8000)
            return 1;
        --remaining;
        ++track;
    } while (remaining != 0);
    return 0;
}

int SsdCheckPlayEffect(unsigned int effect_id, int mode)
{
    SsdEffectSequence *sequence = SsdWork.effect_sequence;
    SsdEffectTrack *track = sequence->tracks;
    int remaining = sequence->track_count;
    switch (mode) {
    case 0:
        do {
            if ((track->flags & 0x8000) && track->effect_id == effect_id)
                return 1;
            --remaining;
            ++track;
        } while (remaining != 0);
        break;
    default:
    do {
        if ((track->flags & 0x8000) &&
            track->effect_id == effect_id && track->mode == mode)
            return 1;
        ++track;
        --remaining;
    } while (remaining != 0);
    break;
    }
    return 0;
}

int SsdCheckPlayEffectFileID(int file_id)
{
    SsdEffectSequence *sequence = SsdWork.effect_sequence;
    SsdEffectTrack *track = sequence->tracks;
    int remaining = sequence->track_count;

    do {
        if ((track->flags & 0x8000) &&
            (track->effect_id >> 16) == file_id)
            return 1;
        --remaining;
        ++track;
    } while (remaining != 0);
    return 0;
}

SsdEffectData *SsdGetFileIDToEffectData(int file_id)
{
    SsdEffectData *effect = SsdWork.effect_sets;

    while (effect != 0) {
        if (effect->file_id == file_id)
            break;
        effect = effect->next;
    }
    return effect;
}
