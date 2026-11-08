#include "common.h"
#include "shared.h"

typedef char *va_list;
#define va_start(ap, last) ((ap) = (va_list)__builtin_next_arg(last) - (8 - __builtin_args_info(2)) * 8)
#define va_end(ap) ((void)0)

extern int vprintf(const char *format, va_list args);
static char dstr[0x100];

void MOutputDebugString(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    sprintf(dstr, "\x1B[1;36mMDMSG:\x1B[m %s\n", format);
    vprintf(dstr, args);
    va_end(args);
}

void MOutputDebugStringWarn(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    sprintf(dstr, "\x1B[1;35mMDMSG:\x1B[m %s\n", format);
    vprintf(dstr, args);
    va_end(args);
}
