#include "common.h"
#include "shared.h"

/* The shared FPK/FCV prefix used by the pack accessors. */
typedef struct FpkFcvPack {
    u32 magic;
    u16 format;
    u16 unmodeled_06;
    int entry_count;
    u16 unmodeled_0c;
    u16 data_offset;
    u32 entries[1];
} FpkFcvPack;

#define FPK_MAGIC 0x004B5046
#define FPK_MAGIC_MASK 0x00FFFFFF
#define FPK_RELOCATED_MASK 0xFF000000
#define FPK_RELOCATED 0x01000000
#define FCV_MAGIC 0x00564346

typedef struct AnmClock {
    u32 flags;
    float frame;
    float speed;
    float reverse_limit;
    float forward_limit;
    u32 unmodeled_14[5];
    float loop_limit;
    u32 unmodeled_2c[3];
    float reset_low;
    float reset_high;
} AnmClock;

#define ANM_WAITING 2
#define ANM_LOOP 8
#define ANM_FINISHED 0x1000
#define ANM_RESTART 0x1010
#define ANM_RESET_BOUNDS 0x40000

/*
 * ANM_getEntry (main:0x0030dc10): PACK_getEntry for the clock's own current
 * data id (low byte at +0x14), relocating the pack on first use; also stores
 * the entry's attribute half-word (or 0 for a bare FCV pack) at +0x20.
 */
void *ANM_getEntry(AnmClock *clock, FpkFcvPack *pack)
{
    int signature;
    int entry_count;
    int entry_index;
    u32 *entries;
    u32 *entry_pointer;
    u32 *format2_pointer;
    int index = *(u8 *)&clock->unmodeled_14[0];

    if (pack != 0) {
        signature = pack->magic & FPK_MAGIC_MASK;
        if (signature == FPK_MAGIC) {
            entries = pack->entries;
            entry_count = pack->entry_count;
            if ((pack->magic & FPK_RELOCATED_MASK) == 0) {
                if (pack->format == 2) {
                    entry_index = 0;
                    if (entry_count > 0) {
                        format2_pointer = entries;
                        do {
                            u32 entry = *format2_pointer;
                            if (entry != 0) {
                                *format2_pointer = entry + pack->data_offset + (u32)pack;
                            }
                            entry_index++;
                            format2_pointer++;
                        } while (entry_index < entry_count);
                    }
                } else if (entry_count > 0) {
                    entry_pointer = entries;
                    entry_index = entry_count;
                    do {
                        u32 entry = *entry_pointer;
                        if (entry != 0) {
                            *entry_pointer = entry + (u32)pack;
                        }
                        entry_index--;
                        entry_pointer++;
                    } while (entry_index != 0);
                }
                pack->magic |= FPK_RELOCATED;
            }

            if (index >= 0 && index < entry_count) {
                u32 *attribute_entry = &pack->entries[entry_count];
                *(u16 *)&clock->unmodeled_14[3] = attribute_entry[index];
                return (void *)entries[index];
            }
        } else if (signature == FCV_MAGIC) {
            *(u16 *)&clock->unmodeled_14[3] = 0;
            return pack;
        }
    }
    return 0;
}

void *PACK_getEntry(FpkFcvPack *pack, int index)
{
    int signature;
    int entry_count;
    int entry_index;
    u32 *entries;
    u32 *entry_pointer;
    u32 *format2_pointer;

    if (pack != 0) {
        signature = pack->magic & FPK_MAGIC_MASK;
        if (signature == FPK_MAGIC) {
            entries = pack->entries;
            entry_count = pack->entry_count;
            if ((pack->magic & FPK_RELOCATED_MASK) == 0) {
                if (pack->format == 2) {
                    entry_index = 0;
                    if (entry_count > 0) {
                        format2_pointer = entries;
                        do {
                            u32 entry = *format2_pointer;
                            if (entry != 0) {
                                *format2_pointer = entry + pack->data_offset + (u32)pack;
                            }
                            entry_index++;
                            format2_pointer++;
                        } while (entry_index < entry_count);
                    }
                } else if (entry_count > 0) {
                    entry_pointer = entries;
                    entry_index = entry_count;
                    do {
                        u32 entry = *entry_pointer;
                        if (entry != 0) {
                            *entry_pointer = entry + (u32)pack;
                        }
                        entry_index--;
                        entry_pointer++;
                    } while (entry_index != 0);
                }
                pack->magic |= FPK_RELOCATED;
            }

            if (index >= 0 && index < entry_count) {
                return (void *)entries[index];
            }
        } else if (signature == FCV_MAGIC) {
            return pack;
        }
    }
    return 0;
}

void *PACK_getAttr(FpkFcvPack *pack, int index)
{
    int signature;
    int entry_count;
    int entry_index;
    u32 *entries;
    u32 *entry_pointer;
    u32 *format2_pointer;

    if (pack != 0) {
        signature = pack->magic & FPK_MAGIC_MASK;
        if (signature == FPK_MAGIC) {
            entries = pack->entries;
            entry_count = pack->entry_count;
            if ((pack->magic & FPK_RELOCATED_MASK) == 0) {
                if (pack->format == 2) {
                    entry_index = 0;
                    if (entry_count > 0) {
                        format2_pointer = entries;
                        do {
                            u32 entry = *format2_pointer;
                            if (entry != 0) {
                                *format2_pointer = entry + pack->data_offset + (u32)pack;
                            }
                            entry_index++;
                            format2_pointer++;
                        } while (entry_index < entry_count);
                    }
                } else {
                    if (entry_count > 0) {
                        entry_pointer = entries;
                        entry_index = entry_count;
                        do {
                            u32 entry = *entry_pointer;
                            if (entry != 0) {
                                *entry_pointer = entry + (u32)pack;
                            }
                            entry_index--;
                            entry_pointer++;
                        } while (entry_index != 0);
                    }
                }
                pack->magic |= FPK_RELOCATED;
            }

            if (index >= 0 && index < entry_count) {
                u32 *attribute_entry = &pack->entries[entry_count];
                return (void *)attribute_entry[index];
            }
        } else if (signature == FCV_MAGIC) {
            return 0;
        }
    }
    return 0;
}

void *PACK_unlink(FpkFcvPack *pack, u32 base)
{
    int signature;
    int entry_count;
    int entry_index;
    u32 *entries;
    u32 *entry_pointer;

    if (pack != 0) {
        signature = pack->magic & FPK_MAGIC_MASK;
        if (signature == FPK_MAGIC) {
            entries = pack->entries;
            entry_count = pack->entry_count;
            if ((pack->magic & FPK_RELOCATED_MASK) != 0) {
                if (pack->format != 2) {
                    if (entry_count > 0) {
                        entry_pointer = entries;
                        entry_index = entry_count;
                        do {
                            u32 entry = *entry_pointer;
                            if (entry != 0) {
                                *entry_pointer = entry - base;
                            }
                            entry_index--;
                            entry_pointer++;
                        } while (entry_index != 0);
                    }
                }
                pack->magic &= FPK_MAGIC_MASK;
            }
        } else if (signature == FCV_MAGIC) {
            return pack;
        }
    }
    return 0;
}

int PACK_getSize(FpkFcvPack *pack)
{
    int signature;
    int entry_count;
    int entry_index;
    u32 *entries;
    u32 *entry_pointer;
    u32 *format2_pointer;

    if (pack != 0) {
        signature = pack->magic & FPK_MAGIC_MASK;
        if (signature == FPK_MAGIC) {
            entries = pack->entries;
            entry_count = pack->entry_count;
            if ((pack->magic & FPK_RELOCATED_MASK) == 0) {
                if (pack->format >= 2) {
                    entry_index = 0;
                    if (entry_count > 0) {
                        format2_pointer = entries;
                        do {
                            u32 entry = *format2_pointer;
                            if (entry != 0) {
                                *format2_pointer = entry + pack->data_offset + (u32)pack;
                            }
                            entry_index++;
                            format2_pointer++;
                        } while (entry_index < entry_count);
                    }
                } else if (entry_count > 0) {
                    entry_pointer = entries;
                    entry_index = entry_count;
                    do {
                        u32 entry = *entry_pointer;
                        if (entry != 0) {
                            *entry_pointer = entry + (u32)pack;
                        }
                        entry_index--;
                        entry_pointer++;
                    } while (entry_index != 0);
                }
                pack->magic |= FPK_RELOCATED;
            }
            return entry_count;
        } else if (signature == FCV_MAGIC) {
            return 1;
        }
    }
    return 0;
}

extern float D_004D852C;

void *ANM_resetDefault(AnmClock *clock, FpkFcvPack *pack) {
    u16 *e;
    float k, t;

    e = PACK_getEntry(pack, *(u16 *)&clock->unmodeled_14[0]);
    k = D_004D852C;
    clock->flags &= ~ANM_FINISHED;
    if (e != 0) {
        clock->speed = k;
        t = (float)e[4] * k;
        clock->forward_limit = (float)e[5] * k;
        clock->reverse_limit = t;
        clock->frame = t;
    }
    return e;
}

typedef struct FcvAttribute {
    u32 unmodeled_0[3];
    int value_c;
    u16 key;
    int attr;
    u8 *time;
    u16 *range;
    u32 unmodeled_20[4];
} FcvAttribute;

extern void FCV2_readAttribute(FcvAttribute *attribute, void *data);

void *ANM_resetTime(AnmClock *clock)
{
    FcvAttribute attribute;
    u16 *entry;
    float k;

    clock->flags &= ~ANM_RESTART;
    entry = (u16 *)clock->unmodeled_2c[0];
    if (entry != 0) {
        k = 1.0f / 30.0f;
        clock->reverse_limit = (float)entry[4] * k;
        clock->forward_limit = (float)entry[5] * k;
        if (entry[2] >= 2) {
            attribute.key = *(u16 *)&clock->unmodeled_14[3];
            FCV2_readAttribute(&attribute, entry + 8);
            if (attribute.time != 0) {
                clock->reverse_limit = (float)*(u16 *)(attribute.time + 2) * k;
                clock->forward_limit = (float)*(u16 *)(attribute.time + 4) * k;
                clock->speed = (float)(signed char)attribute.time[1] * 0.03125f * k;
            }
            if (attribute.range != 0) {
                clock->reset_low = (float)attribute.range[1] * k;
                clock->reset_high = (float)attribute.range[2] * k;
            } else {
                clock->reset_low = clock->reverse_limit;
                clock->reset_high = clock->forward_limit;
            }
            clock->unmodeled_2c[1] = attribute.attr;
        }
        if (clock->speed == 0.0f) {
            clock->speed = 1.0f / 30.0f;
        }
        if (clock->speed >= 0.0f) {
            clock->frame = clock->reverse_limit;
        } else {
            clock->frame = clock->forward_limit;
        }
    }
    return entry;
}

void *ANM_resetPack(AnmClock *clock)
{
    FcvAttribute attribute;
    u16 *entry;
    float k;
    u32 flags;

    entry = ANM_getEntry(clock, (FpkFcvPack *)clock->unmodeled_2c[2]);
    clock->unmodeled_2c[0] = (u32)entry;
    clock->flags &= ~ANM_FINISHED;
    if (entry != 0) {
        k = 1.0f / 30.0f;
        clock->reverse_limit = (float)entry[4] * k;
        clock->forward_limit = (float)entry[5] * k;
        if (entry[2] >= 2) {
            attribute.key = *(u16 *)&clock->unmodeled_14[3];
            FCV2_readAttribute(&attribute, entry + 8);
            if (attribute.time != 0) {
                clock->reverse_limit = (float)*(u16 *)(attribute.time + 2) * k;
                clock->forward_limit = (float)*(u16 *)(attribute.time + 4) * k;
                clock->speed = (float)(signed char)attribute.time[1] * 0.03125f * k;
            }
            if (attribute.range != 0) {
                clock->reset_low = (float)attribute.range[1] * k;
                clock->reset_high = (float)attribute.range[2] * k;
            } else {
                clock->reset_low = clock->reverse_limit;
                clock->reset_high = clock->forward_limit;
            }
            clock->unmodeled_2c[1] = attribute.attr;
        }
        if (clock->speed == 0.0f) {
            clock->speed = 1.0f / 30.0f;
        }
        if (clock->speed >= 0.0f) {
            clock->frame = clock->reverse_limit;
        } else {
            clock->frame = clock->forward_limit;
        }
        flags = clock->flags;
        if (flags & 1) {
            clock->flags = flags | 2;
            if (clock->speed >= 0.0f) {
                clock->loop_limit = clock->reverse_limit + *(float *)&clock->unmodeled_14[4];
            } else {
                clock->loop_limit = clock->reverse_limit - *(float *)&clock->unmodeled_14[4];
            }
        }
    }
    return entry;
}

void *ANM_resetTimeInterp(AnmClock *clock)
{
    FcvAttribute attribute;
    u16 *entry;
    float k;
    u32 flags;

    clock->flags &= ~ANM_RESTART;
    entry = (u16 *)clock->unmodeled_2c[0];
    if (entry != 0) {
        k = 1.0f / 30.0f;
        clock->reverse_limit = (float)entry[4] * k;
        clock->forward_limit = (float)entry[5] * k;
        if (entry[2] >= 2) {
            attribute.key = *(u16 *)&clock->unmodeled_14[3];
            FCV2_readAttribute(&attribute, entry + 8);
            if (attribute.time != 0) {
                clock->reverse_limit = (float)*(u16 *)(attribute.time + 2) * k;
                clock->forward_limit = (float)*(u16 *)(attribute.time + 4) * k;
                clock->speed = (float)(signed char)attribute.time[1] * 0.03125f * k;
            }
            if (attribute.range != 0) {
                clock->reset_low = (float)attribute.range[1] * k;
                clock->reset_high = (float)attribute.range[2] * k;
            } else {
                clock->reset_low = clock->reverse_limit;
                clock->reset_high = clock->forward_limit;
            }
            clock->unmodeled_2c[1] = attribute.attr;
        }
        if (clock->speed == 0.0f) {
            clock->speed = 1.0f / 30.0f;
        }
        if (clock->speed >= 0.0f) {
            clock->frame = clock->reverse_limit;
        } else {
            clock->frame = clock->forward_limit;
        }
        flags = clock->flags;
        if (flags & 1) {
            clock->flags = flags | 2;
            if (clock->speed >= 0.0f) {
                clock->loop_limit = clock->reverse_limit + *(float *)&clock->unmodeled_14[4];
            } else {
                clock->loop_limit = clock->reverse_limit - *(float *)&clock->unmodeled_14[4];
            }
        }
    }
    return entry;
}

void *ANM_reset(AnmClock *clock, FpkFcvPack *pack)
{
    FcvAttribute attribute;
    u16 *entry;
    float k;
    u32 flags;

    clock->unmodeled_2c[2] = (u32)pack;
    entry = ANM_getEntry(clock, pack);
    clock->unmodeled_2c[0] = (u32)entry;
    clock->flags &= ~ANM_FINISHED;
    if (entry != 0) {
        k = 1.0f / 30.0f;
        clock->reverse_limit = (float)entry[4] * k;
        clock->forward_limit = (float)entry[5] * k;
        if (entry[2] >= 2) {
            attribute.key = *(u16 *)&clock->unmodeled_14[3];
            FCV2_readAttribute(&attribute, entry + 8);
            if (attribute.time != 0) {
                clock->reverse_limit = (float)*(u16 *)(attribute.time + 2) * k;
                clock->forward_limit = (float)*(u16 *)(attribute.time + 4) * k;
                clock->speed = (float)(signed char)attribute.time[1] * 0.03125f * k;
            }
            if (attribute.range != 0) {
                clock->reset_low = (float)attribute.range[1] * k;
                clock->reset_high = (float)attribute.range[2] * k;
            } else {
                clock->reset_low = clock->reverse_limit;
                clock->reset_high = clock->forward_limit;
            }
            clock->unmodeled_2c[1] = attribute.attr;
        }
        if (clock->speed == 0.0f) {
            clock->speed = 1.0f / 30.0f;
        }
        if (clock->speed >= 0.0f) {
            clock->frame = clock->reverse_limit;
        } else {
            clock->frame = clock->forward_limit;
        }
        flags = clock->flags;
        if (flags & 1) {
            clock->flags = flags | 2;
            if (clock->speed >= 0.0f) {
                clock->loop_limit = clock->reverse_limit + *(float *)&clock->unmodeled_14[4];
            } else {
                clock->loop_limit = clock->reverse_limit - *(float *)&clock->unmodeled_14[4];
            }
        }
    }
    return entry;
}

void *ANM_update(AnmClock *clock)
{
    u32 flags = clock->flags;
    float speed = clock->speed;
    u16 key;
    int n;
    int size;

    if (!(flags & 0x80000)) {
        clock->frame += speed;
    }
    if (flags & ANM_WAITING) {
        if (speed >= 0.0f) {
            if (clock->frame > clock->loop_limit) {
                clock->flags = flags & ~ANM_WAITING;
            }
        } else {
            if (clock->frame < clock->loop_limit) {
                clock->flags = flags & ~ANM_WAITING;
            }
        }
    } else if (speed >= 0.0f) {
        if (clock->frame > clock->forward_limit) {
            if (flags & ANM_LOOP) {
                clock->flags = flags | ANM_RESTART;
                clock->frame = clock->reverse_limit;
                if (clock->flags & ANM_RESET_BOUNDS) {
                    clock->reverse_limit = clock->reset_low;
                    clock->forward_limit = clock->reset_high;
                    clock->frame = clock->reset_low;
                }
            } else {
                clock->frame = clock->forward_limit;
                clock->flags = flags | ANM_FINISHED;
            }
            if (clock->flags & 0x20000) {
                size = PACK_getSize((FpkFcvPack *)clock->unmodeled_2c[2]);
                key = *(u16 *)&clock->unmodeled_14[0];
                n = key & 0xFF;
                n++;
                if (n >= size) {
                    n = 0;
                }
                n |= key & 0x700;
        *(u16 *)&clock->unmodeled_14[0] = n;
                return ANM_reset(clock, (FpkFcvPack *)clock->unmodeled_2c[2]);
            }
        }
    } else if (clock->frame < clock->reverse_limit) {
        if (flags & ANM_LOOP) {
            clock->flags = flags | ANM_RESTART;
            clock->frame = clock->forward_limit;
            if (clock->flags & ANM_RESET_BOUNDS) {
                clock->forward_limit = clock->reset_high;
                clock->reverse_limit = clock->reset_low;
                clock->frame = clock->reset_high;
            }
        } else {
            clock->frame = clock->reverse_limit;
            clock->flags = flags | ANM_FINISHED;
        }
        if (clock->flags & 0x20000) {
            size = PACK_getSize((FpkFcvPack *)clock->unmodeled_2c[2]);
            key = *(u16 *)&clock->unmodeled_14[0];
            n = key & 0xFF;
            n++;
            if (n >= size) {
                n = 0;
            }
            n |= key & 0x700;
        *(u16 *)&clock->unmodeled_14[0] = n;
            return ANM_reset(clock, (FpkFcvPack *)clock->unmodeled_2c[2]);
        }
    }
}

void ANM_updateTime(AnmClock *clock, float elapsed)
{
    float frame = clock->frame + elapsed;
    u32 flags = clock->flags;

    clock->frame = frame;
    if (flags & ANM_WAITING) {
        if (clock->speed >= 0.0f) {
            if (clock->loop_limit < frame) {
                clock->flags = flags & ~ANM_WAITING;
            }
        } else if (frame < clock->loop_limit) {
            clock->flags = flags & ~ANM_WAITING;
        }
        return;
    }

    if (clock->speed >= 0.0f) {
        if (clock->forward_limit < frame) {
            if (flags & ANM_LOOP) {
                clock->flags = flags | ANM_RESTART;
                clock->frame = clock->reverse_limit;
                if (clock->flags & ANM_RESET_BOUNDS) {
                    clock->reverse_limit = clock->reset_low;
                    clock->forward_limit = clock->reset_high;
                    clock->frame = clock->reset_low;
                }
            } else {
                clock->frame = clock->forward_limit;
                clock->flags = flags | ANM_FINISHED;
            }
        }
    } else if (frame < clock->reverse_limit) {
        if (flags & ANM_LOOP) {
            clock->flags = flags | ANM_RESTART;
            clock->frame = clock->forward_limit;
            if (clock->flags & ANM_RESET_BOUNDS) {
                clock->forward_limit = clock->reset_high;
                clock->reverse_limit = clock->reset_low;
                clock->frame = clock->reset_high;
            }
        } else {
            clock->frame = clock->reverse_limit;
            clock->flags = flags | ANM_FINISHED;
        }
    }
}
