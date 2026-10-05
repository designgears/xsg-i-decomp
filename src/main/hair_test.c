#include "common.h"
#include "shared.h"

/*
 * JntHairDescriptor is the record JNT_hairID's argument points to at +0x28:
 * only the 16-bit hair ID at +8 is evidenced (main VA 0x002da9c0, lw
 * $3,40($4) then lhu $2,8($3)); the rest stays an unmodeled span
 * (docs/naming.md).
 */
typedef struct JntHairDescriptor {
    unsigned char unmodeled_00[8];
    u16 hairID;
} JntHairDescriptor;

typedef struct JntHairJoint {
    unsigned char unmodeled_00[0x28];
    JntHairDescriptor *hairDescriptor;
} JntHairJoint;

/*
 * FpkFcvHeader is the FPK-pack header getNumFCV reads: a magic word whose
 * low 3 bytes read little-endian as "FPK" (main VA 0x002db678, lui
 * $3,0xff / ori $3,$3,0xffff / lui $4,0x4b / ori $4,$4,0x5046 / and
 * $2,$2,$3), and the FCV count at +8, read only when the magic matches.
 * The word in between is not evidenced here and stays an unmodeled span
 * (docs/naming.md).
 */
typedef struct FpkFcvHeader {
    unsigned int magic;
    unsigned char unmodeled_04[4];
    int fcvCount;
} FpkFcvHeader;

/* "FPK" in the low three bytes of FpkFcvHeader.magic (little-endian). */
#define FPK_MAGIC 0x4B5046
#define FPK_MAGIC_MASK 0xFFFFFF

typedef union HairTestVectorBlock {
    Vector4 vector;
    u64 words[2];
} HairTestVectorBlock;

typedef struct HairResourceEntry {
    int resourceId;
    unsigned char unmodeled_04[12];
} HairResourceEntry;

typedef struct HairTestParticle {
    Vector4 position;
    Vector4 previousPosition;
    Vector4 velocity;
    float gravity;
    float damping;
} HairTestParticle;

struct HairTestAct;
typedef struct HairTestActInitial {
    u64 unmodeled_000[2];
    HairTestVectorBlock position;
    unsigned char unmodeled_020[0x30];
    HairTestVectorBlock rotation;
    HairTestVectorBlock scale;
    unsigned char unmodeled_070[0x6b4];
    FpkFcvHeader *fcvHeader;
    unsigned char unmodeled_728[0x68];
    unsigned char jointMatrix;
} HairTestActInitial;

extern int listnum;
extern int listnow;
extern int listpos;
extern int mot;
extern int pause;
extern unsigned char list[];
extern float crx_004DC650;
extern float cry_004DC654;
extern float lookY;
extern HairTestParticle cpos_00585200;
extern const HairTestVectorBlock D_004CBBE0;
extern StudioCamera *pCamera_004DC634;
extern struct HairTestAct *pAct_004DC638;
extern void GameResourceInit(int resourceType, int resourceGroup);
extern void ppInit(HairTestParticle *particle);
extern void ppSetPos(HairTestParticle *particle, float x, float y, float z);
extern void xglStudioInit(void);
extern void xglStudioChange(int studioIndex);
extern void xglStudioMainCameraInit(void);
extern void xglStudioGetCamera(StudioCamera **camera, int cameraIndex);
extern void ACT_init(void);
extern void ACT_resourceInit(void);
extern void ACT_initMotion(struct HairTestAct *actor);
extern void ACT_loadMotion(struct HairTestAct *actor, int resourceId, int category);
extern void ACT_loadResource(struct HairTestAct *actor, int resourceId);
extern void ACT_allocMatrix(struct HairTestAct *actor, int matrixCount);
extern void ACT_setModelWrapper(struct HairTestAct *actor, int flags);
extern struct HairTestAct *ACT_create(int actorIndex, int resourceId);
extern void __JNT_computeMatrix(void *joint, void *matrix);
extern void JNT_addConsumer(void *joint, int consumerIndex,
                            void (*computeMatrix)(void *, void *), int flags);

#define HAIR_RESOURCE(i) (((HairResourceEntry *)list)[(i)].resourceId)

static void InitTest(void)
{
    HairTestVectorBlock initialPosition;
    HairTestVectorBlock initialRotationAndScale[2];
    float cameraX;
    StudioCamera *camera;
    float zero = 0.0f;

    GameResourceInit(0x07000000, 0x06000000);
    cry_004DC654 = zero;
    lookY = 1.0f;
    crx_004DC650 = zero;
    ppInit(&cpos_00585200);
    ppSetPos(&cpos_00585200, 0.0f, 1.0f, 3.0f);
    cpos_00585200.gravity = zero;
    xglStudioInit();
    xglStudioChange(0);
    xglStudioMainCameraInit();
    xglStudioGetCamera(&pCamera_004DC634, 0);
    camera = pCamera_004DC634;
    cameraX = cpos_00585200.position.x;
    camera->position.x = cameraX;
    listpos = 0;
    listnum = 0;
    camera->position.y = cpos_00585200.position.y;
    listnow = 0;
    camera->position.z = cpos_00585200.position.z;
    if (HAIR_RESOURCE(0) != 0) {
        do {
            listnum++;
        } while (HAIR_RESOURCE(listnum) != 0);
    }
    ACT_init();
    ACT_resourceInit();
    pAct_004DC638 = ACT_create(0, HAIR_RESOURCE(listpos));
    ACT_initMotion(pAct_004DC638);
    ACT_loadMotion(pAct_004DC638, HAIR_RESOURCE(listpos), 1);
    ACT_loadResource(pAct_004DC638, HAIR_RESOURCE(listpos));
    ACT_allocMatrix(pAct_004DC638, -1);
    ACT_setModelWrapper(pAct_004DC638, 0);
    memset(&initialPosition, 0, sizeof(initialPosition));
    initialPosition.vector.w = 1.0f;
    memset(&initialRotationAndScale[0], 0, sizeof(initialRotationAndScale[0]));
    initialRotationAndScale[0].vector.w = 1.0f;
    initialRotationAndScale[1] = D_004CBBE0;
    ((HairTestActInitial *)pAct_004DC638)->position = initialPosition;
    ((HairTestActInitial *)pAct_004DC638)->rotation = initialRotationAndScale[0];
    ((HairTestActInitial *)pAct_004DC638)->scale = initialRotationAndScale[1];
    JNT_addConsumer(&((HairTestActInitial *)pAct_004DC638)->jointMatrix,
                    0, __JNT_computeMatrix, 0);
    mot = 0;
    pause = 0;
}

extern int listpos;
extern int mot;
extern int pause;
extern char D_004CBBF0[];
extern char D_004CBC00[];
extern char D_004CBC10[];
extern char D_004CBC20[];
extern unsigned char list[];
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
static int getNumFCV(FpkFcvHeader *header);

/*
 * The object pAct_004DC638 points to: only the FCV header pointer at +0x724
 * is evidenced here (main VA 0x002da3f8, lw $2,pAct_004DC638 / lw
 * $4,0x724($2)); the rest stays an unmodeled span (docs/naming.md).
 */
typedef struct HairTestAct {
    unsigned char unmodeled_000[0x724];
    FpkFcvHeader *fcvHeader;
} HairTestAct;

extern HairTestAct *pAct_004DC638;

static void PrintDisp(void)
{
    xglFontDebugPrintf(0, 0, D_004CBBF0);
    xglFontDebugPrintf(0x64, 0xC, D_004CBC00, listpos, list + 4 + listpos * 0x10);
    xglFontDebugPrintf(0x5C, 0x14, D_004CBC10, mot, getNumFCV(pAct_004DC638->fcvHeader) - 1);
    if (pause != 0) {
        xglFontDebugPrintf(0x50, 0, D_004CBC20);
    }
}

typedef struct HairTestPair {
    u64 w[2];
} HairTestPair;
typedef struct HairTestPad {
    unsigned char unmodeled_00[0x28];
    union {
        u64 all;
        struct {
            u16 held;
            u16 trigger;
            u16 repeat;
        } h;
    } buttons;
    unsigned char unmodeled_30[0x36];
    signed char stickX;
    signed char stickY;
} HairTestPad;
extern HairTestPad PadData;
extern void xglRenderClearFrame(void);
extern void xglSleep(void);
extern void ppNextStart(HairTestParticle *particle);
extern void ppNextEnd(HairTestParticle *particle);
extern double cos();
extern double sin();
extern float atan2f(float, float);
extern float sqrtf(float);
extern void ACT_setMotion(HairTestAct *actor, int motion);
extern void ACT_updateMotion(HairTestAct *actor);
extern void ACT_modelDraw(void *actor);

void HairTest(void)
{
    float scale;
    float vx, vz;
    float nx, nz, dy;
    int n;
    unsigned short held;
    int next;
    float ang;
    HairTestAct *act;
    StudioCamera *cam;
    double px, pz, dvx, dvz, t1, t2;

    xglRenderClearFrame();
    xglSleep();
    InitTest();
    if ((PadData.buttons.all & 0x08000100) != 0x08000100) {
        scale = 0.0001f;
        do {
            ppNextStart(&cpos_00585200);
            held = PadData.buttons.h.held;
            if (held & 8) {
                cpos_00585200.position.y += 0.01f;
            }
            if (held & 2) {
                cpos_00585200.position.y -= 0.01f;
            }
            if (held & 1) {
                lookY -= 0.02f;
            }
            if (held & 4) {
                lookY += 0.02f;
            }
            vx = PadData.stickX;
            vz = PadData.stickY;
            vx *= scale;
            vz *= scale;
            px = cpos_00585200.position.x;
            dvx = vx;
            t1 = dvx * cos(-cry_004DC654);
            dvz = vz;
            t2 = dvz * sin(-cry_004DC654);
            cpos_00585200.position.x = px + (t1 - t2);
            pz = cpos_00585200.position.z;
            t1 = dvz * cos(-cry_004DC654);
            t2 = dvx * sin(-cry_004DC654);
            cpos_00585200.position.z = pz + (t1 + t2);
            ppNextEnd(&cpos_00585200);
            nx = -cpos_00585200.position.x;
            nz = -cpos_00585200.position.z;
            dy = lookY - cpos_00585200.position.y;
            cry_004DC654 = -atan2f(nx, cpos_00585200.position.z);
            crx_004DC650 = atan2f(dy, __builtin_sqrtf(nx * nx + nz * nz));


            cam = pCamera_004DC634;
            *(HairTestVectorBlock *)&cam->position = *(HairTestVectorBlock *)&cpos_00585200.position;
            cam->rotation.x = crx_004DC650;
            cam->rotation.y = cry_004DC654;
            cam->rotation.z = 0.0f;
            if (pause != 0) {
                if (!(PadData.buttons.h.trigger & 0x800)) {
                    goto draw;
                }
                pause = 0;
            } else if (PadData.buttons.h.trigger & 0x800) {
                pause = 1;
                goto draw;
            }
            if (PadData.buttons.h.repeat & 0x4000) {
                listpos++;
                if (((struct { void *id; int pad[3]; } *)list)[listpos].id == 0) {
                    listpos = 0;
                }
            } else if (PadData.buttons.h.repeat & 0x1000) {
                listpos--;
                if (listpos < 0) {
                    listpos = listnum - 1;
                }
            }
            if (!(PadData.buttons.h.held & 0x80) && listnow != listpos) {
                GameResourceInit(0x07000000, 0x06000000);
                ACT_init();
                pAct_004DC638 = (HairTestAct *)ACT_create(0, ((HairResourceEntry *)list)[listpos].resourceId);
                ACT_loadResource((struct HairTestAct *)pAct_004DC638, ((HairResourceEntry *)list)[listpos].resourceId);
                ACT_loadMotion((struct HairTestAct *)pAct_004DC638, ((HairResourceEntry *)list)[listpos].resourceId, 1);
                listnow = listpos;
            }
            if (PadData.buttons.h.repeat & 0x2000) {
                mot++;
            }
            if (PadData.buttons.h.repeat & 0x8000) {
                mot--;
            }
            n = getNumFCV(pAct_004DC638->fcvHeader);
            while (mot >= n) {
                mot -= n;
            }
            while (mot < 0) {
                mot += n;
            }
            act = pAct_004DC638;
            ((struct { unsigned int a:3; unsigned int b:1; } *)((char *)act + 0x6F0))->b = 1;
            ACT_setMotion(act, mot);
            ACT_updateMotion(pAct_004DC638);
draw:
            ACT_modelDraw(pAct_004DC638);
            PrintDisp();
            xglSleep();
        } while ((PadData.buttons.all & 0x08000100) != 0x08000100);
    }
}

static u16 JNT_hairID(JntHairJoint *joint)
{
    return joint->hairDescriptor->hairID;
}

INCLUDE_ASM("asm/main/nonmatchings/hair_test", __JNT_computeMatrix);

static int getNumFCV(FpkFcvHeader *header)
{
    int count = 1;
    int result = 1;

    if (header != 0) {
        if ((header->magic & FPK_MAGIC_MASK) == FPK_MAGIC) {
            count = header->fcvCount;
        }
        result = count;
    }
    return result;
}
