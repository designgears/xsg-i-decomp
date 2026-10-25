#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "get.h"


float Get_Decimal_Surplus_for_Radius(float angle)
{
    if (angle <= 0.0f) {
        float negative_pi = -3.141592741f;
        if (angle < negative_pi) {
            do {
                angle += 6.283185482f;
            } while (angle < negative_pi);
        }
    } else {
        while (3.141592741f <= angle) {
            angle -= 6.283185482f;
        }
    }
    return angle;
}

extern int Check_Undu(const Point4 *position, const Point4 *target, int mapIndex,
                       Point4 *output, long long arg4, long long arg5, int attrMask);
extern int Check_CrossingOver(const Vector4 *first_start, const Vector4 *first_end,
                              const Vector4 *second_start, const Vector4 *second_end);
extern int CrossPointUwamono(const Point4 *, const Point4 *, float, Point4 *);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern char D_004DB9A8[];
extern char D_004DB9B0[];

int Check_MoveSquare(Point4 *arg0, Point4 *arg1, Point4 *out, int mapIndex, float size)
{
    Point4 hitA;
    Point4 crossA;
    Point4 hitB;
    Point4 crossB;
    Point4 hitC;
    Point4 crossC;
    Point4 hitD;
    Point4 crossD;
    Point4 pts[2];
    int col[4];
    int ret;

    col[0] = 0;
    col[1] = 0;
    col[2] = 255;
    col[3] = 128;
    ret = 0;

    pts[0].x = arg0->x;
    pts[0].y = arg1->y;
    pts[0].z = arg1->z;
    pts[1].x = arg1->x;
    pts[1].y = arg1->y;
    pts[1].z = arg0->z;
    pts[0].x += (arg0->x - arg1->x < 0.0f) ? -0.1f : 0.1f;
    pts[1].z += (arg0->z - arg1->z < 0.0f) ? -0.1f : 0.1f;
    Check_Undu(arg1, &pts[0], mapIndex, &hitA, 1, 0, 0);
    Check_Undu(arg1, &pts[1], mapIndex, &hitB, 1, 0, 0);
    CrossPointUwamono(arg1, &hitA, size, &crossA);
    CrossPointUwamono(arg1, &hitB, size, &crossB);
    crossA.w = 0.1f;
    crossB.w = 0.1f;
    hitA = crossA;
    hitB = crossB;

    pts[0].x = arg1->x;
    pts[0].y = arg0->y;
    pts[0].z = arg0->z;
    pts[1].x = arg0->x;
    pts[1].y = arg0->y;
    pts[1].z = arg1->z;
    pts[0].x += (arg1->x - arg0->x < 0.0f) ? -0.1f : 0.1f;
    pts[1].z += (arg1->z - arg0->z < 0.0f) ? -0.1f : 0.1f;
    Check_Undu(arg0, &pts[0], mapIndex, &hitC, 1, 0, 0);
    Check_Undu(arg0, &pts[1], mapIndex, &hitD, 1, 0, 0);
    CrossPointUwamono(arg0, &hitC, size, &crossC);
    CrossPointUwamono(arg0, &hitD, size, &crossD);
    crossC.w = 0.1f;
    crossD.w = 0.1f;
    hitC = crossC;
    hitD = crossD;

    xglFontDebugPrintf(8, 100, D_004DB9A8);
    if (Check_CrossingOver((Vector4 *)arg1, (Vector4 *)&hitA, (Vector4 *)arg0, (Vector4 *)&hitD) == 1) {
        *out = hitA;
        ret = 1;
        out->z = arg1->z;
        xglFontDebugPrintf(8, 100, D_004DB9B0);
    }
    if (Check_CrossingOver((Vector4 *)arg1, (Vector4 *)&hitB, (Vector4 *)arg0, (Vector4 *)&hitC) == 1) {
        *out = hitB;
        ret = 1;
        out->x = arg1->x;
        xglFontDebugPrintf(8, 100, D_004DB9B0);
    }
    return ret;
}

void Get_Point_By_AngleLength(const Vector4 *source, Vector4 *destination,
                              float angle, float length)
{
    destination->x = source->x + sinf(angle) * length;
    destination->y = source->y;
    destination->z = source->z + cosf(angle) * length;
    destination->w = source->w;
}

void Get_MiddlePoint(const float *start, const float *following,
                     short point_index, short point_count, float *result)
{
    if (point_index == point_count) {
        result[0] = following[0];
        result[1] = following[1];
        result[2] = following[2];
        result[3] = 1.0f;
    } else {
        result[0] = start[0] + (following[0] - start[0]) * (float)point_index
                    / (float)point_count;
        result[1] = start[1] + (following[1] - start[1]) * (float)point_index
                    / (float)point_count;
        result[2] = start[2] + (following[2] - start[2]) * (float)point_index
                    / (float)point_count;
        result[3] = 1.0f;
    }
}

void Get_MiddlePoint_Parabora(const ParabolaVec *first, const ParabolaVec *second,
                              int current, int total, float height,
                              ParabolaVec *destination)
{
    int current_value = (short)current;
    int total_value = (short)total;
    int midpoint = (int)((short)total +
                         ((unsigned int)(total << 16) >> 31)) >> 1;
    int delta = midpoint - current_value;
    float adjusted_height = height * (float)(delta * delta)
                           / (float)(midpoint * midpoint);
    height -= adjusted_height;

    if (current_value == total_value) {
        destination->x = second->x;
        destination->y = second->y;
        destination->z = second->z;
        destination->w = second->w;
    } else {
        destination->x = first->x + (second->x - first->x)
                       * (float)current_value / (float)total_value;
        destination->y = first->y + (second->y - first->y)
                       * (float)current_value / (float)total_value + height;
        destination->z = first->z + (second->z - first->z)
                       * (float)current_value / (float)total_value;
        destination->w = first->w + (second->w - first->w)
                       * (float)current_value / (float)total_value;
    }
}

typedef struct { float x, y, z, w; } __attribute__((aligned(8))) StraightVec;

extern int HitCheckMapUnitPosSize(StraightVec *pos, float size);

extern long EneTerritoryID[];

void Check_Wall(const Point4 *center, char *hits, short count, int mapIndex,
                int unused, int territory, float radius, float size)
{
    struct {
        StraightVec pos;
        int map;
    } w;
    short j;
    short end;
    short start;
    float angle;

    start = 0;
    end = count;
    w.map = mapIndex;
    if (count <= 0) {
        start = xglSRand() % __builtin_abs(count);
        end = start + 1;
        for (j = 0; j < __builtin_abs(count); j++) {
            hits[j] = 0;
        }
    }
    w.pos.w = 0.0f;
    j = start;
    if (j < end) {
        float step = 6.283185482f;
        float zero = w.pos.w;
        for (; j < end; j++) {
            angle = (float)j * step / (float)count;
            w.pos.x = center->x + sinf(angle) * radius;
            w.pos.y = center->y;
            w.pos.z = center->z + cosf(angle) * radius;
            hits[j] = 0;
            if (((float (*)(const Point4 *, int, int, UnduParam *))Get_HeightAttr)(
                    (const Point4 *)&w.pos, w.map, 0, &UnduTest) == zero) {
                hits[j] = 1;
            } else if (HitCheckMapUnitPosSize(&w.pos, size) != -1) {
                hits[j] = 1;
            } else if (territory != 0 && UnduTest.attribute != ((long long *)EneTerritoryID)[territory]) {
                hits[j] = 1;
            }
        }
    }
}

extern unsigned short xglSRand(void);
extern int Check_Straight(Point4 *start, Point4 *end, short steps, StraightVec *result,
                          float size, int mapIndex, int actor, short territory);

void Check_Octpus(Point4 *center, Point4 *out, short n, int mapIndex, int actor,
                  int territory, float radius, float size)
{
    Point4 end;
    Point4 res[16];
    float dist[16];
    char used[16];
    short order[16];
    short i;
    short j;
    short idx;
    float best;
    float ang;

    for (i = 0; i < n; i++) {
        ang = (float)i * 6.283185482f / (float)n;
        end.x = center->x + sinf(ang) * radius;
        end.y = center->y;
        end.z = center->z + cosf(ang) * radius;
        end.w = 0.0f;
        Check_Straight(center, &end, 16, (StraightVec *)&res[i], size, mapIndex, actor, territory);
        dist[i] = Get_Distance(center, &res[i]);
        used[i] = 0;
    }
    for (i = 0; i < n; i++) {
        idx = -1;
        best = -1.0f;
        for (j = 0; j < n; j++) {
            if (best < dist[j]) {
                if (used[j] == 0) {
                    best = dist[j];
                    idx = j;
                }
            }
        }
        order[i] = idx;
        used[idx] = 1;
    }
    for (i = 0; i < 4; i++) {
        short a = xglSRand() % n;
        short b = xglSRand() % n;
        short tmp = order[a];
        order[a] = order[b];
        order[b] = tmp;
    }
    *out = res[order[0]];
}

extern float D_004D81A0;
extern float D_004D81A4;

float Get_WallSpace(int unused, char *map, short n, float angle)
{
  short new_var;
  short half = n / 2;
  short i = 0;
  short result = 0;
  short pos = (angle / (D_004D81A0 / n)) + half;
  int lo;
  int hi;
  if (half > 0)
  {
    new_var = half;
    lo = pos + new_var;
    hi = (pos + n) + new_var;
    do
    {
      if (map[lo % n] == 0)
      {
        result = lo % n;
        break;
      }
      lo++;
      half = 0;
      if (map[hi % n] == half)
      {
        result = hi % n;
        break;
      }
      hi--;
      i++;
    }
    while (i < new_var);
  }
  return result * (D_004D81A4 / n);
}

void Get_One_Step(float length, const Point4 *source,
                  const Point4 *destination, Point4 *output)
{
    float x_step = destination->x - source->x;
    float z_step = destination->z - source->z;
    float inverse_distance = 1.0f / Get_Distance(source, destination);

    output->x = x_step * inverse_distance * length;
    output->y = 0.0f;
    output->z = z_step * inverse_distance * length;
    output->w = 1.0f;
}

extern int Check_Actor(StraightVec *pos, short actor);

int Check_Straight(Point4 *start, Point4 *end, short steps, StraightVec *result,
                   float size, int mapIndex, int actor, short territory)
{
    StraightVec pos;
    StraightVec last;
    StraightVec hit;
    short i;
    float total;
    float t;

    *(Point4 *)&last = *start;
    if (Check_Undu(start, end, mapIndex, (Point4 *)&hit, 1, 0, 0x10) == 0) {
        *result = hit;
        return 0;
    }
    i = 0;
    if (steps >= 0) {
        total = steps;
        do {
            t = i;
            pos.x = start->x + (end->x - start->x) * t / total;
            pos.y = start->y + (end->y - start->y) * t / total;
            pos.z = start->z + (end->z - start->z) * t / total;
            if ((territory != 0
                 && Get_Attr((Point4 *)&pos, mapIndex, 0x10) != EneTerritoryID[territory])
                || HitCheckMapUnitPosSize(&pos, size) != -1
                || Check_Actor(&pos, actor) == 1) {
                *result = last;
                return (short)(i - 1);
            }
            i++;
            last = pos;
        } while (i <= steps);
    }
    *(Point4 *)result = *end;
    return steps;
}

extern UnduParam UnduTemp;
extern int HitCheckMapUnitPos(Point4 *position);

short Check_Straight_ID(Point4 *start, Point4 *end, short steps, Point4 *result,
                        int mapIndex)
{
    Point4 pos;
    Point4 last;
    short i = 0;
    float total;
    float t;

    UnduTemp.header = UnduDataGetHeader(mapIndex, 0x8000);
    last.x = start->x;
    last.y = start->y;
    last.z = start->z;
    if (steps >= 0) {
        total = steps;
        do {
            t = i;
            pos.x = start->x + (end->x - start->x) * t / total;
            pos.y = start->y + (end->y - start->y) * t / total;
            pos.z = start->z + (end->z - start->z) * t / total;
            if (((float (*)(Point4 *, void *, UnduParam *))UnduCheck)(&pos, 0, &UnduTemp) == -1000.0f
                || HitCheckMapUnitPos(&pos) != -1) {
                result->x = last.x;
                result->y = last.y;
                result->z = last.z;
                return i - 1;
            }
            i++;
            last.x = pos.x;
            last.y = pos.y;
            last.z = pos.z;
        } while (i <= steps);
    }
    result->x = end->x;
    result->y = end->y;
    result->z = end->z;
    return steps;
}

float Get_Distance(const Point4 *first, const Point4 *second)
{
    float first_x = first->x;
    float first_z = first->z;

    return __builtin_sqrtf((second->x - first_x) * (second->x - first_x)
                           + (second->z - first_z) * (second->z - first_z));
}

float Get_Distance3D(const Point4 *first, const Point4 *second)
{
    float first_x = first->x;
    float first_y = first->y;
    float first_z = first->z;

    return __builtin_sqrtf((second->x - first_x) * (second->x - first_x)
                           + (second->y - first_y) * (second->y - first_y)
                           + (second->z - first_z) * (second->z - first_z));
}

float Get_Angle(const Point4 *first, const Point4 *second)
{
    return atan2f(second->x - first->x, second->z - first->z);
}

int Check_Angle(float angle, float rangeStart, float rangeEnd)
{
    rangeStart = Get_Decimal_Surplus_for_Radius(rangeStart);
    rangeEnd = Get_Decimal_Surplus_for_Radius(rangeEnd);
    angle = Get_Decimal_Surplus_for_Radius(angle);

    if (rangeStart <= rangeEnd) {
        if (rangeStart <= angle && angle <= rangeEnd) {
            return 1;
        }
        return 0;
    } else {
        if (rangeStart <= angle || angle <= rangeEnd) {
            return 1;
        }
        return 0;
    }
}

/* MARK: provisional extern block. The four words are _gp-relative .sdata
   witnesses at 0x004d81a8..0x004d81b4, not proven original declarations.
   Names below record roles observed in this function only. */
float Get_Angle_Relative(const Point4 *first, const Point4 *second,
                         float reference)
{
    float relative = Get_Angle(first, second) - reference;

    if (relative < -3.141592741f)
        relative += 6.283185482f;
    if (3.141592741f < relative)
        relative -= 6.283185482f;
    return relative;
}

extern int CheckPointLine(const Vector4 *first, const Vector4 *second,
                          const Vector4 *point);

int Check_CrossingOver(const Vector4 *first_start, const Vector4 *first_end,
                       const Vector4 *second_start, const Vector4 *second_end)
{
    int first_start_side = CheckPointLine(first_start, first_end, second_start);
    int first_end_side = CheckPointLine(first_start, first_end, second_end);

    if (first_start_side != first_end_side) {
        int second_start_side = CheckPointLine(second_start, second_end, first_start);
        int second_end_side = CheckPointLine(second_start, second_end, first_end);

        if (second_start_side != second_end_side)
            return 1;
    }
    return 0;
}

extern float D_004D81B8;
extern float D_004D81BC;
extern float D_004D81C0;
extern float D_004D81C4;
extern int Check_Angle(float angle, float rangeStart, float rangeEnd);
extern int CrossPointUwamono(const Point4 *, const Point4 *, float, Point4 *);

int Check_InsideFan(const Point4 *origin, const Point4 *target, int mapIndex,
                    float facing, float radius, float fanWidthDegrees, float height)
{
    Point4 hit;
    float ty;

    if (radius < Get_Distance3D(origin, target)) {
        return 0;
    }
    {
        float angle = Get_Angle(origin, target);
        float halfWidth = (fanWidthDegrees / 180.0f) * D_004D81B8 * 0.5f;

        if (Check_Angle(angle, facing - halfWidth, facing + halfWidth) == 0) {
            return 0;
        }
    }
    if (mapIndex == -1) {
        return 1;
    }
    if (Check_Undu(origin, target, mapIndex, &hit, 1, 0, 0x400) == 0) {
        return 0;
    }
    if (D_004D81BC <= Get_Distance(target, &hit)) {
        return 0;
    }
    ty = target->y;
    if (D_004D81C0 <= __builtin_fabsf(ty - hit.y)) {
        return 0;
    }
    if (D_004D81C4 < __builtin_fabsf(origin->y - ty)) {
        return 0;
    }
    return CrossPointUwamono(origin, target, height, &hit) != 1;
}

/* Check_Angle: defined below in this file, still assembler. */
extern int Check_Angle(float angle, float rangeStart, float rangeEnd);

int Check_InsideFan_Wooo(const Point4 *origin, const Point4 *target,
                         float facing, float radius, float fanWidthDegrees)
{
    int inside = 0;

    if (!(radius < Get_Distance3D(origin, target))) {
        float angle = Get_Angle(origin, target);
        float halfWidth = (fanWidthDegrees / 180.0f) * 3.141592741f * 0.5f;

        inside = Check_Angle(angle, facing - halfWidth, facing + halfWidth) != 0;
    }
    return inside;
}

/* MARK: provisional extern block. All eight words are _gp-relative .sdata
   witnesses at 0x004d81cc..0x004d81e8. They are not proven original
   declarations; names below record roles observed in this function only. */
        /* Role name for TwoPiD:
   subtracted to wrap the cursor down at/above the high clamp. */
float Get_Cursol_by_Reduce_Speed_Angle_Loop(float current, float target,
                                            float speed)
{
    float cursor;

    if (current == target)
        return current;
    /* Difference first, then the same variable carries the adjusted
       cursor position for the remainder of the loop. */
    cursor = target - current;
    /* MARK: builtin form emits abs.s under -fno-builtin; a plain fabsf
       call would tail-call instead and break the match. */
    if (__builtin_fabsf(cursor) < 3.141592741f)
        cursor = current + cursor / speed;
    else if (cursor < -3.141592741f)
        cursor = current + (cursor + 6.283185482f) / speed;
    else
        cursor = current - (6.283185482f - cursor) / speed;
    if (cursor <= -3.141592741f)
        cursor += 6.283185482f;
    if (3.141592741f <= cursor)
        cursor -= 6.283185482f;
    return cursor;
}

extern volatile float D_004D81EC;

void Get_EnemyIDPos(Point4 *position, Point4 *result)
{
    Point4 probe;
    Point4 hit;
    Point4 cross;
    int color[4];
    short i;
    float best = 1000.0f;
    float dist;
    float angle;

    *result = *position;
    UnduParamInit(&UnduTemp);
    UnduTemp.queryFlags = 0;
    UnduTemp.attrMask = 3;
    color[0] = 0xFF;
    color[1] = 0x7F;
    color[2] = 0;
    color[3] = 0x80;
    UnduTemp.header = UnduDataGetHeader(0x304, 0x8000);
    if (((float (*)(Point4 *, void *, UnduParam *))UnduCheck)(position, 0, &UnduTemp) != -1000.0f) {
        result->x = position->x;
        result->y = position->y;
        result->z = position->z;
    } else {
        for (i = 0; i < 8; i++) {
            angle = (float)i * D_004D81EC * 0.125f;
            probe.x = position->x + 2.0f * sinf(angle);
            probe.y = position->y;
            probe.z = position->z + 2.0f * cosf(angle);
            probe.w = 1.0f;
            Check_Undu(&probe, position, 0x304, &hit, 1, 0, 0);
            CrossPointUwamono(&probe, position, 1.0f, &cross);
            dist = Get_Distance(position, &hit);
            if (dist != 0.0f && dist < best) {
                best = dist;
                result->x = hit.x;
                result->y = hit.y;
                result->z = hit.z;
            }
        }
    }
    result->w = 1.0f;
}

float Get_Multi_Max_Under(float value, float step, float maximum)
{
    if (value < maximum) {
        maximum -= step;
        while (value <= maximum)
            value += step;
    } else {
        maximum -= step;
        while (!(value <= maximum))
            value -= step;
    }
    return value;
}

void Get_HeightAttr(const Point4 *position, int mapIndex, int attrMask,
                    UnduParam *param)
{
    UnduParamInit(param);
    param->attrMask = (short)(attrMask | 0x800);
    param->queryFlags = 0;
    if (mapIndex == 0) {
        param->header = ((PlayerActorHeaderView *)GameLoopState[1])->data_header;
    } else {
        param->header = UnduDataGetHeader(mapIndex, 0x8000);
    }
    UnduCheck(position, 0, param);
}

void Get_Height(const Point4 *position, int mapIndex, int attrMask)
{
    UnduParamInit(&UnduTest);
    UnduTest.queryFlags = 0;
    UnduTest.attrMask = (short)(attrMask | 0x800);
    if (mapIndex == 0) {
        UnduTest.header = ((PlayerActorHeaderView *)GameLoopState[1])->data_header;
    } else {
        UnduTest.header = UnduDataGetHeader(mapIndex, 0x8000);
    }
    UnduCheck(position, 0, &UnduTest);
}

int Get_Attr(const Point4 *position, int mapIndex, int attrMask)
{
    UnduParamInit(&UnduTest);
    UnduTest.queryFlags = 0;
    UnduTest.attrMask = (short)(attrMask | 0x800);
    if (mapIndex == 0) {
        UnduTest.header = ((PlayerActorHeaderView *)GameLoopState[1])->data_header;
    } else {
        UnduTest.header = UnduDataGetHeader(mapIndex, 0x8000);
    }
    UnduCheck(position, 0, &UnduTest);
    return (int)UnduTest.attribute;
}

int Get_Attr_NU(const Point4 *position, int mapIndex, int attrMask)
{
    UnduParamInit(&UnduTest);
    UnduTest.queryFlags = 0;
    UnduTest.attrMask = (short)attrMask;
    if (mapIndex == 0) {
        UnduTest.header = ((PlayerActorHeaderView *)GameLoopState[1])->data_header;
    } else {
        UnduTest.header = UnduDataGetHeader(mapIndex, 0x8000);
    }
    UnduCheck(position, 0, &UnduTest);
    return (int)UnduTest.attribute;
}

extern float D_004D81F0;
typedef struct UnduParamExt {
    UnduParam base;
    unsigned char unmodeled_28[0x30 - 0x28];
    long long arg30;
    long long arg38;
} UnduParamExt;

int Check_Undu(const Point4 *position, const Point4 *target, int mapIndex,
               Point4 *output, long long arg4, long long arg5, int attrMask)
{
    Point4 dir;

    UnduParamInit(&UnduTest);
    UnduTest.queryFlags = 0;
    UnduTest.attrMask = (short)(attrMask | 0x813);
    if (mapIndex == 0) {
        UnduTest.header = ((PlayerActorHeaderView *)GameLoopState[1])->data_header;
    } else {
        UnduTest.header = UnduDataGetHeader(mapIndex, 0x8000);
    }
    dir.x = target->x - position->x;
    dir.y = target->y - position->y;
    dir.z = target->z - position->z;
    *output = *position;
    dir.w = 1.0f;
    ((UnduParamExt *)&UnduTest)->arg38 = arg4;
    ((UnduParamExt *)&UnduTest)->arg30 = arg5;
    UnduCheck(output, &dir, &UnduTest);
    return Get_Distance3D(target, output) <= D_004D81F0;
}

int Get_Rnd(int min, int max)
{
    return min + xglSRand() % (max - min + 1);
}

void BSpline_Init(Vector4 *destination, const Vector3 *source)
{
    short i;

    for (i = 0; i < 4; i++) {
        destination[i].x = source->x;
        destination[i].y = source->y;
        destination[i].z = source->z;
        destination[i].w = 1.0f;
    }
}

void BSpline_Add(short frame, short count, short period,
                 Vector4 *destination, const Vector3 *source, short offset)
{
    unsigned short index;

    index = (frame / period + offset) % count;
    destination[index].x = source->x;
    destination[index].y = source->y;
    destination[index].z = source->z;
    destination[index].w = 1.0f;
}

extern unsigned short *DataSpline;
typedef struct { float x, y, z, w; } BSplVec;

void GetBSplineLoop(short t, short num, short div, int pts, float *out) {
    unsigned short n;
    unsigned short f;
    float w0, w1, w2, w3;
    BSplVec *p0, *p1, *p2, *p3;

    n = (t / div) % num;
    f = (t % div) * (0x800 / div);
    w0 = DataSpline[(f + 0x10D0) >> 1];
    w1 = DataSpline[(f + 0x10D2) >> 1];
    w2 = DataSpline[(f + 0x10D4) >> 1];
    w3 = DataSpline[(f + 0x10D6) >> 1];
    p0 = (BSplVec *)(n * 16 + pts);
    p1 = (BSplVec *)((unsigned short)((n + 1) % num) * 16 + pts);
    p2 = (BSplVec *)((unsigned short)((n + 2) % num) * 16 + pts);
    p3 = (BSplVec *)((unsigned short)((n + 3) % num) * 16 + pts);
    out[0] = (w0 * p0->x + w1 * p1->x + w2 * p2->x + w3 * p3->x) / 24576.0f;
    out[1] = (w0 * p0->y + w1 * p1->y + w2 * p2->y + w3 * p3->y) / 24576.0f;
    out[2] = (w0 * p0->z + w1 * p1->z + w2 * p2->z + w3 * p3->z) / 24576.0f;
}

typedef struct SplineMotionParameters {
    unsigned char unmodeled_00[8];
    float step_length;
} SplineMotionParameters;
typedef struct SplineMotionActor {
    unsigned char unmodeled_00[0x80];
    unsigned char number;
    unsigned char unmodeled_81[0xc0 - 0x81];
    SplineMotionParameters parameters;
} SplineMotionActor;
typedef struct EnemySplineState {
    unsigned char unmodeled_00[0x40];
    short frame;
    unsigned char unmodeled_42[4];
    short segment_steps;
    unsigned char unmodeled_48[0x38b0 - 0x48];
} EnemySplineState;
extern EnemySplineState enepc[];

void GetBSplineLoopDummy(short frame, short count, short period,
                         const Point4 *points, Point4 *destination,
                         const SplineMotionActor *actor)
{
    int actor_number = actor->number;
    int period_index = frame / period;
    unsigned short phase = frame % period;
    EnemySplineState *state = &enepc[actor_number];
    const SplineMotionParameters *parameters = &actor->parameters;
    int index = (unsigned short)(period_index % count);
    int next = index + 1;

    state->segment_steps = (int)(Get_Distance(&points[index], &points[next % count])
                                 / parameters->step_length);
    if (phase >= state->segment_steps - 1) {
        state->frame = state->frame - state->frame % 128 + 128;
    } else {
        state->frame++;
    }
    state->frame %= period * count;
    {
    short segment_steps = state->segment_steps;
    if (segment_steps == 0) {
        destination->x = points[index].x;
        destination->y = points[index].y;
        destination->z = points[index].z;
    } else {
        int following = next % count;
        float phase_value = phase;
        float step_count = segment_steps;
        {
            float coordinate = points[index].x;
            coordinate += (points[following].x - coordinate) * phase_value / step_count;
            destination->x = coordinate;
        }
        {
            float coordinate = points[index].y;
            coordinate += (points[following].y - coordinate) * phase_value / step_count;
            destination->y = coordinate;
            coordinate = points[index].z;
            coordinate += (points[following].z - coordinate) * phase_value / step_count;
            destination->z = coordinate;
        }
    }
    }
    destination->w = 1.0f;
}
