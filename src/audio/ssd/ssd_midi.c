/* Original SSD ssd_midi.c translation unit. Unmodeled spans retain original storage. */

typedef struct SsdMidiEventQueue {
    unsigned short count;
    unsigned char unmodeled_002[2];
    unsigned int events[64];
} SsdMidiEventQueue;

typedef struct SsdMidiChannel {
    unsigned int flags;
    unsigned char unmodeled_004[10];
    unsigned short bank;
    unsigned char unmodeled_010[4];
    int pitch_offset;
    int voice_volume;
    int voice_panpot;
    unsigned char unmodeled_020[2];
    signed char output_volume;
    unsigned char unmodeled_023[6];
    signed char midi_volume;
    signed char midi_expression;
    unsigned char unmodeled_02b;
    unsigned char nonregistered_parameter_lsb, nonregistered_parameter_msb;
    unsigned char registered_parameter_lsb, registered_parameter_msb;
    unsigned char unmodeled_030[2];
    unsigned char registered_parameter_selected, pitch_bend_sensitivity;
    unsigned char unmodeled_034[200];
} SsdMidiChannel;

typedef struct SsdMidiDevice {
    unsigned char unmodeled_000[32];
    /* SsdNewMidiDevice allocates the channel tail for the requested count. */
    SsdMidiChannel channels[1];
} SsdMidiDevice;

typedef struct SsdMidiState {
    unsigned short enabled;
    unsigned short queue_index;
    SsdMidiDevice *device;
    SsdMidiEventQueue queues[2];
} SsdMidiState;

typedef struct SsdMidiNote {
    unsigned char unmodeled_000[2];
    unsigned char velocity;
    unsigned char note;
    int duration;
} SsdMidiNote;

typedef void (*SsdMidiController)(SsdMidiChannel *, int);

extern SsdMidiState SsdMidi;

extern SsdMidiDevice *SsdNewMidiDevice(int channel, int mode, int options);

extern void SsdResetMidiDeviceParam(SsdMidiDevice *device, int mode);

extern void SsdResetChannelAll(SsdMidiDevice *device, int mode);

extern int printf(const char *format, ...);

extern void CpuDisableIntr(void);

extern void CpuEnableIntr(void);

extern SsdMidiController SsdMidiControlChangeTable[];

extern void SsdKeyoffChannel(SsdMidiChannel *, SsdMidiNote *);

extern void SsdKeyonChannel(SsdMidiChannel *, SsdMidiNote *);

extern void SsdSetChannelProgram(SsdMidiChannel *, int);

extern void SsdSetChannelVoiceEvent(SsdMidiChannel *channel, int event);

extern void SsdSendChannelKeyoffEvent(SsdMidiChannel *channel);

extern void SsdSetChannelHWEffectOn(SsdMidiChannel *channel, int effect, int mask);

extern void SsdSetChannelHWEffectOff(SsdMidiChannel *channel, int effect, int mask);

SsdMidiDevice *SsdMakeMidiSequenceRec(int channel);

void SsdInMidiEvent(unsigned int event);

void SsdMidiEventDecode(SsdMidiDevice *device, unsigned int event);

void SsdMidiControlBankMSB(SsdMidiChannel *channel, int value);

void SsdMidiControlBankLSB(SsdMidiChannel *channel, int value);

void SsdMidiControlModulationMSB(SsdMidiChannel *channel, int value);

void SsdMidiControlPortamentoMSB(SsdMidiChannel *channel, int value);

void SsdMidiControlVolumeMSB(SsdMidiChannel *channel, int value);

void SsdMidiControlPanpotMSB(SsdMidiChannel *channel, int value);

void SsdMidiControlExpressionMSB(SsdMidiChannel *channel, int value);

void SsdMidiControlSustaion(SsdMidiChannel *channel, int value);

void SsdMidiControlReverb(SsdMidiChannel *channel, int value);

void SsdMidiControlRegParLSB(SsdMidiChannel *channel, int value);

void SsdMidiControlRegParMSB(SsdMidiChannel *channel, int value);

void SsdMidiControlNonRegParLSB(SsdMidiChannel *channel, int value);

void SsdMidiControlNonRegParMSB(SsdMidiChannel *channel, int value);

void SsdMidiControlSetRegParDataMSB(SsdMidiChannel *channel, int value);

void SsdMidiControlSetRegParDataLSB(SsdMidiChannel *channel, int value);

SsdMidiDevice *SsdMakeMidiSequenceRec(int channel)
{
    SsdMidiDevice *device;
    int queue;

    SsdMidi.enabled = 1;
    for (queue = 1; queue >= 0; queue--)
        SsdMidi.queues[queue].count = 0;
    SsdMidi.queue_index = 0;
    device = SsdNewMidiDevice(channel, 0, 0);
    SsdMidi.device = device;
    SsdResetMidiDeviceParam(device, SsdMidi.queue_index);
    SsdResetChannelAll(device, SsdMidi.queue_index);
    printf("Ssd init midi device  DeviceHdl=0x%08x Channel=%d\n", device, channel);
    return device;
}

void SsdInMidiEvent(unsigned int event)
{
    SsdMidiEventQueue *queue = &SsdMidi.queues[0];
    int count;

    CpuDisableIntr();
    count = queue->count;
    if (count < 64) {
        queue->events[count] = event;
        queue->count = count + 1;
    }
    CpuEnableIntr();
}

void SsdMidiEventDecode(SsdMidiDevice *device, unsigned int event)
{
    int channel_index = event & 0xff;
    unsigned int message = event >> 8;
    int status = message & 0xff;
    int data1 = (event >> 16) & 0xff;
    unsigned int data2 = event >> 24;
    SsdMidiChannel *channel;
    SsdMidiNote note;
    SsdMidiController controller;
    int pitch;

    channel_index = (channel_index << 4) + (message & 15);
    if (status < 0xf0)
        status = message & 0xf0;

    if (status == 0xd0)
        return;
    if (status < 0xd1) {
        if (status == 0xa0)
            return;
        if (status < 0xa1) {
            if (status == 0x80)
                goto note_off;
            if (status == 0x90)
                goto note_on;
            return;
        }
        if (status == 0xb0)
            goto control_change;
        if (status == 0xc0)
            goto program_change;
        return;
    }
    if (status == 0xf7)
        return;
    if (status < 0xf8) {
        if (status == 0xe0)
            goto pitch_bend;
    }
    return;

note_off:
    channel = &device->channels[channel_index];
    note.velocity = data2;
    note.note = data1;
    SsdKeyoffChannel(channel, &note);
    return;
note_on:
    channel = &device->channels[channel_index];
    note.velocity = data2;
    note.note = data1;
    note.duration = -1;
    if (data2)
        SsdKeyonChannel(channel, &note);
    else
        SsdKeyoffChannel(channel, &note);
    return;
control_change:
    channel = &device->channels[channel_index];
    controller = SsdMidiControlChangeTable[data1];
    if (controller)
        controller(channel, data2);
    return;
program_change:
    SsdSetChannelProgram(&device->channels[channel_index], data1);
    return;
pitch_bend:
    channel = &device->channels[channel_index];
    pitch = ((int)(data1 + (data2 << 7) - 8192) *
             channel->pitch_bend_sensitivity) / 32;
    channel->pitch_offset = (unsigned int)pitch << 16;
    SsdSetChannelVoiceEvent(channel, 16);
    return;
}

void SsdMidiControlBankMSB(SsdMidiChannel *channel, int value)
{
    channel->bank = (channel->bank & 0xff00) | value;
}

void SsdMidiControlBankLSB(SsdMidiChannel *channel, int value)
{
    channel->bank = (channel->bank & 0xff) | (value << 8);
}

void SsdMidiControlModulationMSB(SsdMidiChannel *channel, int value)
{
}

void SsdMidiControlPortamentoMSB(SsdMidiChannel *channel, int value)
{
}

void SsdMidiControlVolumeMSB(SsdMidiChannel *channel, int value)
{
    channel->voice_volume =
        value * channel->midi_expression * channel->output_volume / (127 * 127);
    channel->midi_volume = value;
    SsdSetChannelVoiceEvent(channel, 32);
}

void SsdMidiControlPanpotMSB(SsdMidiChannel *channel, int value)
{
    int panpot = value - 1;
    if (panpot < 0)
        panpot = 0;
    channel->voice_panpot = panpot << 16;
    SsdSetChannelVoiceEvent(channel, 32);
}

void SsdMidiControlExpressionMSB(SsdMidiChannel *channel, int value)
{
    channel->voice_volume =
        channel->midi_volume * value * channel->output_volume / (127 * 127);
    channel->midi_expression = value;
    SsdSetChannelVoiceEvent(channel, 32);
}

void SsdMidiControlSustaion(SsdMidiChannel *channel, int value)
{
    if (value >= 64) {
        channel->flags |= 4;
    } else {
        channel->flags &= ~4U;
        SsdSendChannelKeyoffEvent(channel);
    }
}

void SsdMidiControlReverb(SsdMidiChannel *channel, int value)
{
    if (value >= 64)
        SsdSetChannelHWEffectOn(channel, 4, 0x4000);
    else
        SsdSetChannelHWEffectOff(channel, 4, 0x4000);
}

void SsdMidiControlRegParLSB(SsdMidiChannel *channel, int value)
{
    channel->registered_parameter_lsb = value;
    channel->registered_parameter_selected = 1;
}

void SsdMidiControlRegParMSB(SsdMidiChannel *channel, int value)
{
    channel->registered_parameter_msb = value;
    channel->registered_parameter_selected = 1;
}

void SsdMidiControlNonRegParLSB(SsdMidiChannel *channel, int value)
{
    channel->nonregistered_parameter_lsb = value;
    channel->registered_parameter_selected = 0;
}

void SsdMidiControlNonRegParMSB(SsdMidiChannel *channel, int value)
{
    channel->nonregistered_parameter_msb = value;
    channel->registered_parameter_selected = 0;
}

void SsdMidiControlSetRegParDataMSB(SsdMidiChannel *channel, int value)
{
    if (channel->registered_parameter_selected &&
        ((channel->registered_parameter_msb << 8) +
         channel->registered_parameter_lsb) == 0)
        channel->pitch_bend_sensitivity = value;
}

void SsdMidiControlSetRegParDataLSB(SsdMidiChannel *channel, int value)
{
}
