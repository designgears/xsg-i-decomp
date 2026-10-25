#include "common.h"
#include "exm_1.h"

typedef struct ExHairTypeBuf {
    int kind;
    unsigned int flags;
} ExHairTypeBuf;

extern ExHairTypeBuf htypebuf[];

static signed char GetLastWord(signed char *text)
{
    int index;

    index = 0;
    if (text != 0) {
        if (text[0] != 0 && !(text[0] & 0x80)) {
            do {
                index++;
            } while (text[index] != 0 && !(text[index] & 0x80));
        }
        if (index != 0) {
            return text[index - 1];
        }
        return 0;
    }
    return 0;
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct HairSelectionActor {
    unsigned char unmodeled_000[0x4c2];
    s16 hair_type_code;
    unsigned char unmodeled_4c4[0x80c - 0x4c4];
    signed char *hair_joint_name;
} HairSelectionActor;

extern int humancol[];
extern int shelleycol[];
extern int kosmoscol[];
extern float skirtColliSize;
extern float skirtColliSizeB;
extern float skirtColliSizeS;
extern float skirtColliFootRad;
typedef unsigned int XglQuadword_XP __attribute__((mode(TI)));
typedef union Matrix_XP {
    float elements[16];
    XglQuadword_XP quadwords[4];
} Matrix_XP;
typedef struct Actor_XP {
    u32 flags;
    void (*update)(struct Actor_XP *actor);
    void (*draw)(struct Actor_XP *actor);
    u32 quadword_alignment_gap;
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    Vector4 acceleration;
    Vector4 rotation;
    Vector4 scale;
    unsigned char unmodeled_070[0x4dc - 0x70];
    float ground_height;
    unsigned char unmodeled_4e0[0xa40 - 0x4e0];
    unsigned char moved_hair_mode;
    unsigned char unmodeled_a41[3];
    float moved_hair_delay;
    unsigned char unmodeled_a48[8];
    float moved_hair_sway[4];
    Matrix_XP *previous_joint_matrices;
} Actor_XP;
typedef struct ExMotionJointState_XP {
    unsigned char unmodeled_00[16];
    Vector4 translation;
    unsigned char unmodeled_20[16];
    Vector4 rotation;
} ExMotionJointState_XP;
typedef struct ExMotionJointWork_XP {
    unsigned char unmodeled_000[0x28];
    struct JntHairIdSource *hair_id_source;
    int current_index;
    unsigned char unmodeled_030[8];
    unsigned int flags;
    unsigned char unmodeled_03c[0xa0 - 0x3c];
    ExMotionJointState_XP channels[16];
    unsigned char unmodeled_4a0[0x20];
    unsigned short matrix_index;
    short saved_index;
    unsigned char unmodeled_4c4[0x4d0 - 0x4c4];
    Matrix_XP saved_matrix;
    unsigned char unmodeled_510[0x720 - 0x510];
    Matrix_XP root_matrix;
    unsigned char unmodeled_760[0x7a0 - 0x760];
    struct JntMatrix_XP *matrix_buffer;
    unsigned char unmodeled_7a4[0x80c - 0x7a4];
    signed char *hair_joint_name;
    unsigned int animation_flags;
    unsigned char unmodeled_814[0x820 - 0x814];
    struct Actor_XP *owner;
} ExMotionJointWork_XP;
extern void *collibuf[1];
extern int ibuf;
extern int startjnt;
extern unsigned char abelcol[];
extern unsigned char albeltcol[];
extern unsigned char allencol[];
extern unsigned char chaoscol[];
extern unsigned char ellycol[];
extern unsigned char gaignuncol[];
extern unsigned char helmercol[];
extern unsigned char hyakucol[];
extern unsigned char kebincol[];
extern unsigned char kebinhcol[];
extern unsigned char mamcol[];
extern unsigned char miyukicol[];
extern unsigned char momo1col[];
extern unsigned char momo4col[];
extern unsigned char momocol[];
extern unsigned char offcol[];
extern unsigned char robotcol[];
extern unsigned char shionchcol[];
extern unsigned char shioncol[];
extern unsigned char shitancol[];
extern unsigned char tonnycol[];
extern const float D_004D8614;
extern const float D_004D8618;
extern const float D_004D861C;
extern const float D_004D8620;
extern const float D_004D8624;
extern const float D_004D8628;
extern const float D_004D862C;
extern const float D_004D8630;
extern const float D_004D8634;
extern const float D_004D8638;
extern const float D_004D863C;
extern const float D_004D8640;
extern const float D_004D8644;
extern const float D_004D8648;
extern const float D_004D864C;
extern const float D_004D8650;
extern const float D_004D8654;
extern const float D_004D8658;
extern const float D_004D865C;
extern const float D_004D8660;
extern const float D_004D8664;
extern const float D_004D8668;
extern const float D_004D866C;
extern const float D_004D8670;
extern const float D_004D8674;
extern const float D_004D8678;
extern const float D_004D867C;
extern const float D_004D8680;
extern const float D_004D8684;
extern const float D_004D8688;
extern const float D_004D868C;
extern const float D_004D869C;
extern const float D_004D86A0;
extern const float D_004D86AC;
extern const float D_004D86B0;
extern const float D_004D86B4;
extern const float D_004D86B8;
extern const float D_004D86BC;
extern const float D_004D86C0;
extern const float D_004D86C4;
extern const float D_004D86C8;
extern const float D_004D86CC;
extern const float D_004D86D0;
typedef struct JntMatrix_XP {
    Matrix4 transform;
} JntMatrix_XP;

void EXM_PresetHair(ExMotionJointWork_XP *work)
{
    int lastLetter = GetLastWord(work->hair_joint_name);
    signed char *name;
    ibuf = 0;
    name = work->hair_joint_name;
    htypebuf[0].kind = 999;
    startjnt = 74;
    collibuf[0] = 0;
    if (name == 0) {
        return;
    }
    switch (name[0]) {
    case 'A':
        switch (name[1]) {
        case 'L':
            switch (name[2]) {
            case 'B':
                collibuf[0] = albeltcol;
                if (lastLetter == 'H') {
                    htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags =
                        htypebuf[3].flags = htypebuf[4].flags = htypebuf[5].flags =
                        htypebuf[6].flags = 1;
                    htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind =
                        htypebuf[3].kind = htypebuf[4].kind = htypebuf[5].kind =
                        htypebuf[6].kind = 18;
                    htypebuf[7].flags = 0;
                    htypebuf[7].kind = 6;
                    htypebuf[8].flags = htypebuf[9].flags = 0;
                    htypebuf[8].kind = htypebuf[9].kind = 7;
                    htypebuf[10].flags = 0;
                    htypebuf[10].kind = 6;
                    htypebuf[11].flags = htypebuf[12].flags = htypebuf[13].flags = 0;
                    htypebuf[11].kind = htypebuf[12].kind = htypebuf[13].kind = 8;
                } else {
                    htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags =
                        htypebuf[3].flags = htypebuf[4].flags = 1;
                    htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind =
                        htypebuf[3].kind = htypebuf[4].kind = 18;
                }
                break;
            case 'L':
                collibuf[0] = allencol;
                htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags = 0;
                htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind = 10;
                htypebuf[3].flags = htypebuf[4].flags = htypebuf[5].flags = 0;
                htypebuf[3].kind = htypebuf[4].kind = htypebuf[5].kind = 1;
                htypebuf[6].flags = htypebuf[7].flags = 0;
                htypebuf[6].kind = htypebuf[7].kind = 10;
                break;
            }
            break;
        case 'N':
            switch (name[4]) {
            case 'W': {
                float size = 1.1f;
                float unitSize = 1.0f;
                float footRadius = 0.05f;
                skirtColliSizeB = skirtColliSize = size;
                skirtColliSizeS = unitSize;
                skirtColliFootRad = footRadius;
                htypebuf[0].flags = 0x4400;
                htypebuf[1].flags = 0xc00;
                htypebuf[1].kind = htypebuf[0].kind = 16;
                return;
            }
            case 'D': {
                float size = D_004D8614;
                float unitSize = 1.0f;
                float footRadius = D_004D8618;
                collibuf[0] = humancol;
                skirtColliSizeB = skirtColliSize = size;
                skirtColliSizeS = unitSize;
                skirtColliFootRad = footRadius;
                htypebuf[0].flags = 0x25400;
                htypebuf[1].flags = 0x26400;
                htypebuf[2].flags = 0x11c00;
                htypebuf[3].flags = 0x12c00;
                htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
                htypebuf[4].flags = htypebuf[5].flags = 0;
                htypebuf[4].kind = htypebuf[5].kind = 7;
                htypebuf[6].flags = 0;
                htypebuf[6].kind = 8;
                break;
            }
            }
            break;
        case 'B': {
            float footRadius = D_004D861C;
            collibuf[0] = abelcol;
            skirtColliSizeS = skirtColliSizeB = skirtColliSize = 1.0f;
            skirtColliFootRad = footRadius;
            htypebuf[0].flags = 0x5400;
            htypebuf[1].flags = 0x6400;
            htypebuf[2].flags = 0x1c00;
            htypebuf[3].flags = 0x2c00;
            htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
            htypebuf[4].flags = 1;
            htypebuf[4].kind = 4;
            htypebuf[5].flags = htypebuf[6].flags = 0;
            htypebuf[5].kind = htypebuf[6].kind = 8;
            break;
        }
        case 'S': {
            float unitSize = 1.0f;
            float backSize = D_004D8620;
            float footRadius = D_004D8624;
            skirtColliSizeB = backSize;
            skirtColliSizeS = skirtColliSize = unitSize;
            skirtColliFootRad = footRadius;
            htypebuf[0].flags = 0x5400;
            htypebuf[1].flags = 0x6400;
            htypebuf[2].flags = 0x1c00;
            htypebuf[3].flags = 0x2c00;
            htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
            break;
        }
        }
        break;
    case 'B':
        switch (name[1]) {
        case 'R':
            if (lastLetter == 'D') {
                skirtColliSize = D_004D8628;
                skirtColliSizeS = skirtColliSizeB = 1.0f;
                skirtColliFootRad = D_004D862C;
                htypebuf[0].flags = 0x10c00;
                htypebuf[0].kind = 16;
                break;
            }
            htypebuf[0].kind = 4;
            collibuf[0] = shelleycol;
            htypebuf[1].flags = htypebuf[0].flags = 0;
            break;
        case 'A':
            collibuf[0] = humancol;
            htypebuf[0].flags = 1;
            htypebuf[0].kind = 4;
            break;
        case 'O':
            switch (name[5]) {
            case '2':
                skirtColliSize = D_004D8630;
                startjnt = 136;
                skirtColliSizeS = skirtColliSizeB = 1.0f;
                skirtColliFootRad = D_004D8634;
                htypebuf[0].flags = 0x1480;
                htypebuf[1].flags = 0x2480;
                htypebuf[2].flags = 0x4480;
                htypebuf[3].flags = 0x5480;
                htypebuf[4].flags = 0x6480;
                htypebuf[4].kind = htypebuf[3].kind = htypebuf[2].kind =
                    htypebuf[1].kind = htypebuf[0].kind = 13;
                break;
            case '3':
                htypebuf[0].kind = 1000;
                break;
            }
            break;
        }
        break;
    case 'C':
        switch (name[1]) {
        case 'H':
            collibuf[0] = chaoscol;
            if (lastLetter == 'H') {
                htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags =
                    htypebuf[3].flags = htypebuf[4].flags = 0;
                htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind =
                    htypebuf[3].kind = htypebuf[4].kind = 5;
                htypebuf[5].flags = htypebuf[6].flags = htypebuf[7].flags =
                    htypebuf[8].flags = htypebuf[9].flags = 0;
                htypebuf[5].kind = htypebuf[6].kind = htypebuf[7].kind =
                    htypebuf[8].kind = htypebuf[9].kind = 5;
                htypebuf[10].flags = htypebuf[11].flags = htypebuf[12].flags =
                    htypebuf[13].flags = 0;
                htypebuf[10].kind = htypebuf[11].kind = htypebuf[12].kind =
                    htypebuf[13].kind = 9;
            } else {
                htypebuf[0].flags = htypebuf[1].flags = 0;
                htypebuf[0].kind = htypebuf[1].kind = 6;
                htypebuf[2].flags = htypebuf[3].flags = 0;
                htypebuf[2].kind = htypebuf[3].kind = 9;
            }
            break;
        case 'A':
            collibuf[0] = humancol;
            if (lastLetter == '1') {
                htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags =
                    htypebuf[3].flags = htypebuf[4].flags = 0;
                htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind =
                    htypebuf[3].kind = htypebuf[4].kind = 6;
                htypebuf[5].flags = htypebuf[6].flags = 0;
                htypebuf[5].kind = htypebuf[6].kind = 1;
            } else {
                htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags = 0;
                htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind = 7;
                htypebuf[3].flags = htypebuf[4].flags = 0;
                htypebuf[3].kind = htypebuf[4].kind = 7;
            }
            break;
        case 'L': {
            float size = D_004D8638;
            float unitSize = 1.0f;
            float footRadius = D_004D863C;
            skirtColliSizeB = skirtColliSize = size;
            skirtColliSizeS = unitSize;
            skirtColliFootRad = footRadius;
            htypebuf[0].flags = 0x4400;
            htypebuf[1].flags = 0xc00;
            htypebuf[1].kind = htypebuf[0].kind = 16;
            break;
        }
        }
        break;
    case 'E': {
        float size = D_004D8640;
        float unitSize = 1.0f;
        float footRadius = D_004D8644;
        collibuf[0] = ellycol;
        skirtColliSizeB = skirtColliSize = size;
        skirtColliSizeS = unitSize;
        skirtColliFootRad = footRadius;
        htypebuf[0].flags = 0x5400;
        htypebuf[1].flags = 0x6400;
        htypebuf[2].flags = 0x11c00;
        htypebuf[3].flags = 0x12c00;
        htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
        htypebuf[4].flags = 1;
        htypebuf[4].kind = 2;
        htypebuf[5].flags = htypebuf[6].flags = 0;
        htypebuf[5].kind = htypebuf[6].kind = 5;
        htypebuf[7].flags = htypebuf[8].flags = 0;
        htypebuf[7].kind = htypebuf[8].kind = 4;
        break;
    }
    case 'F':
        switch (name[1]) {
        case 'R':
            collibuf[0] = humancol;
            htypebuf[0].flags = htypebuf[1].flags = 0;
            htypebuf[0].kind = htypebuf[1].kind = 5;
            break;
        case 'E':
            skirtColliSize = D_004D8648;
            skirtColliSizeB = D_004D864C;
            skirtColliSizeS = 1.0f;
            skirtColliFootRad = D_004D8650;
            htypebuf[0].flags = 0x5400;
            htypebuf[1].flags = 0x6400;
            htypebuf[2].flags = 0x11c00;
            htypebuf[3].flags = 0x12c00;
            htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
            htypebuf[4].flags = 0;
            htypebuf[4].kind = 7;
            htypebuf[5].flags = htypebuf[6].flags = 0;
            htypebuf[5].kind = htypebuf[6].kind = 7;
            break;
        }
        break;
    case 'G':
        switch (name[1]) {
        case 'A':
            collibuf[0] = gaignuncol;
            htypebuf[0].flags = htypebuf[2].flags = 8;
            htypebuf[0].kind = htypebuf[2].kind = 7;
            htypebuf[1].flags = 8;
            htypebuf[1].kind = 5;
            htypebuf[3].flags = htypebuf[4].flags = 0;
            htypebuf[3].kind = htypebuf[4].kind = 7;
            htypebuf[5].flags = 0;
            htypebuf[5].kind = 8;
            break;
        case 'I':
            skirtColliSize = D_004D8654;
            skirtColliSizeS = skirtColliSizeB = 1.0f;
            skirtColliFootRad = D_004D8658;
            htypebuf[0].flags = 0x25400;
            htypebuf[1].flags = 0x26400;
            htypebuf[2].flags = 0x11c00;
            htypebuf[3].flags = 0x12c00;
            htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
            break;
        }
        break;
    case 'H':
        switch (name[1]) {
        case 'A':
            htypebuf[0].flags = htypebuf[1].flags = 0;
            htypebuf[0].kind = htypebuf[1].kind = 8;
            break;
        case 'E':
            collibuf[0] = helmercol;
            skirtColliSize = D_004D865C;
            skirtColliSizeB = D_004D8660;
            skirtColliSizeS = 1.0f;
            skirtColliFootRad = D_004D8664;
            htypebuf[0].flags = 0x5400;
            htypebuf[1].flags = 0x6400;
            htypebuf[2].flags = 0x11c00;
            htypebuf[3].flags = 0x12c00;
            htypebuf[4].flags = 0x1400;
            htypebuf[5].flags = 0x2400;
            htypebuf[5].kind = htypebuf[4].kind = htypebuf[3].kind =
                htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
            htypebuf[6].flags = 0x1008;
            htypebuf[7].flags = 0x2010;
            htypebuf[7].kind = htypebuf[6].kind = 18;
            break;
        case 'Y':
            collibuf[0] = hyakucol;
            htypebuf[0].flags = htypebuf[1].flags = 0;
            htypebuf[0].kind = htypebuf[1].kind = 4;
            htypebuf[2].flags = htypebuf[3].flags = 0;
            htypebuf[2].kind = htypebuf[3].kind = 7;
            htypebuf[4].flags = htypebuf[5].flags = 0;
            htypebuf[4].kind = htypebuf[5].kind = 4;
            break;
        }
        break;
    case 'I':
        skirtColliSize = D_004D8668;
        skirtColliSizeB = D_004D866C;
        skirtColliSizeS = 1.0f;
        skirtColliFootRad = D_004D8670;
        htypebuf[0].flags = 0x25400;
        htypebuf[1].flags = 0x26400;
        htypebuf[2].flags = 0x11c00;
        htypebuf[3].flags = 0x12c00;
        htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
        break;
    case 'J':
        switch (name[1]) {
        case 'R':
            if (name[2] == '1') {
                htypebuf[0].kind = 11;
                htypebuf[0].flags = 0;
                return;
            }
            skirtColliSize = D_004D8674;
            skirtColliSizeS = skirtColliSizeB = 1.0f;
            skirtColliFootRad = D_004D8678;
            htypebuf[0].flags = 0x4400;
            htypebuf[1].flags = 0x1400;
            htypebuf[2].flags = 0x2400;
            htypebuf[3].flags = 0x5420;
            htypebuf[4].flags = 0x6420;
            htypebuf[4].kind = htypebuf[3].kind = htypebuf[2].kind =
                    htypebuf[1].kind = htypebuf[0].kind = 13;
            htypebuf[5].flags = 0;
            htypebuf[5].kind = 11;
            break;
        case 'O':
            collibuf[0] = kebinhcol;
            htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags = htypebuf[3].flags = 0;
            htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind = htypebuf[3].kind = 7;
            break;
        }
        break;
    case 'K':
        switch (name[1]) {
        case 'O':
            collibuf[0] = kosmoscol;
            if (lastLetter == 'H') {
                htypebuf[0].flags = 1;
                htypebuf[0].kind = 3;
                htypebuf[1].flags = htypebuf[2].flags = htypebuf[3].flags = 0;
                htypebuf[1].kind = htypebuf[2].kind = htypebuf[3].kind = 5;
                htypebuf[4].flags = htypebuf[5].flags = 2;
                htypebuf[4].kind = htypebuf[5].kind = 4;
                htypebuf[6].flags = htypebuf[7].flags = 0;
                htypebuf[6].kind = htypebuf[7].kind = 8;
                return;
            }
            if (lastLetter != '2') {
                htypebuf[0].flags = 1;
                htypebuf[0].kind = 3;
                htypebuf[1].flags = htypebuf[2].flags = 0;
                htypebuf[1].kind = htypebuf[2].kind = 5;
                htypebuf[3].flags = htypebuf[4].flags = 2;
                htypebuf[3].kind = htypebuf[4].kind = 6;
                htypebuf[5].flags = 0;
                htypebuf[5].kind = 8;
            } else {
                htypebuf[0].kind = 1000;
            }
            break;
        case 'E': {
            unsigned char *defaultPalette = kebincol;
            if (lastLetter == 'H') {
                collibuf[0] = kebinhcol;
                htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags = 0;
                htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind = 6;
                htypebuf[3].flags = htypebuf[4].flags = htypebuf[5].flags =
                    htypebuf[6].flags = htypebuf[7].flags = htypebuf[8].flags = 0;
                htypebuf[3].kind = htypebuf[4].kind = htypebuf[5].kind =
                    htypebuf[6].kind = htypebuf[7].kind = htypebuf[8].kind = 6;
            } else {
                collibuf[0] = defaultPalette;
                if (lastLetter == 'N') {
                    skirtColliSize = 2.0f;
                    skirtColliSizeB = D_004D867C;
                    skirtColliSizeS = D_004D8680;
                    skirtColliFootRad = D_004D8684;
                    htypebuf[0].flags = 0x4400;
                    htypebuf[1].flags = 0x9400;
                    htypebuf[2].flags = 0xa400;
                    htypebuf[3].flags = 0xd400;
                    htypebuf[4].flags = 0xe400;
                    htypebuf[4].kind = htypebuf[3].kind = htypebuf[2].kind =
                    htypebuf[1].kind = htypebuf[0].kind = 13;
                    htypebuf[5].flags = htypebuf[6].flags = 0;
                    htypebuf[5].kind = htypebuf[6].kind = 7;
                } else {
                    htypebuf[0].flags = htypebuf[1].flags = 0;
                    htypebuf[0].kind = htypebuf[1].kind = 7;
                }
            }
            break;
        }
        }
        break;
    case 'L':
        collibuf[0] = momocol;
        htypebuf[0].flags = htypebuf[1].flags = 0;
        htypebuf[0].kind = htypebuf[1].kind = 5;
        htypebuf[2].flags = htypebuf[3].flags = 0;
        htypebuf[2].kind = htypebuf[3].kind = 4;
        break;
    case 'M':
        switch (name[1]) {
        case 'O':
            if (lastLetter == 'H') {
                if (name[2] == 'M') {
                    collibuf[0] = momocol;
                    htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags =
                        htypebuf[3].flags = htypebuf[4].flags = 0;
                    htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind =
                        htypebuf[3].kind = htypebuf[4].kind = 5;
                    htypebuf[5].flags = htypebuf[6].flags = htypebuf[7].flags = 0;
                    htypebuf[5].kind = htypebuf[6].kind = htypebuf[7].kind = 10;
                    htypebuf[8].flags = htypebuf[9].flags = 0;
                    htypebuf[8].kind = htypebuf[9].kind = 2;
                    htypebuf[10].flags = 0;
                    htypebuf[10].kind = 8;
                    return;
                }
                skirtColliSize = D_004D8688;
                skirtColliSizeS = skirtColliSizeB = 1.0f;
                skirtColliFootRad = D_004D868C;
                htypebuf[0].flags = 0x4400;
                htypebuf[1].flags = 0xc00;
                htypebuf[1].kind = htypebuf[0].kind = 16;
            } else if (lastLetter == '1') {
                collibuf[0] = momo1col;
                htypebuf[0].flags = htypebuf[1].flags = 0;
                htypebuf[0].kind = htypebuf[1].kind = 14;
                htypebuf[2].flags = 0;
                htypebuf[2].kind = 8;
                htypebuf[3].flags = htypebuf[4].flags = 0;
                htypebuf[3].kind = htypebuf[4].kind = 5;
                htypebuf[5].flags = htypebuf[6].flags = 0;
                htypebuf[5].kind = htypebuf[6].kind = 4;
                htypebuf[7].flags = 0;
                htypebuf[7].kind = 8;
            } else if (lastLetter == '2') {
                collibuf[0] = momocol;
                skirtColliSize = 1.3f;
                skirtColliSizeB = 0.8f;
                skirtColliSizeS = 1.0f;
                skirtColliFootRad = 0.05f;
                htypebuf[0].flags = 0x5400;
                htypebuf[1].flags = 0x6400;
                htypebuf[2].flags = 0x11c00;
                htypebuf[3].flags = 0x12c00;
                htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
                htypebuf[4].flags = htypebuf[5].flags = htypebuf[6].flags = htypebuf[7].flags = 0;
                htypebuf[4].kind = htypebuf[5].kind = htypebuf[6].kind = htypebuf[7].kind = 17;
                htypebuf[8].flags = 0;
                htypebuf[8].kind = 8;
                htypebuf[9].flags = htypebuf[10].flags = 0;
                htypebuf[9].kind = htypebuf[10].kind = 5;
                htypebuf[11].flags = htypebuf[12].flags = 0;
                htypebuf[11].kind = htypebuf[12].kind = 4;
                htypebuf[13].flags = 0;
                htypebuf[13].kind = 8;
            } else if (lastLetter == '4') {
                collibuf[0] = momo4col;
                htypebuf[0].flags = 1;
                htypebuf[0].kind = 4;
                htypebuf[1].flags = htypebuf[2].flags = 0;
                htypebuf[1].kind = htypebuf[2].kind = 5;
                htypebuf[3].flags = htypebuf[4].flags = 0;
                htypebuf[3].kind = htypebuf[4].kind = 4;
            } else {
                collibuf[0] = momocol;
                htypebuf[0].flags = 0;
                htypebuf[0].kind = 8;
                htypebuf[1].flags = htypebuf[2].flags = 0;
                htypebuf[1].kind = htypebuf[2].kind = 5;
                htypebuf[3].flags = htypebuf[4].flags = 0;
                htypebuf[3].kind = htypebuf[4].kind = 4;
                htypebuf[5].flags = 0;
                htypebuf[5].kind = 8;
            }
            break;
        case 'A':
            collibuf[0] = momocol;
            htypebuf[0].flags = htypebuf[1].flags = htypebuf[2].flags = htypebuf[3].flags = 0;
            htypebuf[0].kind = htypebuf[1].kind = htypebuf[2].kind = htypebuf[3].kind = 4;
            break;
        case 'I':
            collibuf[0] = miyukicol;
            htypebuf[0].flags = htypebuf[2].flags = 0;
            htypebuf[0].kind = htypebuf[2].kind = 4;
            htypebuf[1].flags = 1;
            htypebuf[1].kind = 2;
            htypebuf[3].flags = htypebuf[4].flags = htypebuf[5].flags = 0;
            htypebuf[3].kind = htypebuf[4].kind = htypebuf[5].kind = 7;
            break;
        }
        break;
    case 'O':
        switch (name[1]) {
        case 'B':
            skirtColliSize = D_004D869C;
            skirtColliSizeS = skirtColliSizeB = 1.0f;
            skirtColliFootRad = D_004D86A0;
            htypebuf[0].flags = 0x5400;
            htypebuf[1].flags = 0x6400;
            htypebuf[2].flags = 0x1c00;
            htypebuf[3].flags = 0x2c00;
            htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
            break;
        case 'F': {
            const u32 channelFlags = 1;
            const int channelKind = 4;
            collibuf[0] = offcol;
            htypebuf[0].flags = channelFlags;
            htypebuf[0].kind = channelKind;
            break;
        }
        }
        break;
    case 'P':
        htypebuf[0].kind = 1000;
        break;
    case 'R': {
        float unitSize = 1.0f;
        float size = 1.3f;
        float footRadius = 0.15f;
        collibuf[0] = robotcol;
        skirtColliSize = size;
        skirtColliSizeS = skirtColliSizeB = unitSize;
        skirtColliFootRad = footRadius;
        htypebuf[0].flags = 0x4400;
        htypebuf[0].kind = 16;
        htypebuf[1].flags = 0x4400;
        htypebuf[1].kind = 16;
        htypebuf[2].flags = 0xc00;
        htypebuf[2].kind = 16;
        htypebuf[3].flags = 0xc00;
        htypebuf[3].kind = 16;
        htypebuf[4].flags = 1;
        htypebuf[4].kind = 14;
        break;
    }
    case 'S':
        switch (name[1]) {
        case 'H':
            switch (name[2]) {
            case 'I':
                switch (name[3]) {
                case 'O':
                    collibuf[0] = shioncol;
                    if (lastLetter == 'H') {
                        if (name[6] == 'C') {
                            collibuf[0] = shionchcol;
                            skirtColliSize = D_004D86AC;
                            skirtColliSizeS = skirtColliSizeB = 1.0f;
                            skirtColliFootRad = D_004D86B0;
                            htypebuf[0].flags = 0x5400;
                            htypebuf[1].flags = 0x6400;
                            htypebuf[2].flags = 0x11c00;
                            htypebuf[3].flags = 0x12c00;
                            htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 15;
                            htypebuf[4].flags = 1;
                            htypebuf[4].kind = 2;
                            htypebuf[5].flags = htypebuf[6].flags = htypebuf[7].flags = 0;
                            htypebuf[5].kind = htypebuf[6].kind = htypebuf[7].kind = 7;
                        } else {
                            htypebuf[0].flags = 1;
                            htypebuf[0].kind = 2;
                            htypebuf[8].flags = 8;
                            htypebuf[8].kind = 4;
                            htypebuf[1].flags = htypebuf[2].flags = htypebuf[7].flags = 0;
                            htypebuf[1].kind = htypebuf[2].kind = htypebuf[7].kind = 8;
                            htypebuf[3].flags = 0;
                            htypebuf[3].kind = 4;
                            htypebuf[4].flags = htypebuf[5].flags = htypebuf[6].flags = 0;
                            htypebuf[4].kind = htypebuf[5].kind = htypebuf[6].kind = 5;
                        }
                    } else {
                        htypebuf[0].flags = 0;
                        htypebuf[0].kind = 6;
                        htypebuf[1].flags = htypebuf[4].flags = 0;
                        htypebuf[1].kind = htypebuf[4].kind = 8;
                        htypebuf[2].flags = 1;
                        htypebuf[2].kind = 2;
                        htypebuf[3].flags = 0;
                        htypebuf[3].kind = 4;
                    }
                    break;
                case 'T':
                    collibuf[0] = shitancol;
                    if (lastLetter == 'H') {
                        htypebuf[0].kind = 10;
                        htypebuf[1].flags = htypebuf[0].flags = 1;
                        htypebuf[1].kind = 4;
                        htypebuf[2].flags = htypebuf[3].flags = 0;
                        htypebuf[2].kind = htypebuf[3].kind = 4;
                    } else {
                        htypebuf[0].flags = 1;
                        htypebuf[0].kind = 4;
                        htypebuf[1].flags = htypebuf[2].flags = 0;
                        htypebuf[1].kind = htypebuf[2].kind = 4;
                    }
                    break;
                case '_':
                    switch (name[4]) {
                    case 'D':
                        collibuf[0] = humancol;
                        htypebuf[0].flags = htypebuf[1].flags = 0;
                        htypebuf[0].kind = htypebuf[1].kind = 6;
                        htypebuf[2].flags = 0;
                        htypebuf[2].kind = 5;
                        break;
                    case 'M':
                        collibuf[0] = mamcol;
                        htypebuf[0].kind = 7;
                        htypebuf[1].flags = 1;
                        htypebuf[1].kind = 2;
                        htypebuf[0].flags = 8;
                        htypebuf[2].flags = htypebuf[3].flags = htypebuf[4].flags = 0;
                        htypebuf[2].kind = htypebuf[3].kind = htypebuf[4].kind = 8;
                        break;
                    }
                    break;
                }
                break;
            case 'E':
                htypebuf[0].kind = 1000;
                break;
            }
            break;
        case 'E':
            skirtColliSize = D_004D86B4;
            skirtColliSizeS = skirtColliSizeB = 1.0f;
            skirtColliFootRad = D_004D86B8;
            htypebuf[0].flags = 0x5400;
            htypebuf[1].flags = 0x6400;
            htypebuf[2].flags = 0x1c00;
            htypebuf[3].flags = 0x2c00;
            htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
            htypebuf[4].flags = htypebuf[5].flags = 0;
            htypebuf[4].kind = htypebuf[5].kind = 5;
            break;
        case 'T':
            collibuf[0] = humancol;
            htypebuf[0].flags = htypebuf[1].flags = 0;
            htypebuf[0].kind = htypebuf[1].kind = 5;
            htypebuf[2].flags = 0;
            htypebuf[2].kind = 7;
            htypebuf[3].flags = htypebuf[4].flags = 0;
            htypebuf[3].kind = htypebuf[4].kind = 8;
            break;
        }
        break;
    case 'T':
        collibuf[0] = tonnycol;
        htypebuf[0].flags = htypebuf[1].flags = 0;
        htypebuf[0].kind = htypebuf[1].kind = 4;
        htypebuf[2].flags = htypebuf[3].flags = htypebuf[4].flags = 0;
        htypebuf[2].kind = htypebuf[3].kind = htypebuf[4].kind = 5;
        break;
    case 'U':
        collibuf[0] = momocol;
        htypebuf[0].flags = htypebuf[1].flags = 0;
        htypebuf[0].kind = htypebuf[1].kind = 4;
        break;
    case 'V':
        skirtColliSize = 2.0f;
        skirtColliSizeB = D_004D86BC;
        skirtColliSizeS = D_004D86C0;
        skirtColliFootRad = D_004D86C4;
        htypebuf[0].flags = 0x4400;
        htypebuf[1].flags = 0x9400;
        htypebuf[2].flags = 0xa400;
        htypebuf[3].flags = 0xd400;
        htypebuf[4].flags = 0xe400;
        htypebuf[4].kind = htypebuf[3].kind = htypebuf[2].kind =
                    htypebuf[1].kind = htypebuf[0].kind = 13;
        break;
    case 'W':
        skirtColliSize = 1.5f;
        skirtColliSizeB = D_004D86C8;
        skirtColliSizeS = 1.0f;
        skirtColliFootRad = D_004D86CC;
        htypebuf[0].flags = 0x5400;
        htypebuf[1].flags = 0x6400;
        htypebuf[2].flags = 0x11c00;
        htypebuf[3].flags = 0x12c00;
        htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
        break;
    case 'Y':
        skirtColliSizeS = skirtColliSizeB = skirtColliSize = 1.0f;
        skirtColliFootRad = D_004D86D0;
        htypebuf[0].flags = 0x5400;
        htypebuf[1].flags = 0x6400;
        htypebuf[2].flags = 0x1c00;
        htypebuf[3].flags = 0x2c00;
        htypebuf[3].kind = htypebuf[2].kind = htypebuf[1].kind = htypebuf[0].kind = 16;
        htypebuf[4].flags = htypebuf[5].flags = 0;
        htypebuf[4].kind = htypebuf[5].kind = 7;
        break;
    case 'Z':
        htypebuf[0].flags = htypebuf[1].flags = 0;
        htypebuf[0].kind = htypebuf[1].kind = 5;
        break;
    }
}

extern const float D_004D86D4;
extern const float D_004D86D8;
extern const float D_004D86DC;
extern const float D_004D86E0;
extern int humancol[];
extern int shelleycol[];
extern int kosmoscol[];
extern float skirtColliSize;
extern float skirtColliSizeB;
extern float skirtColliSizeS;
extern float skirtColliFootRad;

static int GetHairTypeSelection(unsigned int *flag, int **col, char *obj)
{
    int ret;
    signed char *name;

    name = *(signed char **)(obj + 0x80C);
    ret = 0;
    *flag = 0;
    if (name != 0) {
    switch (*name) {
    case 'P': {
        short type = *(short *)(obj + 0x4C2);
        *col = humancol;
        skirtColliSize = D_004D86D4;
        skirtColliSizeB = 1.0f;
        skirtColliSizeS = 1.0f;
        skirtColliFootRad = D_004D86D8;
        switch (type) {
        case 0x4A:
            ret = 0x10;
            *flag = 0x4400;
            break;
        case 0x4F:
            ret = 0x10;
            *flag = 0xC00;
            break;
        case 0x54:
            goto ret8;
        case 0x57:
        case 0x5B:
            goto ret5;
        case 0x5F:
        case 0x64:
            ret = 7;
            break;
        }
        break;
    }
    case 'S': {
        short type = *(short *)(obj + 0x4C2);
        *col = shelleycol;
        skirtColliSize = 1.0f;
        skirtColliSizeB = 1.0f;
        skirtColliSizeS = 1.0f;
        skirtColliFootRad = D_004D86DC;
        switch (type) {
        case 0x4A:
            ret = 0x10;
            *flag = 0x4400;
            break;
        case 0x4D:
            ret = 0x10;
            *flag = 0xC00;
            break;
        case 0x50:
            ret = 2;
            *flag = 1;
            break;
        case 0x56:
        case 0x5A:
            goto ret5;
        case 0x5E:
        case 0x65:
            goto ret4;
        }
        break;
    }
    case 'B': {
        short type = *(short *)(obj + 0x4C2);
        skirtColliSize = 1.0f;
        skirtColliSizeB = 1.0f;
        skirtColliSizeS = 1.0f;
        skirtColliFootRad = D_004D86E0;
        switch (type) {
        case 0x71:
            ret = 0x10;
            *flag = 0x5500;
            break;
        case 0x75:
            ret = 0x10;
            *flag = 0x4500;
            break;
        case 0x79:
            ret = 0x10;
            *flag = 0x6500;
            break;
        case 0x7D:
            ret = 0x10;
            *flag = 0x11D00;
            break;
        case 0x80:
            ret = 0x10;
            *flag = 0x12D00;
            break;
        case 0x83:
        case 0x87:
        case 0x8B:
        case 0x96:
        case 0x9B:
        ret4:
            ret = 4;
            break;
        case 0x8E:
        case 0x92:
            goto ret5;
        }
        break;
    }
    case 'K': {
        short type = *(short *)(obj + 0x4C2);
        *col = kosmoscol;
        switch (type) {
        case 0x4A:
        case 0x4D:
        case 0x50:
        case 0x53:
            ret = 0xC;
            break;
        case 0x56:
            ret = 3;
            *flag |= 1;
            break;
        case 0x5C:
        case 0x5F:
        ret5:
            ret = 5;
            break;
        case 0x62:
        case 0x67:
            ret = 4;
            *flag |= 2;
            break;
        case 0x6C:
        ret8:
            ret = 8;
            break;
        }
        break;
    }
    }
    }
    return ret;
}

struct request_XC {
    u8 mode;
    u8 data;
};
typedef unsigned int XglQuadword_XC __attribute__((mode(TI)));
typedef union Matrix_XC {
    float elements[16];
    XglQuadword_XC quadwords[4];
} Matrix_XC;
typedef struct Actor_XC {
    u32 flags;
    void (*update)(struct Actor_XC *actor);
    void (*draw)(struct Actor_XC *actor);
    u32 quadword_alignment_gap;
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    Vector4 acceleration;
    Vector4 rotation;
    Vector4 scale;
    unsigned char unmodeled_070[0x4dc - 0x70];
    float ground_height;
    unsigned char unmodeled_4e0[0xa40 - 0x4e0];
    unsigned char moved_hair_mode;
    unsigned char unmodeled_a41[3];
    float moved_hair_delay;
    unsigned char unmodeled_a48[8];
    float moved_hair_sway[4];
    Matrix_XC *previous_joint_matrices;
} Actor_XC;
typedef struct ExMotionJointState_XC {
    unsigned char unmodeled_00[16];
    Vector4 translation;
    unsigned char unmodeled_20[16];
    Vector4 rotation;
} ExMotionJointState_XC;
typedef struct ExMotionJointWork_XC {
    unsigned char unmodeled_000[0x28];
    struct JntHairIdSource *hair_id_source;
    int current_index;
    unsigned char unmodeled_030[8];
    unsigned int flags;
    unsigned char unmodeled_03c[0xa0 - 0x3c];
    ExMotionJointState_XC channels[16];
    unsigned char unmodeled_4a0[0x20];
    unsigned short matrix_index;
    short saved_index;
    unsigned char unmodeled_4c4[0x4d0 - 0x4c4];
    Matrix_XC saved_matrix;
    unsigned char unmodeled_510[0x720 - 0x510];
    Matrix_XC root_matrix;
    unsigned char unmodeled_760[0x7a0 - 0x760];
    struct JntMatrix_XC *matrix_buffer;
    unsigned char unmodeled_7a4[0x80c - 0x7a4];
    signed char *hair_joint_name;
    unsigned int animation_flags;
    unsigned char unmodeled_814[0x820 - 0x814];
    struct Actor_XC *owner;
} ExMotionJointWork_XC;
void MATRIX_translate4s(Matrix4 matrix, float x, float y, float z);
typedef struct ExCollisionPrimitive_XC {
    unsigned short joint;
    unsigned short kind;
    unsigned char unmodeled_004[12];
    Vector4 start;
    Vector4 end;
} ExCollisionPrimitive_XC;
typedef struct ExCollisionRequest_XC {
    struct ExCollisionPrimitive_XC *primitives;
    unsigned int flags;
    float *correction;
} ExCollisionRequest_XC;
typedef struct JntMatrix_XC {
    Matrix4 transform;
} JntMatrix_XC;
extern float vel_0[4];

static int EXM_CheckExCollision(ExMotionJointWork_XC *work, const float position[3],
                               const Matrix4 jointMatrix, ExCollisionRequest_XC *request_XC)
{
    JntMatrix_XC startMatrix;
    Vector4 difference;
    float correctionScale;
    int count = 0;
    int index;
    vel_0[0] = vel_0[1] = vel_0[2] = 0.0f;
    index = 0;
    while (request_XC->primitives[index].kind != 0) {
        const ExCollisionPrimitive_XC *primitive = &request_XC->primitives[index];
        u32 filterFlags = request_XC->flags;
        if ((filterFlags & 8) && primitive->kind != 6) {
            index++;
            continue;
        }
        if ((filterFlags & 16) && primitive->kind != 7) {
            index++;
            continue;
        }
        __builtin_memcpy(startMatrix.transform, work->matrix_buffer[primitive->joint].transform,
                         sizeof(startMatrix.transform));
        MATRIX_translate4s(startMatrix.transform, primitive->start.x,
                           primitive->start.y, primitive->start.z);
        switch (primitive->kind) {
        case 5:
            if (!(request_XC->flags & 2)) {
                break;
            }
            goto check_sphere;
        case 7:
            if (!(request_XC->flags & 16)) {
                break;
            }
            goto check_sphere;
        case 6:
            if (!(request_XC->flags & 8)) {
                break;
            }
        case 1:
        case 3:
        check_sphere:
        {
            float radiusSquared;
            float separationMeasure;
            if (request_XC->flags & 1) {
                break;
            }
            difference.x = position[0] - startMatrix.transform[3][0];
            difference.y = position[1] - startMatrix.transform[3][1];
            difference.z = position[2] - startMatrix.transform[3][2];
            separationMeasure = difference.x * difference.x +
                              difference.y * difference.y +
                              difference.z * difference.z;
            correctionScale = primitive->start.w * jointMatrix[0][3];
            if (separationMeasure == 0.0f) {
                break;
            }
            radiusSquared = correctionScale * correctionScale;
            if (primitive->kind == 3) {
                if (radiusSquared < separationMeasure) {
                    float correctedX;
                    float correctedY;
                    float correctedZ;
                    correctionScale = correctionScale / __builtin_sqrtf(separationMeasure) - 1.0f;
                    count++;
                    correctedX = vel_0[0] + difference.x * correctionScale;
                    correctedY = vel_0[1] + difference.y * correctionScale;
                    correctedZ = vel_0[2] + difference.z * correctionScale;
                    vel_0[2] = correctedZ;
                    vel_0[0] = correctedX;
                    vel_0[1] = correctedY;
                }
            } else {
                if (separationMeasure < radiusSquared) {
                    float correctedX;
                    float correctedY;
                    float correctedZ;
                    correctionScale = correctionScale / __builtin_sqrtf(separationMeasure) - 1.0f;
                    count++;
                    correctedX = vel_0[0] + difference.x * correctionScale;
                    correctedY = vel_0[1] + difference.y * correctionScale;
                    correctedZ = vel_0[2] + difference.z * correctionScale;
                    vel_0[2] = correctedZ;
                    vel_0[0] = correctedX;
                    vel_0[1] = correctedY;
                }
            }
            break;
        }
        case 4:
            if (request_XC->flags & 1) {
                float radiusSquared;
                float separationMeasure;
                difference.x = position[0] - startMatrix.transform[3][0];
                difference.y = position[1] - startMatrix.transform[3][1];
                difference.z = position[2] - startMatrix.transform[3][2];
                separationMeasure = difference.x * difference.x +
                                  difference.y * difference.y +
                                  difference.z * difference.z;
                correctionScale = primitive->start.w * jointMatrix[0][3];
                if (separationMeasure == 0.0f) {
                    break;
                }
                radiusSquared = correctionScale * correctionScale;
                if (separationMeasure < radiusSquared) {
                    correctionScale = correctionScale / __builtin_sqrtf(separationMeasure) - 1.0f;
                    count++;
                    {
                        float correctedX = vel_0[0] + difference.x * correctionScale;
                        float correctedY = vel_0[1] + difference.y * correctionScale;
                        float correctedZ = vel_0[2] + difference.z * correctionScale;
                        vel_0[2] = correctedZ;
                        vel_0[0] = correctedX;
                        vel_0[1] = correctedY;
                    }
                }
            }
            break;
        case 2:
            if (!(request_XC->flags & 1)) {
                JntMatrix_XC endMatrix;
                Vector4 axis;
                float projection;
                float axisOriginDot;
                float pointProjection;
                float capsuleRadius;
                float separationMeasure;
                float originX;
                float originY;
                float originZ;
                __builtin_memcpy(endMatrix.transform, startMatrix.transform, sizeof(endMatrix.transform));
                MATRIX_translate4s(endMatrix.transform, primitive->end.x,
                                   primitive->end.y, primitive->end.z);
                originX = startMatrix.transform[3][0];
                originY = startMatrix.transform[3][1];
                axis.x = endMatrix.transform[3][0] - originX;
                axis.y = endMatrix.transform[3][1] - originY;
                originZ = startMatrix.transform[3][2];
                axis.z = endMatrix.transform[3][2] - originZ;
                axisOriginDot = axis.x * originX;
                axisOriginDot += axis.y * originY;
                axisOriginDot += axis.z * originZ;
                projection = -axisOriginDot;
                pointProjection = axis.x * position[0] + axis.y * position[1];
                pointProjection += axis.z * position[2];
                projection += pointProjection;
                separationMeasure = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
                if (separationMeasure != 0.0f) {
                    projection /= separationMeasure;
                } else {
                    projection = 0.0f;
                }
                if (projection <= -0.1f) {
                    break;
                }
                if (projection >= 1.1f) {
                    break;
                }
                axis.x = position[0] - (originX + axis.x * projection);
                axis.y = position[1] - (originY + axis.y * projection);
                axis.z = position[2] - (originZ + axis.z * projection);
                separationMeasure = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
                {
                    float startRadius = primitive->start.w;
                    float endRadius = primitive->end.w;
                    if (endRadius == startRadius) {
                        capsuleRadius = startRadius;
                    } else {
                        capsuleRadius = startRadius + (endRadius - startRadius) * projection;
                    }
                }
                capsuleRadius = projection <= 0.0f ?
                    capsuleRadius * ((projection + 0.1f) * 10.0f) : capsuleRadius;
                capsuleRadius = projection >= 1.0f ?
                    capsuleRadius * ((1.1f - projection) * 10.0f) : capsuleRadius;
                capsuleRadius *= jointMatrix[0][3];
                if (separationMeasure < capsuleRadius * capsuleRadius) {
                    if (separationMeasure != 0.0f) {
                        separationMeasure = capsuleRadius / __builtin_sqrtf(separationMeasure) - 1.0f;
                        {
                            float correctedX = vel_0[0] + axis.x * separationMeasure;
                            float correctedY = vel_0[1] + axis.y * separationMeasure;
                            float correctedZ = vel_0[2] + axis.z * separationMeasure;
                            vel_0[2] = correctedZ;
                            vel_0[0] = correctedX;
                            vel_0[1] = correctedY;
                        }
                    }
                    count++;
                }
            }
            break;
        }
        index++;
    }
    if (count != 0) {
        float averageX;
        float averageY;
        float averageZ;
        correctionScale = 1.0f / count;
        averageX = vel_0[0] * correctionScale;
        averageY = vel_0[1] * correctionScale;
        averageZ = vel_0[2] * correctionScale;
        vel_0[0] = averageX;
        vel_0[2] = averageZ;
        vel_0[1] = averageY;
    }
    request_XC->correction = vel_0;
    return count;
}

float xglSin(float angle);
float xglCos(float angle);
typedef unsigned int XglQuadword_XS __attribute__((mode(TI)));
typedef union Matrix_XS {
    float elements[16];
    XglQuadword_XS quadwords[4];
} Matrix_XS;
typedef struct Actor_XS {
    u32 flags;
    void (*update)(struct Actor_XS *actor);
    void (*draw)(struct Actor_XS *actor);
    u32 quadword_alignment_gap;
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    Vector4 acceleration;
    Vector4 rotation;
    Vector4 scale;
    unsigned char unmodeled_070[0x4dc - 0x70];
    float ground_height;
    unsigned char unmodeled_4e0[0xa40 - 0x4e0];
    unsigned char moved_hair_mode;
    unsigned char unmodeled_a41[3];
    float moved_hair_delay;
    unsigned char unmodeled_a48[8];
    float moved_hair_sway[4];
    Matrix_XS *previous_joint_matrices;
} Actor_XS;
typedef struct ExMotionJointState_XS {
    unsigned char unmodeled_00[16];
    Vector4 translation;
    unsigned char unmodeled_20[16];
    Vector4 rotation;
} ExMotionJointState_XS;
typedef struct ExMotionJointWork_XS {
    unsigned char unmodeled_000[0x28];
    struct JntHairIdSource *hair_id_source;
    int current_index;
    unsigned char unmodeled_030[8];
    unsigned int flags;
    unsigned char unmodeled_03c[0xa0 - 0x3c];
    ExMotionJointState_XS channels[16];
    unsigned char unmodeled_4a0[0x20];
    unsigned short matrix_index;
    short saved_index;
    unsigned char unmodeled_4c4[0x4d0 - 0x4c4];
    Matrix_XS saved_matrix;
    unsigned char unmodeled_510[0x720 - 0x510];
    Matrix_XS root_matrix;
    unsigned char unmodeled_760[0x7a0 - 0x760];
    struct JntMatrix_XS *matrix_buffer;
    unsigned char unmodeled_7a4[0x80c - 0x7a4];
    signed char *hair_joint_name;
    unsigned int animation_flags;
    unsigned char unmodeled_814[0x820 - 0x814];
    struct Actor_XS *owner;
} ExMotionJointWork_XS;
typedef struct JntMatrix_XS {
    Matrix4 transform;
} JntMatrix_XS;
void xglVectorNormal(Vector4 *destination, const Vector4 *source);
extern int rootBorn;

static void CheckSkirtCollisionSub(ExMotionJointWork_XS *work, float position[4],
                            float referencePosition[4], u32 flags)
{
    float orientationSin;
    float orientationCos;
    float responseX;
    float responseZ;
    float distanceScale;
    float lengthMeasure;
    float firstZ;
    float (*firstStart)[4];
    float (*firstEnd)[4];
    float (*secondStart)[4];
    float (*secondEnd)[4];
    u32 profileFlags;
    {
        const float defaultResponse = 0.2f;
        float horizontalResponse = defaultResponse;
        float depthResponse = defaultResponse;
        float profileAngle = 0.0f;
        profileFlags = flags;
        if (profileFlags & 0x800) {
            if (profileFlags & 0x1000) {
                const float selectedProfileResponse = 0.7f;
                const float selectedProfileAngle = 2.3561945f;
                horizontalResponse = selectedProfileResponse;
                profileAngle = selectedProfileAngle;
                depthResponse = selectedProfileResponse;
            } else if (profileFlags & 0x2000) {
                const float selectedProfileResponse = 0.7f;
                const float selectedProfileAngle = 3.926991f;
                horizontalResponse = selectedProfileResponse;
                profileAngle = selectedProfileAngle;
                depthResponse = selectedProfileResponse;
            } else {
                const float selectedProfileAngle = 3.1415927f;
                horizontalResponse = 1.0f;
                profileAngle = selectedProfileAngle;
            }
        } else if (profileFlags & 0x4000) {
            if (profileFlags & 0x1000) {
                horizontalResponse = 0.7f;
                profileAngle = 0.7853982f;
                depthResponse = horizontalResponse;
            } else if (profileFlags & 0x2000) {
                horizontalResponse = 0.7f;
                profileAngle = 5.4977875f;
                depthResponse = horizontalResponse;
            } else {
                horizontalResponse = 1.0f;
            }
        } else if (profileFlags & 0x1000) {
            depthResponse = 1.0f;
            profileAngle = 1.5707964f;
        } else {
            depthResponse = 1.0f;
            profileAngle = 4.712389f;
        }
        {
            const int reverseSide = flags & 0x100;
            JntMatrix_XS basis;
            JntMatrix_XS *joints;
            float (*originRow)[4];
            if (reverseSide) {
                __builtin_memcpy(basis.transform, work->matrix_buffer[14].transform, sizeof(basis.transform));
                basis.transform[3][0] = basis.transform[3][1] = basis.transform[3][2] = 0.0f;
                MATRIX_translate4s(basis.transform, 0.0f, 0.0f, 1.0f);
                orientationCos = basis.transform[3][0];
                orientationSin = -basis.transform[3][2];
            } else {
                __builtin_memcpy(basis.transform, work->matrix_buffer[36].transform, sizeof(basis.transform));
                basis.transform[3][0] = basis.transform[3][1] = basis.transform[3][2] = 0.0f;
                MATRIX_translate4s(basis.transform, 0.0f, 0.0f, 1.0f);
                orientationCos = basis.transform[3][0];
                orientationSin = -basis.transform[3][2];
            }
            joints = work->matrix_buffer;
            originRow = &joints[13].transform[3];
            switch (rootBorn) {
            case 0:
                firstStart = &joints[16].transform[3];
                secondStart = &joints[26].transform[3];
                firstEnd = &joints[17].transform[3];
                secondEnd = &joints[27].transform[3];
                break;
            case 2:
                firstStart = &joints[54].transform[3];
                secondStart = &joints[66].transform[3];
                firstEnd = &joints[56].transform[3];
                secondEnd = &joints[68].transform[3];
                break;
            case 3:
                firstStart = &joints[54].transform[3];
                secondStart = &joints[66].transform[3];
                firstEnd = &joints[60].transform[3];
                secondEnd = &joints[72].transform[3];
                break;
            case 4:
                firstStart = &joints[16].transform[3];
                secondStart = &joints[29].transform[3];
                firstEnd = &joints[17].transform[3];
                secondEnd = &joints[30].transform[3];
                break;
            case 5:
                firstStart = &joints[16].transform[3];
                secondStart = &joints[29].transform[3];
                firstEnd = &joints[20].transform[3];
                secondEnd = &joints[33].transform[3];
                break;
            case 6:
                firstStart = &joints[16].transform[3];
                secondStart = &joints[26].transform[3];
                firstEnd = &joints[18].transform[3];
                secondEnd = &joints[28].transform[3];
                break;
            case 1:
            case 7:
                firstStart = &joints[16].transform[3];
                secondStart = &joints[26].transform[3];
                firstEnd = &joints[20].transform[3];
                secondEnd = &joints[30].transform[3];
                break;
            default:
                return;
            }
            {
                if (reverseSide) {
                    float angle = -1.5707964f - profileAngle;
                    float sine;
                    responseZ = xglCos(angle);
                    sine = xglSin(angle);
                    __builtin_memcpy(&basis.transform, &work->matrix_buffer[14].transform, sizeof(basis.transform));
                    MATRIX_translate4s(basis.transform, 0.0f, responseZ, sine);
                    {
                        float (*translatedOrigin)[4] = &basis.transform[3];
                        responseX = (*translatedOrigin)[0] - (*originRow)[0];
                        {
                            float profileDepth = (*translatedOrigin)[2];
                            distanceScale = profileDepth - (*originRow)[2];
                        }
                    }
                    responseZ = orientationCos * distanceScale + orientationSin * responseX;
                    responseX = orientationCos * responseX - orientationSin * distanceScale;
                } else {
                    float sine;
                    responseZ = xglCos(profileAngle);
                    sine = xglSin(profileAngle);
                    __builtin_memcpy(&basis.transform, &work->matrix_buffer[13].transform, sizeof(basis.transform));
                    MATRIX_translate4s(basis.transform, 0.0f, responseZ, sine);
                    {
                        float (*translatedOrigin)[4] = &basis.transform[3];
                        distanceScale = (*translatedOrigin)[0] - (*originRow)[0];
                        responseX = (*translatedOrigin)[2] - (*originRow)[2];
                    }
                    responseZ = orientationCos * distanceScale - orientationSin * responseX;
                    responseX = orientationCos * responseX + orientationSin * distanceScale;
                }
            }
        }
        {
            JntMatrix_XS *motionMatrices = work->matrix_buffer;
            position[0] -= motionMatrices[11].transform[3][0];
            position[2] -= motionMatrices[11].transform[3][2];
            (*firstStart)[0] -= motionMatrices[11].transform[3][0];
            (*firstStart)[2] -= motionMatrices[11].transform[3][2];
            (*secondStart)[0] -= motionMatrices[11].transform[3][0];
            (*secondStart)[2] -= motionMatrices[11].transform[3][2];
            (*firstEnd)[0] -= motionMatrices[11].transform[3][0];
            (*firstEnd)[2] -= motionMatrices[11].transform[3][2];
            (*secondEnd)[0] -= motionMatrices[11].transform[3][0];
            (*secondEnd)[2] -= motionMatrices[11].transform[3][2];
        }
        {
            Vector4 normalizedPosition;
            normalizedPosition.x = position[0];
            normalizedPosition.y = position[1];
            normalizedPosition.z = position[2];
            xglVectorNormal(&normalizedPosition, &normalizedPosition);
        }
        {
            float deltaX = position[0] - (*firstStart)[0];
            float deltaY = position[1] - (*firstStart)[1];
            float deltaZ = position[2] - (*firstStart)[2];
            float firstX;
            float segmentScale;
            float fullFootRadius;
            deltaX *= deltaX;
            deltaY *= deltaY;
            deltaZ *= deltaZ;
            {
                const float firstPointDistanceSquared = deltaX + deltaY + deltaZ;
                distanceScale = __builtin_sqrtf(firstPointDistanceSquared);
            }
            {
                float segmentX = (*firstEnd)[0] - (*firstStart)[0];
                float segmentY = (*firstEnd)[1] - (*firstStart)[1];
                float segmentZ = (*firstEnd)[2] - (*firstStart)[2];
                segmentX *= segmentX;
                segmentY *= segmentY;
                segmentZ *= segmentZ;
                lengthMeasure = distanceScale / __builtin_sqrtf(segmentX + segmentY + segmentZ);
            }
            segmentScale = skirtColliSize;
            firstX = ((*firstEnd)[0] - (*firstStart)[0]) * lengthMeasure + (*firstStart)[0];
            firstZ = ((*firstEnd)[2] - (*firstStart)[2]) * lengthMeasure + (*firstStart)[2];
            if (profileAngle < 1.5707964f || 4.712389f < profileAngle) {
                segmentScale *= skirtColliSizeB;
            }
            if (profileAngle < 2.3561945f && 0.7853982f < profileAngle) {
                segmentScale *= skirtColliSizeS;
            } else if (3.926991f < profileAngle && profileAngle < 5.4977875f) {
                segmentScale *= skirtColliSizeS;
            }
            segmentScale = lengthMeasure > 1.0f ? segmentScale / lengthMeasure : segmentScale;
            firstX *= segmentScale;
            {
                float pointX = position[0] - (*secondStart)[0];
                float pointY = position[1] - (*secondStart)[1];
                float pointZ = position[2] - (*secondStart)[2];
                firstZ *= segmentScale;
                pointX *= pointX;
                pointY *= pointY;
                pointZ *= pointZ;
                distanceScale = __builtin_sqrtf(pointX + pointY + pointZ);
            }
            {
                float segmentX = (*secondEnd)[0] - (*secondStart)[0];
                float segmentY = (*secondEnd)[1] - (*secondStart)[1];
                float segmentZ = (*secondEnd)[2] - (*secondStart)[2];
                segmentX *= segmentX;
                segmentY *= segmentY;
                segmentZ *= segmentZ;
                lengthMeasure = distanceScale / __builtin_sqrtf(segmentX + segmentY + segmentZ);
            }
            segmentScale = skirtColliSize;
            {
                float secondX = ((*secondEnd)[0] - (*secondStart)[0]) * lengthMeasure + (*secondStart)[0];
                float secondZ = ((*secondEnd)[2] - (*secondStart)[2]) * lengthMeasure + (*secondStart)[2];
                if (profileAngle < 1.5707964f || 4.712389f < profileAngle) {
                    segmentScale *= skirtColliSizeB;
                }
                if (profileAngle < 2.3561945f && 0.7853982f < profileAngle) {
                    segmentScale *= skirtColliSizeS;
                } else if (3.926991f < profileAngle && profileAngle < 5.4977875f) {
                    segmentScale *= skirtColliSizeS;
                }
                segmentScale = lengthMeasure > 1.0f ? segmentScale / lengthMeasure : segmentScale;
                secondZ *= segmentScale;
                secondX *= segmentScale;
                distanceScale = firstX;
                firstX = orientationCos * distanceScale - orientationSin * firstZ;
                firstZ = orientationCos * firstZ + orientationSin * distanceScale;
                distanceScale = secondX;
                secondX = orientationCos * distanceScale - orientationSin * secondZ;
                secondZ = orientationCos * secondZ + orientationSin * distanceScale;
                if (firstX < secondX) {
                    distanceScale = firstX;
                    firstX = secondX;
                    secondX = distanceScale;
                }
                if (firstZ < secondZ) {
                    distanceScale = firstZ;
                    firstZ = secondZ;
                    secondZ = distanceScale;
                }
                fullFootRadius = skirtColliFootRad;
                {
                    const float halfFootRadius = skirtColliFootRad * 0.5f;
                    firstX += halfFootRadius;
                    secondX -= halfFootRadius;
                    firstZ += fullFootRadius;
                    secondZ -= fullFootRadius;
                    distanceScale = orientationCos * position[0] - orientationSin * position[2];
                    {
                        {
                            lengthMeasure = orientationCos * position[2] + orientationSin * position[0];
                            if (responseZ > 0.0f) {
                                if (distanceScale < firstX) {
                                    float localXDisplacement = (firstX - distanceScale) * responseZ * depthResponse;
                                    distanceScale += localXDisplacement;
                                }
                            } else if (responseZ < 0.0f) {
                                if (secondX < distanceScale) {
                                    float localXDisplacement = (distanceScale - secondX) * responseZ * depthResponse;
                                    distanceScale += localXDisplacement;
                                }
                            }
                            if (responseX > 0.0f) {
                                if (lengthMeasure < firstZ) {
                                    float localZDisplacement = (firstZ - lengthMeasure) * responseX * horizontalResponse;
                                    lengthMeasure += localZDisplacement;
                                }
                            } else if (responseX < 0.0f) {
                                if (secondZ < lengthMeasure) {
                                    float localZDisplacement = (lengthMeasure - secondZ) * responseX * horizontalResponse;
                                    lengthMeasure += localZDisplacement;
                                }
                            }
                            position[0] = orientationCos * distanceScale + orientationSin * lengthMeasure;
                            position[2] = orientationCos * lengthMeasure - orientationSin * distanceScale;
                        }
                    }
                }
            }
        }
    }
    if (flags & 0x10000) {
        if (rootBorn == 0) {
            float (*endpoint)[4] = 0;
            position[1] -= work->matrix_buffer[13].transform[3][1];
            (*firstEnd)[1] -= work->matrix_buffer[13].transform[3][1];
            (*secondEnd)[1] -= work->matrix_buffer[13].transform[3][1];
            referencePosition[0] -= work->matrix_buffer[11].transform[3][0];
            referencePosition[1] -= work->matrix_buffer[13].transform[3][1];
            referencePosition[2] -= work->matrix_buffer[11].transform[3][2];
            for (; endpoint != secondEnd;) {
                if (endpoint == 0) {
                    endpoint = firstEnd;
                } else {
                    endpoint = secondEnd;
                }
                firstZ = orientationCos * (*endpoint)[2] + orientationSin * (*endpoint)[0];
                if (firstZ < 0.0f) {
                    continue;
                }
                {
                    float referenceRadius;
                    lengthMeasure = __builtin_sqrtf((*endpoint)[0] * (*endpoint)[0] + (*endpoint)[2] * (*endpoint)[2]);
                    referenceRadius = __builtin_sqrtf(referencePosition[0] * referencePosition[0] + referencePosition[2] * referencePosition[2]);
                    if (!(referenceRadius < lengthMeasure)) {
                        continue;
                    }
                    distanceScale = __builtin_sqrtf(position[0] * position[0] + position[2] * position[2]);
                    if (distanceScale != 0.0f) {
                        distanceScale = lengthMeasure / distanceScale;
                    } else {
                        distanceScale = 1.0f;
                    }
                    {
                        const float pointHeight = position[1];
                        lengthMeasure = pointHeight * distanceScale;
                        if (distanceScale != 0.0f) {
                            float minimumHeight;
                            minimumHeight = (*endpoint)[1] + skirtColliFootRad + skirtColliFootRad;
                            if (lengthMeasure < minimumHeight) {
                                position[1] = minimumHeight / distanceScale;
                            }
                        }
                    }
                }
            }
            position[1] += work->matrix_buffer[13].transform[3][1];
            (*firstEnd)[1] += work->matrix_buffer[13].transform[3][1];
            (*secondEnd)[1] += work->matrix_buffer[13].transform[3][1];
            referencePosition[0] += work->matrix_buffer[11].transform[3][0];
            referencePosition[1] += work->matrix_buffer[13].transform[3][1];
            referencePosition[2] += work->matrix_buffer[11].transform[3][2];
        } else if (rootBorn == 6) {
            float (*endpoint)[4] = 0;
            position[1] -= work->matrix_buffer[13].transform[3][1];
            (*firstEnd)[1] -= work->matrix_buffer[13].transform[3][1];
            (*secondEnd)[1] -= work->matrix_buffer[13].transform[3][1];
            while (endpoint != secondEnd) {
                if (endpoint == 0) {
                    endpoint = firstEnd;
                } else {
                    endpoint = secondEnd;
                }
                lengthMeasure = __builtin_sqrtf((*endpoint)[0] * (*endpoint)[0] + (*endpoint)[2] * (*endpoint)[2]);
                distanceScale = __builtin_sqrtf(position[0] * position[0] + position[2] * position[2]);
                if (distanceScale != 0.0f) {
                    distanceScale = lengthMeasure / distanceScale;
                } else {
                    distanceScale = 1.0f;
                }
                    {
                        const float pointHeight = position[1];
                        lengthMeasure = pointHeight * distanceScale;
                        if (distanceScale != 0.0f) {
                            float minimumHeight;
                            minimumHeight = (*endpoint)[1] + skirtColliFootRad + skirtColliFootRad;
                            if (lengthMeasure < minimumHeight) {
                                position[1] = minimumHeight / distanceScale;
                            }
                        }
                    }
            }
            position[1] += work->matrix_buffer[13].transform[3][1];
            (*firstEnd)[1] += work->matrix_buffer[13].transform[3][1];
            (*secondEnd)[1] += work->matrix_buffer[13].transform[3][1];
        }
    }
    if ((flags & 0x20000) && rootBorn == 1) {
        float (*endpoint)[4] = 0;
        position[1] -= work->matrix_buffer[13].transform[3][1];
        (*firstEnd)[1] -= work->matrix_buffer[13].transform[3][1];
        (*secondEnd)[1] -= work->matrix_buffer[13].transform[3][1];
        referencePosition[0] -= work->matrix_buffer[11].transform[3][0];
        referencePosition[1] -= work->matrix_buffer[13].transform[3][1];
        referencePosition[2] -= work->matrix_buffer[11].transform[3][2];
        for (; endpoint != secondEnd;) {
            if (endpoint == 0) {
                endpoint = firstEnd;
            } else {
                endpoint = secondEnd;
            }
            firstZ = orientationCos * (*endpoint)[2] + orientationSin * (*endpoint)[0];
            if (firstZ > 0.0f) {
                continue;
            }
            {
                float referenceRadius;
                lengthMeasure = __builtin_sqrtf((*endpoint)[0] * (*endpoint)[0] + (*endpoint)[2] * (*endpoint)[2]);
                referenceRadius = __builtin_sqrtf(referencePosition[0] * referencePosition[0] + referencePosition[2] * referencePosition[2]);
                if (!(referenceRadius < lengthMeasure)) {
                    continue;
                }
                distanceScale = __builtin_sqrtf(position[0] * position[0] + position[2] * position[2]);
                if (distanceScale != 0.0f) {
                    distanceScale = lengthMeasure / distanceScale;
                } else {
                    distanceScale = 1.0f;
                }
                {
                    const float pointHeight = position[1];
                    lengthMeasure = pointHeight * distanceScale;
                    if (distanceScale != 0.0f) {
                        float minimumHeight;
                        minimumHeight = (*endpoint)[1] + skirtColliFootRad + skirtColliFootRad;
                        if (lengthMeasure < minimumHeight) {
                            position[1] = minimumHeight / distanceScale;
                        }
                    }
                }
            }
        }
        position[1] += work->matrix_buffer[13].transform[3][1];
        (*firstEnd)[1] += work->matrix_buffer[13].transform[3][1];
        (*secondEnd)[1] += work->matrix_buffer[13].transform[3][1];
        referencePosition[0] += work->matrix_buffer[11].transform[3][0];
        referencePosition[1] += work->matrix_buffer[13].transform[3][1];
        referencePosition[2] += work->matrix_buffer[11].transform[3][2];
    }
    {
        JntMatrix_XS *motionMatrices = work->matrix_buffer;
        position[0] += motionMatrices[11].transform[3][0];
        position[2] += motionMatrices[11].transform[3][2];
        (*firstStart)[0] += motionMatrices[11].transform[3][0];
        (*firstStart)[2] += motionMatrices[11].transform[3][2];
        (*secondStart)[0] += motionMatrices[11].transform[3][0];
        (*secondStart)[2] += motionMatrices[11].transform[3][2];
        (*firstEnd)[0] += motionMatrices[11].transform[3][0];
        (*firstEnd)[2] += motionMatrices[11].transform[3][2];
        (*secondEnd)[0] += motionMatrices[11].transform[3][0];
        (*secondEnd)[2] += motionMatrices[11].transform[3][2];
    }
}

extern int rootBorn;
void CheckSkirtCollisionSub();

static void EXM_CheckSkirtCollision(int arg0, int arg1, int arg2, int arg3)
{
    if (arg3 & 0x80) {
        rootBorn = 4;
        CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
        rootBorn = 5;
        CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
        return;
    }
    if (arg3 & 0x100) {
        rootBorn = 6;
        CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
        rootBorn = 7;
        CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
        return;
    }
    rootBorn = 0;
    CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
    rootBorn = 1;
    CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
    if (!(arg3 & 0x8000)) {
        return;
    }
    rootBorn = 2;
    CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
    rootBorn = 3;
    CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
}

static void AdjustJrCoatWidth(float *a, float *b, float width)
{
    float d[4];
    float t;
    float x, y, z;

    d[0] = a[0] - b[0];
    d[1] = a[1] - b[1];
    d[2] = a[2] - b[2];
    xglVectorLength(&d[3], (Vector4 *)d);
    if (d[3] == 0.0f) {
        d[3] = 1.0f;
    }
    t = width / d[3];
    x = (b[0] + d[0] * t) * 0.5f + a[0] * 0.5f;
    y = (b[1] + d[1] * t) * 0.5f + a[1] * 0.5f;
    z = (b[2] + d[2] * t) * 0.5f + a[2] * 0.5f;
    a[0] = x;
    a[1] = y;
    a[2] = z;
}

float atan2f(float y, float x);
void xglMatrixInverse(Matrix4 destination, const Matrix4 source);
typedef unsigned int XglQuadword_XM __attribute__((mode(TI)));
typedef union Matrix_XM {
    float elements[16];
    XglQuadword_XM quadwords[4];
} Matrix_XM;
typedef struct Actor_XM {
    u32 flags;
    void (*update)(struct Actor_XM *actor);
    void (*draw)(struct Actor_XM *actor);
    u32 quadword_alignment_gap;
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    Vector4 acceleration;
    Vector4 rotation;
    Vector4 scale;
    unsigned char unmodeled_070[0x4dc - 0x70];
    float ground_height;
    unsigned char unmodeled_4e0[0xa40 - 0x4e0];
    unsigned char moved_hair_mode;
    unsigned char unmodeled_a41[3];
    float moved_hair_delay;
    unsigned char unmodeled_a48[8];
    float moved_hair_sway[4];
    Matrix_XM *previous_joint_matrices;
} Actor_XM;
typedef struct ExMotionJointState_XM {
    unsigned char unmodeled_00[16];
    Vector4 translation;
    unsigned char unmodeled_20[16];
    Vector4 rotation;
} ExMotionJointState_XM;
typedef struct ExMotionJointWork_XM {
    unsigned char unmodeled_000[0x28];
    struct JntHairIdSource *hair_id_source;
    int current_index;
    unsigned char unmodeled_030[8];
    unsigned int flags;
    unsigned char unmodeled_03c[0xa0 - 0x3c];
    ExMotionJointState_XM channels[16];
    unsigned char unmodeled_4a0[0x20];
    unsigned short matrix_index;
    short saved_index;
    unsigned char unmodeled_4c4[0x4d0 - 0x4c4];
    Matrix_XM saved_matrix;
    unsigned char unmodeled_510[0x720 - 0x510];
    Matrix_XM root_matrix;
    unsigned char unmodeled_760[0x7a0 - 0x760];
    struct JntMatrix_XM *matrix_buffer;
    unsigned char unmodeled_7a4[0x80c - 0x7a4];
    signed char *hair_joint_name;
    unsigned int animation_flags;
    unsigned char unmodeled_814[0x820 - 0x814];
    struct Actor_XM *owner;
} ExMotionJointWork_XM;
typedef struct ExHairTypeSelection_XM {
    int kind;
    unsigned int flags;
} ExHairTypeSelection_XM;
extern void *collibuf[1];
extern int ibuf;
typedef struct ExCollisionPrimitive_XM {
    unsigned short joint;
    unsigned short kind;
    unsigned char unmodeled_004[12];
    Vector4 start;
    Vector4 end;
} ExCollisionPrimitive_XM;
typedef struct ExCollisionRequest_XM {
    struct ExCollisionPrimitive_XM *primitives;
    unsigned int flags;
    float *correction;
} ExCollisionRequest_XM;
typedef struct JntMatrix_XM {
    Matrix4 transform;
} JntMatrix_XM;
void xglMatrixMul(Matrix_XM *destination, Matrix_XM *first, Matrix_XM *second);
void xglVectorInner(float *result, const Vector4 *first, const Vector4 *second);
void MATRIX_rotX4s(Matrix_XM *matrix, float angle);
void MATRIX_rotY4s(Matrix_XM *matrix, float angle);
void MATRIX_rotZ4s(Matrix_XM *matrix, float angle);
void EXM_ResetWave(void);
void UnduParamInit(void *query);
float asinf(float value);
typedef struct ExMotionChannel_XM {
    unsigned short type;
    unsigned char unmodeled_002[12];
    unsigned short matrix_index;
    Vector4 translation;
    Vector4 rotation;
    unsigned char unmodeled_030[16];
} ExMotionChannel_XM;
typedef struct ExHairParameters_XM {
    float gravity;
    float length_response;
    unsigned char unmodeled_008[8];
    float response[3];
    unsigned char unmodeled_01c[4];
    float chain_response;
    unsigned char unmodeled_024[12];
} ExHairParameters_XM;
typedef struct ExGroundQuery_XM {
    unsigned char unmodeled_00[8];
    short attribute_mask;
    unsigned char unmodeled_0a[0x40 - 10];
} ExGroundQuery_XM;
float UnduCheck(const void *position, void *velocity, ExGroundQuery_XM *query);
extern ExHairParameters_XM hairpara[];
extern float JR_coatAdjust[];
void EXM_GetWindPower(float output[3], ExMotionJointWork_XM *work, const float input[3],
                      const float parent_position[4]);
typedef struct HairSelectionWork_XM {
    unsigned char unmodeled_000[0x4c2];
    s16 hair_type_code;
    unsigned char unmodeled_4c4[0x80c - 0x4c4];
    signed char *hair_joint_name;
} HairSelectionWork_XM;
static inline Matrix_XM *CopyExMatrix(float *destination, const float *source)
{
    return __builtin_memcpy(destination, source, sizeof(Matrix_XM));
}
static inline float *ExMatrixData(Matrix_XM *matrix)
{
    return matrix->elements;
}
typedef float ExMatrixRow_XM[4];
static inline ExMatrixRow_XM *ExMatrixRows(Matrix_XM *matrix)
{
    return (ExMatrixRow_XM *)(void *)matrix->elements;
}
static inline const ExMatrixRow_XM *ExMatrixConstRows(const Matrix_XM *matrix)
{
    return (const ExMatrixRow_XM *)(const void *)matrix->elements;
}
static inline Matrix_XM *CopyExMatrixObject(Matrix_XM *destination, const Matrix_XM *source)
{
    *destination = *source;
    return destination;
}
static inline Vector4 *TransformExDirection(const Matrix_XM *matrix,
                                        const Vector4 *source,
                                        Vector4 *destination)
{
    float x = source->x;
    float y = source->y;
    float z = source->z;
    destination->x = matrix->elements[0] * x + matrix->elements[1] * y + matrix->elements[2] * z;
    destination->y = matrix->elements[4] * x + matrix->elements[5] * y + matrix->elements[6] * z;
    destination->z = matrix->elements[8] * x + matrix->elements[9] * y + matrix->elements[10] * z;
    return destination;
}
static inline Matrix_XM *ExMotionMatrices(ExMotionJointWork_XM *work)
{
    return (Matrix_XM *)work->matrix_buffer;
}
static inline void OrientExMotionJoint(ExMotionJointWork_XM *work, int index,
                                      Vector4 *direction)
{
    Matrix_XM *current = ExMotionMatrices(work) + work->saved_index + index;
    Vector4 *transformed;
    float horizontalLength;
    float length;
    float yawSin;
    float yawCos;
    float pitchAngle;
    float yawAngle;
    int matrixIndex;
    transformed = TransformExDirection(current, direction, direction);
    horizontalLength = __builtin_sqrtf(transformed->x * transformed->x + transformed->y * transformed->y);
    length = __builtin_sqrtf(transformed->x * transformed->x + transformed->y * transformed->y + transformed->z * transformed->z);
    yawSin = -transformed->y / horizontalLength;
    yawCos = transformed->x / horizontalLength;
    pitchAngle = -atan2f(transformed->z / length, horizontalLength / length);
    MATRIX_rotY4s(&ExMotionMatrices(work)[work->saved_index + index], pitchAngle);
    yawAngle = -atan2f(yawSin, yawCos);
    matrixIndex = work->saved_index + index;
    MATRIX_rotZ4s(&ExMotionMatrices(work)[matrixIndex], yawAngle);
}

ExMotionChannel_XM *EXM_CalcExMotion(ExMotionJointWork_XM *work, ExMotionChannel_XM *channel)
{
    Matrix_XM matrix;
    const Vector4 *translation = &channel->translation;
    const Vector4 *rotation = &channel->rotation;
    float position[4];
    float previousPosition[4];
    Vector4 difference;
    Vector4 direction;
    Vector4 sway;
    Matrix_XM inverse;
    Matrix_XM relative;
    ExGroundQuery_XM ground;
    ExCollisionRequest_XM collision;
    Vector4 firstDirection;
    Vector4 secondDirection;
    unsigned int flags;
    void *collisionTable;
    Actor_XM *owner = work->owner;
    ExMotionChannel_XM *next = channel + 1;
    Matrix_XM *previousMatrices;
    ExHairParameters_XM *parameters;
    ExMotionJointState_XM *state;
    Matrix_XM *currentMatrix;
    int kind;
    int index;
    int jointCount;
    int jointIndex;
    unsigned int baseMatrixIndex;
    float response;
    unsigned short groupStart;
    switch (channel->type) {
    case 3:
        groupStart = work->current_index;
        baseMatrixIndex = channel->matrix_index;
        work->saved_index = groupStart;
        work->matrix_index = 0;
        MATRIX_translate4s(ExMatrixRows(CopyExMatrix(matrix.elements, ExMotionMatrices(work)[baseMatrixIndex].elements)),
                           translation->x, translation->y, translation->z);
        MATRIX_rotZ4s(&matrix, rotation->x);
        MATRIX_rotY4s(&matrix, rotation->y);
        MATRIX_rotX4s(&matrix, rotation->z);
        jointIndex = work->current_index;
        currentMatrix = CopyExMatrixObject(&ExMotionMatrices(work)[jointIndex], &matrix);
        CopyExMatrix(work->saved_matrix.elements, currentMatrix->elements);
        break;
    case 4:
        state = &work->channels[work->matrix_index];
        work->matrix_index++;
        state->translation = *translation;
        collisionTable = 0;
        state->rotation = *rotation;
        flags = 0;
        switch (htypebuf[0].kind) {
        case 999:
            kind = 0;
            flags = 0;
            collisionTable = 0;
            break;
        case 1000:
            kind = GetHairTypeSelection((unsigned int *)&flags, (int **)&collisionTable, (char *)(HairSelectionWork_XM *)(void *)work);
            break;
        default:
            flags = htypebuf[ibuf].flags;
            kind = htypebuf[ibuf].kind;
            collisionTable = collibuf[0];
            ibuf++;
            break;
        }
        parameters = &hairpara[kind];
        if (owner != 0 && (owner->moved_hair_mode & 1) && owner->moved_hair_sway[3] > 0.0f) {
            CopyExMatrix(matrix.elements, ExMotionMatrices(work)[13].elements);
            MATRIX_translate4s(ExMatrixRows(&matrix), 0.0f, 1.0f, 0.0f);
            sway.x = (matrix.elements[12] - ExMotionMatrices(work)[13].elements[12]) * owner->moved_hair_sway[3];
            sway.y = (matrix.elements[13] - ExMotionMatrices(work)[13].elements[13]) * owner->moved_hair_sway[3];
            sway.z = (matrix.elements[14] - ExMotionMatrices(work)[13].elements[14]) * owner->moved_hair_sway[3];
        } else {
            sway.x = 0.0f;
            sway.y = 0.0f;
            sway.z = 0.0f;
        }
        state = &work->channels[0];
        matrix = work->saved_matrix;
        MATRIX_translate4s(ExMatrixRows(&matrix), state->translation.x,
                           state->translation.y, state->translation.z);
        MATRIX_rotZ4s(&matrix, state->rotation.x);
        MATRIX_rotY4s(&matrix, state->rotation.y);
        MATRIX_rotX4s(&matrix, state->rotation.z);
        {
            int matrixIndex = work->saved_index;
            ExMotionMatrices(work)[matrixIndex + 1] = matrix;
        }
        if (owner == 0 || owner->previous_joint_matrices == 0) {
            previousMatrices = ExMotionMatrices(work);
        } else {
            previousMatrices = owner->previous_joint_matrices;
        }
        index = 1;
        EXM_ResetWave();
        jointCount = work->matrix_index;
        for (; index < jointCount; ) {
            Matrix_XM *current = &ExMotionMatrices(work)[work->saved_index + index + 1];
            Matrix_XM *parent;
            float amount;
            float distance;
            state = &work->channels[index];
            if (work->animation_flags & 0x10000) {
                int matrixIndex = work->saved_index;
                const Matrix_XM *previousGroup = &previousMatrices[matrixIndex];
                xglMatrixInverse(ExMatrixRows(&inverse),
                                 ExMatrixConstRows(&previousGroup[1]));
                xglMatrixMul(&relative, &inverse, &previousMatrices[work->saved_index + index + 1]);
                matrixIndex = work->saved_index;
                xglMatrixMul(&inverse, &ExMotionMatrices(work)[matrixIndex + 1], &relative);
                position[0] = previousPosition[0] = inverse.elements[12];
                position[1] = previousPosition[1] = inverse.elements[13];
                position[2] = previousPosition[2] = inverse.elements[14];
            } else {
                position[0] = previousPosition[0] = previousMatrices[work->saved_index + index + 1].elements[12];
                position[1] = previousPosition[1] = previousMatrices[work->saved_index + index + 1].elements[13];
                position[2] = previousPosition[2] = previousMatrices[work->saved_index + index + 1].elements[14];
            }
            if (work->animation_flags & 0x80000) {
                CopyExMatrix(ExMatrixData(current),
                    ExMatrixData(&previousMatrices[work->saved_index + index]));
            } else {
                parent = current - 1;
                CopyExMatrix(ExMatrixData(current), ExMatrixData(parent));
                MATRIX_translate4s(ExMatrixRows(current), state->translation.x, state->translation.y, state->translation.z);
                MATRIX_rotZ4s(current, state->rotation.x);
                MATRIX_rotY4s(current, state->rotation.y);
                MATRIX_rotX4s(current, state->rotation.z);
                if (!(work->animation_flags & 0x200000)) {
                    position[1] -= parameters->gravity;
                }
                if (!(work->animation_flags & 0x100000)) {
                    EXM_GetWindPower(position, work, position, &parent->elements[12]);
                }
                amount = (float)index * 0.3f + 0.4f;
                response = parameters->chain_response / ((float)(index - 1) + parameters->chain_response);
                position[0] += sway.x * amount;
                position[1] += sway.y * amount;
                position[2] += sway.z * amount;
                position[0] += (current->elements[12] - position[0]) * parameters->response[0] * response;
                position[1] += (current->elements[13] - position[1]) * parameters->response[1] * response;
                position[2] += (current->elements[14] - position[2]) * parameters->response[2] * response;
                if (flags & 0x20) {
                    int coatIndex;
                    AdjustJrCoatWidth((float *)((Vector4 *)position), (float *)((Vector4 *)&ExMotionMatrices(work)[index + 74].elements[12]), 0.25f);
                    coatIndex = 0;
                    switch (work->saved_index) {
                    case 91:
                        coatIndex = index + 80;
                        break;
                    case 96:
                        coatIndex = index + 86;
                        break;
                    }
                    if (index < 4) {
                        Vector4 *coatJoint = (Vector4 *)&ExMotionMatrices(work)[coatIndex].elements[12];
                        AdjustJrCoatWidth((float *)(coatJoint), (float *)((Vector4 *)position), JR_coatAdjust[index - 1]);
                    } else {
                        AdjustJrCoatWidth((float *)((Vector4 *)&ExMotionMatrices(work)[coatIndex].elements[12]), (float *)((Vector4 *)position), JR_coatAdjust[2]);
                        AdjustJrCoatWidth((float *)((Vector4 *)&ExMotionMatrices(work)[coatIndex + 1].elements[12]), (float *)((Vector4 *)position), JR_coatAdjust[2]);
                    }
                }
                if (!(work->animation_flags & 0x400000)) {
                    if (owner != 0 && owner->ground_height != -1000.0f) {
                        response = owner->ground_height - owner->position.y + 0.07f;
                    } else {
                        UnduParamInit(&ground);
                        ground.attribute_mask = 0x800;
                        response = UnduCheck(&work->root_matrix.elements[12], 0, &ground) - work->root_matrix.elements[13] + 0.07f;
                    }
                    if (position[1] < response) {
                        position[1] = response;
                    }
                }
                difference.x = position[0] - parent->elements[12];
                difference.y = position[1] - parent->elements[13];
                difference.z = position[2] - parent->elements[14];
                distance = __builtin_sqrtf(difference.x * difference.x + difference.y * difference.y + difference.z * difference.z);
                if (distance != 0.0f) {
                    float weightedX;
                    float weightedY;
                    float weightedZ;
                    response = parameters->length_response / distance;
                    weightedX = response * difference.x;
                    weightedY = response * difference.y;
                    weightedZ = response * difference.z;
                    position[0] += weightedX * (state->translation.x * current->elements[3] - distance);
                    position[1] += weightedY * (state->translation.x * current->elements[7] - distance);
                    position[2] += weightedZ * (state->translation.x * current->elements[11] - distance);
                } else {
                    position[1] -= parameters->gravity;
                }
                if (flags & 0x400) {
                    EXM_CheckSkirtCollision((int)work, (int)position, (int)&ExMotionMatrices(work)[work->saved_index + index].elements[12], flags);
                } else if (collisionTable != 0) {
                    collision.primitives = collisionTable;
                    collision.flags = flags;
                    if (EXM_CheckExCollision((ExMotionJointWork_XC *)work, position, ExMatrixConstRows(current), (ExCollisionRequest_XC *)&collision)) {
                        position[0] = (position[0] - previousPosition[0]) * 0.9f + collision.correction[0] + previousPosition[0];
                        position[1] += collision.correction[1];
                        position[2] = (position[2] - previousPosition[2]) * 0.9f + collision.correction[2] + previousPosition[2];
                    }
                }
            }
            do { (current)->elements[12] = (position)[0]; (current)->elements[13] = (position)[1]; (current)->elements[14] = (position)[2]; } while (0);
            jointCount = work->matrix_index;
            index++;
        }
        if (flags & 1) {
            CopyExMatrix(matrix.elements, ExMotionMatrices(work)[39].elements);
            MATRIX_translate4s(ExMatrixRows(&matrix), 0.0f, 0.0f, 1.0f);
            firstDirection.x = matrix.elements[12] - ExMotionMatrices(work)[39].elements[12];
            firstDirection.y = 0.0001f;
            firstDirection.z = matrix.elements[14] - ExMotionMatrices(work)[39].elements[14];
            CopyExMatrix(matrix.elements, ExMotionMatrices(work)[48].elements);
            MATRIX_translate4s(ExMatrixRows(&matrix), 1.0f, 0.0f, 0.0f);
            secondDirection.x = matrix.elements[12] - ExMotionMatrices(work)[48].elements[12];
            secondDirection.y = 0.0001f;
            secondDirection.z = matrix.elements[14] - ExMotionMatrices(work)[48].elements[14];
            xglVectorNormal(&firstDirection, &firstDirection);
            xglVectorNormal(&secondDirection, &secondDirection);
            xglVectorInner(&response, &firstDirection, &secondDirection);
            response = asinf(response);
            jointCount = work->matrix_index;
        }
        response /= (float)jointCount;
        index = 1;
        if (index < jointCount) {
            do {
                Matrix_XM *current = ExMotionMatrices(work) + work->saved_index + index;
                __asm__ __volatile__(
                    "lqc2 $vf20,0(%0)\n\t"
                    "lqc2 $vf21,0(%1)\n\t"
                    "vsub.xyzw $vf20,$vf20,$vf21\n\t"
                    "sqc2 $vf20,0(%2)"
                    : : "r"(&current[1].elements[12]), "r"(&current->elements[12]), "r"(&direction)
                    : "memory");
                OrientExMotionJoint(work, index, &direction);
                if (flags & 1) {
                    MATRIX_rotX4s(&ExMotionMatrices(work)[work->saved_index + index],
                                   (float)index * response);
                }
                index++;
            } while (index < work->matrix_index);
        }
        jointIndex = work->current_index;
        break;
    case 2:
        jointIndex = work->current_index;
        state = &work->channels[work->matrix_index];
        work->matrix_index++;
        state->translation = *translation;
        state->rotation = *rotation;
        break;
    case 0:
    case 1:
    case 5:
        jointIndex = work->current_index;
        break;
    default:
        jointIndex = work->current_index;
        break;
    }
    work->current_index = jointIndex + 1;
    return next;
}

void EXM_InitMovedHair(Actor *actor)
{
    if (actor != 0) {
        actor->moved_hair_mode = 2;
        actor->moved_hair_delay = 0.5f;
        actor->moved_hair_sway[0] = 0.0f;
        actor->moved_hair_sway[1] = 0.0f;
        actor->moved_hair_sway[2] = 0.0f;
        actor->moved_hair_sway[3] = 0.0f;
    }
}

void EXM_OnMovedHair(Actor *actor)
{
    if (actor != 0) {
        actor->moved_hair_mode = 2;
    }
}

void EXM_OffMovedHair(Actor *actor)
{
    if (actor != 0) {
        actor->moved_hair_mode = 0;
    }
}

void EXM_SetDelayMovedHair(Actor *actor, float delay)
{
    if (actor != 0) {
        actor->moved_hair_delay = delay;
    }
}
