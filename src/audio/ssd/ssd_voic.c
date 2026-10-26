/* Original SSD object: ssd_voic.c. Untouched storage remains partial. */

typedef struct SsdVoiceControl {
    unsigned char index;
    unsigned char core;
    unsigned char voice;
    unsigned char state;
    unsigned short dirty_attributes;
    unsigned char hardware_attributes;
    unsigned char unmodeled_007;
    unsigned short left_volume;
    unsigned short right_volume;
    unsigned char unmodeled_00c[8];
    unsigned short pitch;
    unsigned char unmodeled_016[2];
    unsigned int priority;
    unsigned int start_address;
    unsigned int loop_address;
    unsigned short adsr1;
    unsigned short adsr2;
    unsigned char unmodeled_028[216];
} SsdVoiceControl;

typedef struct SsdChannel {
    unsigned char unmodeled_000[6];
    unsigned char allocation_mode;
    unsigned char unmodeled_007;
    unsigned char priority;
    unsigned char unmodeled_009[223];
    SsdVoiceControl *voice;
} SsdChannel;

typedef struct SsdVoiceRange {
    unsigned char first;
    unsigned char last;
} SsdVoiceRange;

extern struct {
    unsigned char unmodeled_000[0x1df];
    unsigned char stereo;
    unsigned char unmodeled_1e0[0x50];
    unsigned int key_on[2];
    unsigned int key_off[2];
    unsigned int cutoff[2];
    unsigned int noise[2];
    unsigned int pitch_modulation[2];
    unsigned int effect_left[2];
    unsigned int effect_right[2];
    unsigned int active_voices;
    SsdVoiceControl voices[48];
} SsdWork;

extern void sceSdSetSwitch(unsigned short entry, unsigned int bits);
extern void SsdUnlinkChannelVoice(SsdVoiceControl *voice);
extern void sceSdSetParam(unsigned short entry, unsigned short value);
extern void sceSdSetAddr(unsigned short entry, unsigned int value);
extern unsigned short sceSdGetParam(unsigned short entry);
extern unsigned char SsdAllKeyTable[];
extern short SsdAllPitchTable[];
extern void SsdSetVoiceRRAttr(int voice, int rate, int mode);

void SsdInitVoiceManager(void);
SsdVoiceControl *SsdGetKeyonVoice(SsdChannel *channel, SsdVoiceRange *range);
void SsdKeyonVoice(SsdVoiceControl *voice);
void SsdKeyoffUnlinkVoice(SsdVoiceControl *voice);
void SsdCutoffUnlinkVoice(SsdVoiceControl *voice);
void SsdCutoffVoice(SsdVoiceControl *voice);
void SsdKeyoffVoice(SsdVoiceControl *voice);
void SsdSetVoiceKeyon(void);
void SsdSetVoiceKeyoff(void);
int SsdNoteToPitch(int note);
void SsdCompellingKeyoff(void);
void SsdCalcDirectVoiceVolume(short output[2], int volume, int pan);

void SsdInitVoiceManager(void)
{
    int i;
    int core;
    int index;
    for (i = 0; i < 2; ++i) {
        sceSdSetSwitch(i | 0x1600, 0xffffff);
        SsdWork.key_on[i] = 0;
        SsdWork.key_off[i] = 0;
        SsdWork.cutoff[i] = 0;
        SsdWork.noise[i] = 0;
        SsdWork.pitch_modulation[i] = 0;
        SsdWork.effect_left[i] = 0;
        SsdWork.effect_right[i] = 0;
    }
    SsdWork.active_voices = 0;
    index = 0;
    for (core = 0; core < 2; ++core) {
        for (i = 0; i < 24; ++i) {
            SsdWork.voices[index].index = index;
            SsdWork.voices[index].voice = i;
            SsdWork.voices[index].core = core;
            SsdWork.voices[index].state = 0;
            SsdWork.voices[index].priority = 0;
            ++index;
        }
    }
}

SsdVoiceControl *SsdGetKeyonVoice(SsdChannel *channel, SsdVoiceRange *range)
{
    SsdVoiceControl *voice;
    int i;
    unsigned int priority;
    int selected;
    switch (channel->allocation_mode) {
    case 2:
        voice = channel->voice;
        if (voice != 0) return voice;
        voice = &SsdWork.voices[range->first];
        priority = (channel->priority << 16) + 511;
        if (voice->priority < priority)
            SsdUnlinkChannelVoice(voice);
        else
            voice = 0;
        return voice;
    case 1:
        voice = channel->voice;
        if (voice != 0) return voice;
        break;
    case 0:
    default:
        break;
    }
    i = range->first;
    priority = (channel->priority << 16) + 511;
    selected = range->last;
    for (; i <= range->last; ++i) {
        unsigned int candidate = SsdWork.voices[i].priority;
        if (candidate < priority) {
            selected = i;
            if (candidate == 0) break;
            priority = candidate;
        }
    }
    i = selected;
    if (selected >= 48) {
        voice = 0;
    } else {
        voice = &SsdWork.voices[i];
        SsdUnlinkChannelVoice(voice);
    }
    return voice;
}

void SsdKeyonVoice(SsdVoiceControl *voice)
{
    int core;
    unsigned int mask;
    voice->priority |= 0x1ff;
    voice->dirty_attributes = 0xffff;
    core = voice->core;
    mask = 1 << voice->voice;
    SsdWork.key_off[core] &= ~mask;
    SsdWork.cutoff[core] &= ~mask;
    SsdWork.key_on[core] |= mask;
}

void SsdKeyoffUnlinkVoice(SsdVoiceControl *voice)
{
    unsigned int mask;
    int core;
    voice->priority = (unsigned char)voice->priority;
    mask = 1 << voice->voice;
    core = voice->core;
    SsdWork.key_on[core] &= ~mask;
    SsdWork.key_off[core] |= mask;
}

void SsdCutoffUnlinkVoice(SsdVoiceControl *voice)
{
    unsigned int mask;
    int core;
    voice->priority = (unsigned char)voice->priority;
    mask = 1 << voice->voice;
    core = voice->core;
    SsdWork.key_on[core] &= ~mask;
    SsdWork.cutoff[core] |= mask;
}

void SsdCutoffVoice(SsdVoiceControl *voice)
{
    unsigned int mask;
    int core;
    voice->priority &= 0xffff00ff;
    mask = 1 << voice->voice;
    core = voice->core;
    SsdWork.key_on[core] &= ~mask;
    SsdWork.cutoff[core] |= mask;
}

void SsdKeyoffVoice(SsdVoiceControl *voice)
{
    unsigned int mask;
    int core;
    voice->priority &= 0xffff00ff;
    mask = 1 << voice->voice;
    core = voice->core;
    SsdWork.key_on[core] &= ~mask;
    SsdWork.key_off[core] |= mask;
}

void SsdSetVoiceKeyon(void)
{
    unsigned short changes[2];
    SsdVoiceControl *voice = SsdWork.voices;
    int i;
    int core;
    int hardware_voice;
    unsigned short dirty;
    unsigned int mask;
    changes[1] = 0;
    changes[0] = 0;
    for (i = 0; i < 48; ++i) {
        if (voice->priority & 0x1ff) {
            dirty = voice->dirty_attributes;
            if (dirty != 0) {
                core = voice->core;
                hardware_voice = voice->voice << 1;
                if (dirty & 2) {
                    sceSdSetParam(core | hardware_voice, voice->left_volume);
                    sceSdSetParam(core | hardware_voice | 0x100, voice->right_volume);
                }
                if (dirty & 1)
                    sceSdSetParam(core | hardware_voice | 0x200, voice->pitch);
                if (dirty & 8)
                    sceSdSetAddr(core | hardware_voice | 0x2040, voice->start_address);
                if (dirty & 0x10)
                    sceSdSetParam(core | hardware_voice | 0x300, voice->adsr1);
                if (dirty & 0x20)
                    sceSdSetParam(core | hardware_voice | 0x400, voice->adsr2);
                if (dirty & 0xf000) {
                    mask = 1 << voice->voice;
                    if (dirty & 0x1000) {
                        changes[core] |= 0x1000;
                        if (voice->hardware_attributes & 1)
                            SsdWork.noise[core] |= mask;
                        else
                            SsdWork.noise[core] &= ~mask;
                    }
                    if (dirty & 0x2000) {
                        changes[core] |= 0x2000;
                        if (voice->hardware_attributes & 2)
                            SsdWork.pitch_modulation[core] |= mask;
                        else
                            SsdWork.pitch_modulation[core] &= ~mask;
                    }
                    if (dirty & 0x4000) {
                        changes[core] |= 0x4000;
                        if (voice->hardware_attributes & 4)
                            SsdWork.effect_left[core] |= mask;
                        else
                            SsdWork.effect_left[core] &= ~mask;
                    }
                    if (dirty & 0x8000) {
                        changes[core] |= 0x8000;
                        if (voice->hardware_attributes & 8)
                            SsdWork.effect_right[core] |= mask;
                        else
                            SsdWork.effect_right[core] &= ~mask;
                    }
                }
                voice->dirty_attributes = 0;
            }
        }
        ++voice;
    }
    for (core = 0; core < 2; ++core) {
        if (changes[core] & 0xf000) {
            if (changes[core] & 0x1000)
                sceSdSetSwitch(core | 0x1300, SsdWork.noise[core]);
            if (changes[core] & 0x2000)
                sceSdSetSwitch(core | 0x1400, SsdWork.pitch_modulation[core]);
            if (changes[core] & 0x4000) {
                sceSdSetSwitch(core | 0x1900, SsdWork.effect_left[core]);
                sceSdSetSwitch(core | 0x1b00, SsdWork.effect_left[core]);
            }
            if (changes[core] & 0x8000) {
                sceSdSetSwitch(core | 0x1800, SsdWork.effect_right[core]);
                sceSdSetSwitch(core | 0x1a00, SsdWork.effect_right[core]);
            }
        }
        if (SsdWork.key_on[core] != 0) {
            sceSdSetSwitch(core | 0x1500, SsdWork.key_on[core]);
            SsdWork.key_on[core] = 0;
        }
    }
}

void SsdSetVoiceKeyoff(void)
{
    int core;
    int voice;
    unsigned int bits;
    for (core = 0; core < 2; ++core) {
        bits = SsdWork.cutoff[core];
        if (bits != 0) {
            for (voice = 0; voice < 24; ++voice) {
                if (bits & (1 << voice)) {
                    unsigned short entry = core | (voice << 1) | 0x400;
                    unsigned short adsr = sceSdGetParam(entry);
                    sceSdSetParam(entry, (adsr & 0xffc0) | 0x66);
                }
            }
        }
        bits |= SsdWork.key_off[core];
        if (bits != 0) {
            sceSdSetSwitch(core | 0x1600, bits);
            SsdWork.key_off[core] = SsdWork.cutoff[core] = 0;
        }
    }
}

int SsdNoteToPitch(int note)
{
    int key = SsdAllKeyTable[(unsigned)(note & 0x7fff) >> 8];
    int pitch;
    int distance;
    unsigned normalized;
    note &= 0xff;
    do {
        note += (key & 0xf) << 8;
    } while (0);
    pitch = SsdAllPitchTable[note];
    distance = 6 - (key >> 4);
    normalized = (unsigned)distance;
    distance >>= 31;
    normalized = (normalized ^ ((unsigned)distance & 224U)) & 255U;
    do { return (pitch) >> (normalized); } while (0);
}

void SsdCompellingKeyoff(void)
{
    int i;
    SsdVoiceControl *voice;
    for (i = 0; i < 48; ++i) {
        SsdSetVoiceRRAttr(i, 6, 3);
    }
    sceSdSetSwitch(0x1600, 0xffffff);
    sceSdSetSwitch(0x1601, 0xffffff);
    voice = SsdWork.voices;
    for (i = 0; i < 48; ++i) {
        SsdUnlinkChannelVoice(voice);
        ++voice;
    }
}

void SsdCalcDirectVoiceVolume(short output[2], int volume, int pan)
{
    int left;
    int right;
    volume *= volume;
    pan = (pan - 1) << 8;
    if (pan > 32256) pan = 32256;
    if (pan < 0) pan = 0;
    if (SsdWork.stereo) {
        if (pan < 16128) {
            right = pan * 23040 / 16128;
            left = 32512 - pan * 9472 / 16128;
        } else {
            pan = 32256 - pan;
            left = pan * 23040 / 16128;
            right = 32512 - pan * 9472 / 16128;
        }
        left = left * volume >> 15;
        right = right * volume >> 15;
    } else {
        right = volume * 23040 >> 15;
        left = right;
    }
    output[0] = left & 0x7fff;
    output[1] = right & 0x7fff;
}
