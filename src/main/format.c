#include "common.h"
#include "shared.h"
#include "format.h"

/*
 * The script VM's per-thread context, recovered as `JThread` in
 * src/main/chr.h (main's chr TU). Every Java native receives
 * (thread, arguments, result) as the sibling natives elsewhere in this
 * TU map (and in main/tu238, src/main/runtime.c) show.
 */
typedef struct JThread JThread;

void Java_xeno_util_Format_floatToIntBits__F(JThread *thread, int *arguments, unsigned int *result)
{
    result[0] = arguments[0];
}

void Java_xeno_util_Format_intBitsToFloat__I(JThread *thread, int *arguments, float *result)
{
    result[0] = (float) arguments[0];
}

void Java_xeno_util_Format_toInt__Ljava_lang_String_(JThread *thread, void *arguments,
                                                      unsigned int *result)
{
}

extern int sprintf(char *buf, const char *fmt, ...);
extern int strlen(const char *s);
extern char D_004DC098[];

void Java_xeno_util_Format_toString__C(JThread *thread, union { int i; short s; void *p; } *args, int *result)
{
    FormatByteArray *arr;
    FormatString *str;
    str = newObject(classString);
    str->value = newArray(classByte, 0xFF);
    sprintf((char *)str->value->data, D_004DC098, args[0].s);
    arr = str->value;
    arr->length = strlen((char *)arr->data);
    *result = (int)str;
}

typedef struct { FormatString *string; } FormatTmpStringViewF;
extern double fptodp(float value);
extern char D_004DC0A0[];

void Java_xeno_util_Format_toString__F(JThread *thread, union { int i; float f; void *p; } *args, int *result)
{
    FormatByteArray *arr;
    char *buf;
    char *fmt;

    if (JAVA_tmpString == 0) {
        JAVA_tmpString = newObject(classString);
        JAVA_tmpString->value = newArray(classByte, 0xFF);
    }
    fmt = D_004DC0A0;
    buf = (char *)JAVA_tmpString->value->data;
    sprintf(buf, fmt, fptodp(args[0].f));
    arr = JAVA_tmpString->value;
    arr->length = strlen((char *)arr->data);
    *result = (int)((FormatTmpStringViewF *)&JAVA_tmpString)->string;
}

void Java_xeno_util_Format_toString__I(JThread *thread, int *arguments,
                                       int *result)
{
    unsigned char *end;
    FormatByteArray *source_array;
    FormatByteArray *stored_array;

    if (JAVA_tmpString == 0) {
        JAVA_tmpString = newObject(classString);
        JAVA_tmpString->value = newArray(classByte, 255);
    }

    source_array = JAVA_tmpString->value;
    end = STRING_int(source_array->data, arguments[0]);
    *end = 0;
    stored_array = JAVA_tmpString->value;
    stored_array->length = end - stored_array->data;
    *result = (int)JAVA_tmpString;
}

typedef struct { FormatString *string; } FormatTmpStringView;

void Java_xeno_util_Format_toString__Z(JThread *thread, union { int i; unsigned char b; void *p; } *args, int *result)
{
    FormatByteArray *arr;

    if (JAVA_tmpString == 0) {
        JAVA_tmpString = newObject(classString);
        JAVA_tmpString->value = newArray(classByte, 0xFF);
    }
    sprintf((char *)JAVA_tmpString->value->data, D_004DC098, args[0].b);
    arr = JAVA_tmpString->value;
    arr->length = strlen((char *)arr->data);
    *result = (int)((FormatTmpStringView *)&JAVA_tmpString)->string;
}
