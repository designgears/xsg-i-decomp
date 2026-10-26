/* Original SSD object: ssd_chan.c. Untouched storage remains partial. */

typedef struct SsdLfoParam {
    unsigned short flags;
    unsigned char target;
    unsigned char waveform;
    int depth;
    unsigned short period;
    unsigned short delay;
    unsigned short fadeStep;
} SsdLfoParam;
typedef struct SsdLfoState SsdLfoState;
struct SsdLfoState {
    SsdLfoParam param;
    int (*process)(SsdLfoState *, void *);
    SsdLfoState *next;
    int value;
    int step;
    unsigned short countdown;
    unsigned short delay;
    short fadeLevel;
};
typedef struct SsdLfoWork {
    unsigned char active;
    short pitch;
    short volume;
    short pan;
    short field08;
    SsdLfoState *first;
    SsdLfoState states[4];
} SsdLfoWork;

typedef struct SsdVoiceRange { unsigned char first, last; } SsdVoiceRange;

typedef struct SsdWave {
    unsigned int sample_address;
    unsigned int loop_offset;
    unsigned char volume;
    unsigned char panpot;
    unsigned short note;
    unsigned short adsr1;
    unsigned short adsr2;
    unsigned char synthesis_parameters[16];
} SsdWave;

typedef struct SsdWaveBank {
    unsigned char header[6];
    unsigned char wave_count;
    unsigned char program_count;
    unsigned char unmodeled_008[4];
    unsigned int sample_offset;
    unsigned char properties[8];
    SsdWave *waves;
    unsigned short *program_offsets;
} SsdWaveBank;

typedef struct SsdProgramSplit {
    unsigned char wave_index;
    unsigned char root_key;
    unsigned char lower_key;
    unsigned char upper_key;
    unsigned char volume;
    unsigned char panpot;
    unsigned char effects;
    unsigned char properties;
    unsigned short adsr1;
    unsigned short adsr2;
    unsigned char synthesis_parameters[4];
} SsdProgramSplit;

typedef struct SsdProgram {
    unsigned char properties[3];
    unsigned char split_count;
    unsigned short adsr1;
    unsigned short adsr2;
    SsdVoiceRange voice_range;
    unsigned char synthesis_properties[6];
    SsdLfoParam lfo[4];
    unsigned char unmodeled_lfo_tail[16];
    SsdProgramSplit splits[1];
} SsdProgram;

typedef struct SsdVoice {
    unsigned char index;
    unsigned char core;
    unsigned char number;
    unsigned char state;
    unsigned short flags;
    unsigned char effects;
    unsigned char synthesis_prefix[17];
    union { unsigned int flags; unsigned char envx; } pending;
    unsigned int sample_start;
    unsigned int loop_start;
    unsigned short adsr1;
    unsigned short adsr2;
    unsigned char envelope_state[4];
    unsigned short key_state;
    unsigned short events;
    unsigned char playback_state[4];
    unsigned int duration;
    unsigned short note;
    short velocity;
    unsigned char release_rate;
    signed char transpose;
    unsigned char key;
    unsigned char wave_index;
    unsigned short base_note;
    unsigned short base_velocity;
    unsigned short base_panpot;
    SsdLfoWork lfo;
    struct SsdVoice *next;
    struct SsdChannel *channel;
} SsdVoice;

typedef struct SsdChannel {
    unsigned int flags;
    unsigned char effects;
    unsigned char ordinal;
    unsigned char allocation_mode;
    unsigned char program_type;
    unsigned char priority;
    SsdVoiceRange voice_range;
    unsigned char random_note_low;
    unsigned char random_note_high;
    unsigned short bank;
    signed char program_index;
    unsigned char wave_index;
    int note;
    int volume;
    int panpot;
    short fader_note;
    signed char fader_volume;
    signed char fader_panpot;
    short transpose;
    short pitch_bend;
    unsigned char articulation;
    signed char channel_volume;
    signed char expression;
    unsigned char playback_state[8];
    unsigned char adsr_mode;
    unsigned short adsr1;
    unsigned short adsr2;
    SsdLfoWork lfo;
    SsdVoice *voices;
    SsdWaveBank *bank_data;
    SsdWave *wave_data;
    SsdProgram *program_data;
    struct SsdMidiDevice *device;
} SsdChannel;

typedef struct SsdMidiDevice {
    unsigned int state;
    unsigned char id;
    unsigned char channel_count;
    unsigned short reset_flags;
    unsigned char output_volume;
    unsigned char volume;
    int master_note;
    int master_volume;
    int master_panpot;
    SsdVoiceRange voice_range;
    struct SsdMidiDevice *next;
    SsdChannel channels[1];
} SsdMidiDevice;

typedef struct SsdDriverWork {
    unsigned char driver_state[376];
    SsdMidiDevice *devices;
} SsdDriverWork;

typedef struct SsdMidiEvent {
    unsigned char header[2];
    unsigned char velocity;
    unsigned char key;
    unsigned int duration;
} SsdMidiEvent;

extern void *SsdNewPtr(unsigned int size);
extern void SsdDisposePtr(void *allocation);
extern SsdDriverWork SsdWork;
extern void sceSdSetParam(unsigned short entry, unsigned short value);
extern unsigned int sceSdGetParam(unsigned short entry);
extern void SsdInitLfoWork(SsdLfoWork *lfo);
extern void SsdCutoffUnlinkVoice(SsdVoice *voice);
extern SsdVoice *SsdGetKeyonVoice(SsdChannel *channel, SsdVoiceRange *range);
extern void SsdResetLfoProc(SsdLfoWork *voice, SsdLfoWork *channel);
extern int SsdGetRSRandom(void);
extern SsdWaveBank *SsdGetWSPtrBank(unsigned short bank);
extern void SsdInitLfoParam(SsdLfoWork *work, const SsdLfoParam *parameters);

SsdMidiDevice *SsdNewMidiDevice(int channel_count, int allocation_mode,
                              const SsdVoiceRange *voice_range);
void SsdDisposeMidiDevice(SsdMidiDevice *device);
int SsdGetNewDeivceID(void);
void SsdLinkMidiDevice(SsdMidiDevice *device);
void SsdUnlinkMidiDevice(SsdMidiDevice *device);
void SsdResumeDeviceAll(void);
void SsdResumeDeviceChannel(SsdMidiDevice *device);
void SsdResumeChannel(SsdChannel *channel);
void SsdSuspendDeviceAll(void);
void SsdSuspendDeviceChannel(SsdMidiDevice *device);
void SsdSuspendChannel(SsdChannel *channel);
void SsdResetMidiDeviceParam(SsdMidiDevice *device, int flags);
void SsdResetChannelAll(SsdMidiDevice *device, int flags);
void SsdResetChannel(SsdChannel *channel, int bank);
void SsdResetChannelFader(SsdMidiDevice *device, int propagate);
void SsdSetMidiDevicePriority(SsdMidiDevice *device, unsigned char priority);
void SsdSetMidiDeviceMasterValue(SsdMidiDevice *device, unsigned int note,
                               unsigned int volume, unsigned int panpot);
void SsdSetMidiDeviceNote(SsdMidiDevice *device, unsigned int note);
void SsdSetMidiDeviceVolume(SsdMidiDevice *device, int volume);
void SsdSetMidiDevicePanpot(SsdMidiDevice *device, int panpot);
void SsdSetMidiDeviceOutputVolume(SsdMidiDevice *device, int volume);
void SsdCutoffDevive(SsdMidiDevice *device);
void SsdCutoffChannel(SsdChannel *channel);
void SsdEnvxKeyoff(SsdChannel *channel);
void SsdLinkChannelVoice(SsdChannel *channel, SsdVoice *voice);
void SsdUnlinkChannelVoice(SsdVoice *voice);
void SsdSetChannelAllVoiceEvent(SsdMidiDevice *device, unsigned short events);
void SsdSetChannelVoiceEvent(SsdChannel *channel, unsigned short events);
void SsdSetChannelVoiceVelocity(SsdChannel *channel, int velocity);
void SsdSetChannelHWEffectOn(SsdChannel *channel, unsigned char effects,
                           unsigned short flags);
void SsdSetChannelHWEffectOff(SsdChannel *channel, unsigned char effects,
                            unsigned short flags);
void SsdKeyonChannel(SsdChannel *channel, SsdMidiEvent *event);
void SsdKeyonChannelWave(SsdChannel *channel, SsdMidiEvent *event);
void SsdKeyonChannelProgram(SsdChannel *channel, SsdMidiEvent *event);
SsdProgramSplit *SsdGetProgramSplitPtr(SsdProgram *program, int key);
void SsdKeyonChannelVoice(SsdChannel *channel, SsdVoice *voice);
void SsdSendChannelKeyoffEvent(SsdChannel *channel);
void SsdKeyoffChannel(SsdChannel *channel, const SsdMidiEvent *event);
void SsdKeyoffChannelVoice(SsdChannel *channel, SsdVoice *voice);
void SsdForceKeyoffChannel(SsdChannel *channel, const SsdMidiEvent *event);
void SsdForceKeyoffChannelVoice(SsdChannel *channel, SsdVoice *voice);
void SsdSetChannelWave(SsdChannel *channel, unsigned char wave_index);
SsdProgram *SsdSetChannelProgram(SsdChannel *channel, unsigned char program_index);
void SsdSetChannelAdsr(SsdChannel *channel, int adsr1, int adsr2);

SsdMidiDevice *SsdNewMidiDevice(int channel_count, int allocation_mode,
                              const SsdVoiceRange *voice_range)
{
    SsdMidiDevice *device = SsdNewPtr(sizeof(*device) - sizeof(SsdChannel) +
        channel_count * sizeof(SsdChannel));
    if (device == 0)
        return 0;
    device->state = 0;
    device->id = SsdGetNewDeivceID();
    device->channel_count = channel_count;
    device->output_volume = 127;
    if (voice_range != 0) {
        device->voice_range = *voice_range;
        if ((unsigned char)device->voice_range.first >= 48)
            device->voice_range.first = 0;
        if ((unsigned char)device->voice_range.last >= 48)
            device->voice_range.last = 47;
    } else {
        device->voice_range.first = 0;
        device->voice_range.last = 47;
    }
    {
        int index;
        SsdChannel *channel = device->channels;
        for (index = 0; index < channel_count;) {
            channel->flags = 0;
            channel->ordinal = index++;
            channel->priority = 0;
            channel->allocation_mode = allocation_mode;
            channel->voice_range = device->voice_range;
            channel->voices = 0;
            channel->device = device;
            ++channel;
        }
    }
    SsdLinkMidiDevice(device);
    SsdResetChannelFader(device, 0);
    return device;
}

void SsdDisposeMidiDevice(SsdMidiDevice *device)
{
    SsdResetChannelAll(device, 0);
    SsdUnlinkMidiDevice(device);
    SsdDisposePtr(device);
}

int SsdGetNewDeivceID(void)
{
    int id = 1;
    SsdMidiDevice *device;
    for (;;) {
        device = SsdWork.devices;
        while (device != 0) {
            if (device->id == id)
                break;
            device = device->next;
        }
        if (device == 0)
            break;
        ++id;
        if (id >= 255) {
            id = -1;
            break;
        }
    }
    return id;
}

void SsdLinkMidiDevice(SsdMidiDevice *device)
{
    SsdMidiDevice *tail = SsdWork.devices;
    if (tail != 0) {
        while (tail->next != 0)
            tail = tail->next;
        tail->next = device;
    } else {
        SsdWork.devices = device;
    }
}

void SsdUnlinkMidiDevice(SsdMidiDevice *device)
{
    SsdMidiDevice *previous = SsdWork.devices;
    if (previous == device) {
        SsdWork.devices = previous->next;
    } else {
        while (previous->next != device)
            previous = previous->next;
        previous->next = device->next;
    }
}

void SsdResumeDeviceAll(void)
{
    SsdMidiDevice *device = SsdWork.devices;
    while (device != 0) {
        SsdResumeDeviceChannel(device);
        device = device->next;
    }
}

void SsdResumeDeviceChannel(SsdMidiDevice *device)
{
    int remaining = device->channel_count;
    SsdChannel *channel = device->channels;
    while (remaining != 0) {
        SsdResumeChannel(channel);
        --remaining;
        ++channel;
    }
}

void SsdResumeChannel(SsdChannel *channel)
{
    SsdVoice *voice = channel->voices;
    while (voice != 0) {
        voice->flags |= 0x33;
        voice = voice->next;
    }
}

void SsdSuspendDeviceAll(void)
{
    SsdMidiDevice *device = SsdWork.devices;
    while (device != 0) {
        SsdSuspendDeviceChannel(device);
        device = device->next;
    }
}

void SsdSuspendDeviceChannel(SsdMidiDevice *device)
{
    int remaining = device->channel_count;
    SsdChannel *channel = device->channels;
    while (remaining != 0) {
        SsdSuspendChannel(channel);
        --remaining;
        ++channel;
    }
}

void SsdSuspendChannel(SsdChannel *channel)
{
    SsdVoice *voice = channel->voices;
    while (voice != 0) {
        unsigned char entry = voice->core | (voice->number << 1);
        unsigned short adsr_entry;
        unsigned short adsr;
        sceSdSetParam(entry, 0);
        sceSdSetParam(entry | 0x100, 0);
        sceSdSetParam(entry | 0x200, 0);
        adsr_entry = entry | 0x300;
        adsr = sceSdGetParam(adsr_entry);
        adsr |= 0x7f00;
        sceSdSetParam(adsr_entry, adsr);
        adsr_entry = entry | 0x400;
        sceSdSetParam(adsr_entry, (sceSdGetParam(adsr_entry) & 0xe000) | 0x1fe6);
        voice = voice->next;
    }
}

void SsdResetMidiDeviceParam(SsdMidiDevice *device, int flags)
{
    int volume = (device->output_volume + 1) * 128;
    device->reset_flags = flags;
    device->volume = 127;
    if (volume != 0)
        --volume;
    device->master_volume = volume * 512;
    device->master_note = 0;
    device->master_panpot = 0;
}

void SsdResetChannelAll(SsdMidiDevice *device, int flags)
{
    int remaining = device->channel_count;
    SsdChannel *channel = device->channels;
    if (flags < 0)
        flags = device->reset_flags;
    while (remaining != 0) {
        SsdCutoffChannel(channel);
        SsdResetChannel(channel, flags);
        --remaining;
        ++channel;
    }
}

void SsdResetChannel(SsdChannel *channel, int bank)
{
    channel->effects = 8;
    channel->program_type = 1;
    channel->program_index = -1;
    channel->flags = 0;
    channel->bank = bank;
    channel->wave_index = 0;
    channel->transpose = 0;
    channel->pitch_bend = 0;
    channel->channel_volume = 100;
    channel->note = 0;
    channel->voices = 0;
    channel->bank_data = 0;
    channel->wave_data = 0;
    channel->program_data = 0;
    channel->expression = 127;
    channel->panpot = 0x3f0000;
    channel->adsr_mode = 2;
    channel->volume = channel->fader_volume * 100 * 127 / (127 * 127);
    SsdInitLfoWork(&channel->lfo);
}

void SsdResetChannelFader(SsdMidiDevice *device, int propagate)
{
    int remaining = device->channel_count;
    SsdChannel *channel = device->channels;
    int reset_volume;
    int reset_panpot;
    if (remaining != 0) {
        reset_volume = 127;
        reset_panpot = 64;
        do {
        channel->fader_note = 0;
        channel->fader_volume = reset_volume;
        channel->fader_panpot = reset_panpot;
        if (propagate != 0) {
            int volume = channel->channel_volume;
            int expression = channel->expression;
            channel->volume = volume * expression / 127;
            SsdSetChannelVoiceEvent(channel, 48);
        }
        --remaining;
        ++channel;
        } while (remaining != 0);
    }
}

void SsdSetMidiDevicePriority(SsdMidiDevice *device, unsigned char priority)
{
    SsdChannel *channel = device->channels;
    int remaining = device->channel_count;
    while (remaining != 0) {
        channel->priority = priority;
        --remaining;
        ++channel;
    }
}

void SsdSetMidiDeviceMasterValue(SsdMidiDevice *device, unsigned int note,
                               unsigned int volume, unsigned int panpot)
{
    device->master_note = note << 16;
    device->master_volume = volume << 16;
    device->master_panpot = panpot << 16;
    SsdSetChannelAllVoiceEvent(device, 48);
}

void SsdSetMidiDeviceNote(SsdMidiDevice *device, unsigned int note)
{
    device->master_note = note << 16;
    SsdSetChannelAllVoiceEvent(device, 16);
}

void SsdSetMidiDeviceVolume(SsdMidiDevice *device, int volume)
{
    device->volume = volume;
    volume = (volume + 1) * (device->output_volume + 1);
    if (volume != 0)
        --volume;
    device->master_volume = volume * 512;
    SsdSetChannelAllVoiceEvent(device, 32);
}

void SsdSetMidiDevicePanpot(SsdMidiDevice *device, int panpot)
{
    device->master_panpot = (panpot - 64) * 131072;
    SsdSetChannelAllVoiceEvent(device, 32);
}

void SsdSetMidiDeviceOutputVolume(SsdMidiDevice *device, int volume)
{
    device->output_volume = volume;
    volume = (device->volume + 1) * (volume + 1);
    if (volume != 0)
        --volume;
    device->master_volume = volume * 512;
    SsdSetChannelAllVoiceEvent(device, 32);
}

void SsdCutoffDevive(SsdMidiDevice *device)
{
    int remaining = device->channel_count;
    SsdChannel *channel = device->channels;
    while (remaining != 0) {
        SsdCutoffChannel(channel);
        --remaining;
        ++channel;
    }
}

void SsdCutoffChannel(SsdChannel *channel)
{
    while (channel->voices != 0)
        SsdUnlinkChannelVoice(channel->voices);
}

void SsdEnvxKeyoff(SsdChannel *channel)
{
    SsdVoice *voice = channel->voices;

    while (voice != 0) {
        SsdVoice *next = voice->next;

        if (voice->pending.envx == 0)
            SsdUnlinkChannelVoice(channel->voices);
        voice = next;
    }
}

void SsdLinkChannelVoice(SsdChannel *channel, SsdVoice *voice)
{
    if (voice->channel != 0)
        SsdUnlinkChannelVoice(voice);
    voice->next = channel->voices;
    channel->voices = voice;
    voice->channel = channel;
}

void SsdUnlinkChannelVoice(SsdVoice *voice)
{
    SsdChannel *channel = voice->channel;

    if (channel != 0) {
        SsdVoice *previous = channel->voices;

        if (previous == voice) {
            channel->voices = voice->next;
        } else {
            while (previous->next != voice)
                previous = previous->next;
            previous->next = voice->next;
        }
        voice->channel = 0;
    }
    voice->key_state = 0;
    voice->next = 0;
    SsdCutoffUnlinkVoice(voice);
}

void SsdSetChannelAllVoiceEvent(SsdMidiDevice *device, unsigned short events)
{
    int channels_remaining = device->channel_count;
    SsdChannel *channel = device->channels;

    while (channels_remaining != 0) {
        --channels_remaining;
        SsdSetChannelVoiceEvent(channel, events);
        ++channel;
    }
}

void SsdSetChannelVoiceEvent(SsdChannel *channel, unsigned short events)
{
    SsdVoice *voice = channel->voices;

    while (voice != 0) {
        voice->events |= events;
        voice = voice->next;
    }
}

void SsdSetChannelVoiceVelocity(SsdChannel *channel, int velocity)
{
    SsdVoice *voice = channel->voices;
    velocity = (unsigned int)velocity << 24;
    if (voice == 0)
        return;
process_voice:
    voice->velocity = (velocity >> 24) * voice->base_velocity;
    voice->events |= 32;
    voice = voice->next;
    if (voice != 0)
        goto process_voice;
}

void SsdSetChannelHWEffectOn(SsdChannel *channel, unsigned char effects,
                           unsigned short flags)
{
    SsdVoice *voice = channel->voices;

    channel->effects = (unsigned char)effects | channel->effects;
    while (voice != 0) {
        voice->effects |= effects;
        voice->flags |= flags;
        voice = voice->next;
    }
}

void SsdSetChannelHWEffectOff(SsdChannel *channel, unsigned char effects,
                            unsigned short flags)
{
    SsdVoice *voice = channel->voices;

    effects = ~effects;
    channel->effects = effects & channel->effects;
    while (voice != 0) {
        voice->effects &= effects;
        voice->flags |= flags;
        voice = voice->next;
    }
}

void SsdKeyonChannel(SsdChannel *channel, SsdMidiEvent *event)
{
    if ((channel->flags & 2) == 0)
        SsdForceKeyoffChannel(channel, event);
    switch (channel->program_type) {
    case 1:
        SsdKeyonChannelWave(channel, event);
        break;
    case 2:
        SsdKeyonChannelProgram(channel, event);
        break;
    }
}

void SsdKeyonChannelWave(SsdChannel *channel, SsdMidiEvent *event)
{
    SsdWave *wave = channel->wave_data;
    SsdWaveBank *bank = channel->bank_data;
    unsigned int sample_address = wave->sample_address;
    SsdVoice *voice = SsdGetKeyonVoice(channel, &channel->voice_range);

    if (voice != 0) {
        if ((channel->flags & 2) == 0) {
            voice->sample_start = sample_address + bank->sample_offset;
            voice->loop_start = sample_address + wave->loop_offset;
        }
        voice->key = event->key;
        voice->effects = channel->effects;
        voice->adsr1 = channel->adsr1;
        voice->adsr2 = channel->adsr2;
        voice->release_rate = channel->adsr2 & 31;
        voice->wave_index = channel->wave_index;
        voice->base_velocity = wave->volume;
        voice->base_panpot = wave->panpot;
        voice->base_note = wave->note;
        voice->transpose = 0;
        voice->duration = event->duration;
        voice->note = voice->base_note + (event->key << 8);
        voice->velocity = event->velocity * wave->volume;
        voice->events |= 0xfff0;
        SsdResetLfoProc(&voice->lfo, &channel->lfo);
        SsdKeyonChannelVoice(channel, voice);
    }
}

void SsdKeyonChannelProgram(SsdChannel *channel, SsdMidiEvent *event)
{
    if ((channel->flags & 2) == 0) {
        SsdProgram *program = channel->program_data;
        SsdProgramSplit *split = program->splits;
        unsigned short key;
        SsdWaveBank *bank;
        int splits_remaining;

        if ((channel->flags & 32) != 0) {
            key = SsdGetRSRandom() %
                (channel->random_note_high - channel->random_note_low) +
                channel->random_note_low;
        } else {
            key = event->key;
        }
        bank = channel->bank_data;
        splits_remaining = program->split_count;

        {
        short range_key = key;
        while (splits_remaining != 0) {
            --splits_remaining;
            if (range_key >= split->lower_key && range_key <= split->upper_key) {
                SsdWave *wave = &bank->waves[split->wave_index];
                unsigned int sample_address = wave->sample_address;
                SsdVoiceRange range;
                SsdVoice *voice;

                range.first = program->voice_range.first;
                if (range.first < channel->voice_range.first)
                    range.first = channel->voice_range.first;
                range.last = program->voice_range.last;
                if (range.last > channel->voice_range.last)
                    range.last = channel->voice_range.last;
                voice = SsdGetKeyonVoice(channel, &range);
                if (voice != 0) {
                    unsigned short adsr2;
                    unsigned short adsr1;
                    int base_velocity;
                    int transpose;
                    unsigned short note;

                    voice->sample_start = sample_address + bank->sample_offset;
                    voice->loop_start = sample_address + wave->loop_offset;
                    voice->key = key;
                    voice->effects = channel->effects & split->effects;
                    adsr2 = split->adsr2;
                    adsr1 = split->adsr1;
                    voice->adsr2 = adsr2;
                    voice->adsr1 = adsr1;
                    voice->release_rate = adsr2 & 31;
                    voice->wave_index = split->wave_index;
                    base_velocity = wave->volume * split->volume / 127;
                    voice->base_velocity = base_velocity;
                    voice->base_panpot = split->panpot;
                    voice->base_note = wave->note;
                    transpose = 60 - split->root_key;
                    if ((channel->flags & 16) != 0) {
                        voice->transpose = 0;
                        note = voice->base_note + (60 + (signed char)transpose) * 256;
                    } else {
                        voice->transpose = transpose;
                        note = voice->base_note +
                            (((unsigned int)key + (signed char)transpose) << 8);
                    }
                    voice->note = note;
                    voice->duration = event->duration;
                    voice->velocity = event->velocity * base_velocity;
                    voice->events |= 0xfff0;
                    SsdResetLfoProc(&voice->lfo, &channel->lfo);
                    SsdKeyonChannelVoice(channel, voice);
                }
            }
            ++split;
        }
        }
    }
}

SsdProgramSplit *SsdGetProgramSplitPtr(SsdProgram *program, int key)
{
    SsdProgramSplit *split = program->splits;

    while (split->upper_key < key)
        ++split;
    return split;
}

void SsdKeyonChannelVoice(SsdChannel *channel, SsdVoice *voice)
{
    unsigned short events = voice->events;

    if ((channel->flags & 2) == 0)
        events |= 1;
    voice->events = events & ~2;
    voice->pending.flags |= (channel->priority << 16) + 511;
    if (voice->channel != channel) {
        voice->next = channel->voices;
        channel->voices = voice;
        voice->channel = channel;
    }
}

void SsdSendChannelKeyoffEvent(SsdChannel *channel)
{
    if ((channel->flags & 6) == 0) {
        SsdVoice *voice = channel->voices;

        while (voice != 0) {
            if ((voice->key_state & 2) != 0)
                SsdKeyoffChannelVoice(channel, voice);
            voice = voice->next;
        }
    }
}

void SsdKeyoffChannel(SsdChannel *channel, const SsdMidiEvent *event)
{
    SsdVoice *voice = channel->voices;

    while (voice != 0) {
        if (voice->key == event->key)
            SsdKeyoffChannelVoice(channel, voice);
        voice = voice->next;
    }
}

void SsdKeyoffChannelVoice(SsdChannel *channel, SsdVoice *voice)
{
    voice->key_state |= 2;
    if ((channel->flags & 6) == 0) {
        if (channel->allocation_mode == 2)
            voice->pending.flags &= 0xffff00ffU;
        else
            voice->pending.flags &= 0xff;
        voice->adsr2 = (voice->adsr2 & 0xffe0) | voice->release_rate;
        voice->flags |= 32;
        voice->events = (voice->events & 1) | 2;
    }
}

void SsdForceKeyoffChannel(SsdChannel *channel, const SsdMidiEvent *event)
{
    SsdVoice *voice = channel->voices;

    while (voice != 0) {
        if (voice->key == event->key)
            SsdForceKeyoffChannelVoice(channel, voice);
        voice = voice->next;
    }
}

void SsdForceKeyoffChannelVoice(SsdChannel *channel, SsdVoice *voice)
{
    voice->key_state |= 2;
    if (channel->allocation_mode == 2)
        voice->pending.flags &= 0xffff00ffU;
    else
        voice->pending.flags &= 0xff;
    voice->adsr2 = (voice->adsr2 & 0xffe0) | 6;
    voice->events = (voice->events & 1) | 2;
    voice->flags |= 32;
}

void SsdSetChannelWave(SsdChannel *channel, unsigned char wave_index)
{
    SsdWaveBank *bank = SsdGetWSPtrBank(channel->bank);
    SsdWave *wave = 0;
    int program_type = 0;
    unsigned char index = wave_index;

    if (bank != 0) {
        channel->wave_index = wave_index;
        if (index < bank->wave_count) {
            wave = &bank->waves[index];
            if (wave->volume != 0) {
                unsigned short adsr1 = wave->adsr1;
                unsigned short adsr2 = wave->adsr2;
                channel->adsr1 = adsr1;
                channel->adsr2 = adsr2;
                program_type = 1;
            }
        }
    }
    channel->program_type = program_type;
    channel->bank_data = bank;
    channel->wave_data = wave;
    channel->program_index = -1;
    channel->program_data = 0;
}

SsdProgram *SsdSetChannelProgram(SsdChannel *channel, unsigned char program_index)
{
    SsdProgram *program = 0;
    int selected_index = -1;
    SsdWaveBank *bank = SsdGetWSPtrBank(channel->bank);
    int program_type = 0;

    if (bank != 0 && program_index < bank->program_count) {
        unsigned short *offsets = bank->program_offsets;

        if (offsets != 0) {
            unsigned short offset = offsets[program_index];

            if (offset != 0) {
                unsigned short adsr1;
                unsigned short adsr2;

                selected_index = program_index;
                /* Program-table entries are byte offsets from the table base. */
                program = (SsdProgram *)((unsigned char *)offsets + offset);
                adsr1 = program->adsr1;
                adsr2 = program->adsr2;
                channel->adsr1 = adsr1;
                channel->adsr2 = adsr2;
                SsdInitLfoParam(&channel->lfo, program->lfo);
                program_type = 2;
            }
        }
    }
    channel->program_index = selected_index;
    channel->program_type = program_type;
    channel->bank_data = bank;
    channel->program_data = program;
    return program;
}

void SsdSetChannelAdsr(SsdChannel *channel, int adsr1, int adsr2)
{
    channel->adsr1 = adsr1;
    channel->adsr2 = adsr2;
}
