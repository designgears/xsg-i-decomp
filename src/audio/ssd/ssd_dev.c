typedef unsigned short SdParam;

/* The effect API fills all fields before the depth-only update. */
typedef struct SsdEffectAttr {
    unsigned int core;
    int mode;
    short depth_left;
    short depth_right;
    int delay;
    int feedback;
} SsdEffectAttr;

extern void sceSdSetParam(SdParam entry, SdParam value);
extern SdParam sceSdGetParam(SdParam entry);
extern void sceSdSetAddr(SdParam entry, unsigned int address);
extern void sceSdSetCoreAttr(SdParam entry, SdParam value);
extern void sceSdGetEffectAttr(int core, SsdEffectAttr *attr);
extern void sceSdSetEffectAttr(int core, SsdEffectAttr *attr);

void SsdSetVoiceVolume(int voice, int left, int right)
{
    int entry;
    if (voice >= 24) {
        entry = 1;
        voice -= 24;
    } else {
        entry = 0;
    }
    voice *= 2;
    entry |= voice;
    sceSdSetParam(entry, left);
    entry |= 0x100;
    sceSdSetParam(entry, right);
}

void SsdSetVoicePitch(int voice, int pitch)
{
    int core;
    if (voice >= 24) {
        core = 1;
        voice -= 24;
    } else {
        core = 0;
    }
    sceSdSetParam(core | (voice * 2) | 0x200, pitch);
}

void SsdSetVoiceAdsr1(int voice, int adsr)
{
    int core;
    if (voice >= 24) {
        core = 1;
        voice -= 24;
    } else {
        core = 0;
    }
    sceSdSetParam(core | (voice * 2) | 0x300, adsr);
}

void SsdSetVoiceAdsr2(int voice, int adsr)
{
    int core;
    if (voice >= 24) {
        core = 1;
        voice -= 24;
    } else {
        core = 0;
    }
    sceSdSetParam(core | (voice * 2) | 0x400, adsr);
}

void SsdSetVoiceARAttr(int voice, int rate, int mode)
{
    int entry;
    SdParam adsr;
    if (voice >= 24) {
        entry = 1;
        voice -= 24;
    } else {
        entry = 0;
    }
    voice *= 2;
    entry |= voice;
    entry |= 0x300;
    entry = (SdParam)entry;
    adsr = sceSdGetParam(entry);
    adsr = (adsr & 0xff) | (rate << 8) | (mode << 15);
    sceSdSetParam(entry, adsr);
}

void SsdSetVoiceDR(int voice, int rate)
{
    int entry;
    SdParam adsr;
    if (voice >= 24) {
        entry = 1;
        voice -= 24;
    } else {
        entry = 0;
    }
    voice *= 2;
    entry |= voice;
    entry |= 0x300;
    entry = (SdParam)entry;
    adsr = sceSdGetParam(entry);
    adsr = (adsr & 0xff0f) | (rate << 4);
    sceSdSetParam(entry, adsr);
}

void SsdSetVoiceSL(int voice, int level)
{
    int entry;
    SdParam adsr;
    if (voice >= 24) {
        entry = 1;
        voice -= 24;
    } else {
        entry = 0;
    }
    voice *= 2;
    entry |= voice;
    entry |= 0x300;
    entry = (SdParam)entry;
    adsr = sceSdGetParam(entry);
    adsr = level | (adsr | 0xfff0);
    sceSdSetParam(entry, adsr);
}

void SsdSetVoiceSRAttr(int voice, int rate, int mode)
{
    int entry;
    SdParam adsr;
    if (voice >= 24) {
        entry = 1;
        voice -= 24;
    } else {
        entry = 0;
    }
    voice *= 2;
    entry |= voice;
    entry |= 0x400;
    entry = (SdParam)entry;
    adsr = sceSdGetParam(entry);
    adsr = (adsr & 0x203f) | (rate << 6) | (mode << 13);
    sceSdSetParam(entry, adsr);
}

void SsdSetVoiceRRAttr(int voice, int rate, int mode)
{
    int entry;
    SdParam adsr;
    if (voice >= 24) {
        entry = 1;
        voice -= 24;
    } else {
        entry = 0;
    }
    voice *= 2;
    entry |= voice;
    entry |= 0x400;
    entry = (SdParam)entry;
    adsr = sceSdGetParam(entry);
    adsr = (adsr & 0xffc0) | rate | (mode << 5);
    sceSdSetParam(entry, adsr);
}

void SsdSetVoiceStartAddress(int voice, unsigned int address)
{
    int core;
    if (voice >= 24) {
        core = 1;
        voice -= 24;
    } else {
        core = 0;
    }
    sceSdSetAddr(core | (voice * 2) | 0x2040, address);
}

void SsdSetVoiceLoopStartAddress(int voice, unsigned int address)
{
    int core;
    if (voice >= 24) {
        core = 1;
        voice -= 24;
    } else {
        core = 0;
    }
    sceSdSetAddr(core | (voice * 2) | 0x2140, address);
}

void SsdGetVoiceEnvelope(int voice, unsigned short *envelope)
{
    int core;
    if (voice >= 24) {
        core = 1;
        voice -= 24;
    } else {
        core = 0;
    }
    *envelope = sceSdGetParam(core | (voice * 2) | 0x500);
}

void SsdSetMasterVolume(int core, int left, int right)
{
    sceSdSetParam(0x980, left);
    sceSdSetParam(0xa80, right);
    sceSdSetParam(0x981, left);
    sceSdSetParam(0xa81, right);
}

void SsdSetStreamVolume(int core, int left, int right)
{
}

void SsdSetReverb(int core, int enabled)
{
    sceSdSetCoreAttr(core | 2, enabled);
}

void SsdSetReverbModeType(int core, int type)
{
}

void SsdSetReverbModeDepth(int core, int left, int right)
{
    SsdEffectAttr attr;
    sceSdGetEffectAttr(core, &attr);
    attr.core = 0;
    attr.depth_left = left;
    attr.depth_right = right;
    sceSdSetEffectAttr(core, &attr);
}

void SsdSetReverbModeDelayTime(int core, int delay)
{
}

void SsdSetReverbModeFeedback(int core, int feedback)
{
}
