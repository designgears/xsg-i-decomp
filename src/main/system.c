#include "common.h"

#include "shared.h"
struct SystemClassName {
    unsigned char unmodeled_00[8];
    signed char *characters;
};
struct SystemArrayClass {
    unsigned char unmodeled_00[4];
    struct SystemClassName *name;
    unsigned char unmodeled_08[2];
    unsigned short flags;
    unsigned char unmodeled_0c[0x2e];
    unsigned char element_width;
    unsigned char unmodeled_3b;
    struct SystemArrayClass *element_class;
};
struct SystemArrayReference {
    struct SystemArrayClass *klass;
};
struct SystemArray {
    struct SystemArrayReference *reference;
    unsigned char unmodeled_04[4];
    unsigned char *data;
};
struct SystemArrayCopyEndpoint {
    struct SystemArray *array;
    int index;
};
struct SystemArrayCopyArguments {
    struct SystemArrayCopyEndpoint source;
    struct SystemArrayCopyEndpoint destination;
    unsigned int count;
};
extern void *memcpy(void *destination, const void *source, unsigned int size);

void *Java_xeno_vm_System_arraycopy__Ljava_lang_Object_ILjava_lang_Object_II(
    void *thread, void *argument_block)
{
    struct SystemArrayCopyArguments *arguments = argument_block;
    struct SystemArrayClass *element_class;
    struct SystemArray *destination = arguments->destination.array;
    struct SystemArray *source = arguments->source.array;
    int source_index = arguments->source.index;
    int destination_index = arguments->destination.index;
    unsigned int count = arguments->count;
    unsigned int element_width;

    element_class = destination->reference->klass;
    while (element_class->name->characters[0] == '[') {
        element_class = element_class->element_class;
    }

    element_width = 4;
    if (element_class->flags & 0x100) {
        element_width = element_class->element_width;
    }

    {
        int source_offset = source_index * element_width;
        unsigned int byte_count = count * element_width;
        int destination_offset = destination_index * element_width;
        unsigned char *source_bytes = source->data;
        unsigned char *destination_bytes = destination->data;

        source_bytes = &source_bytes[source_offset];
        destination_bytes = &destination_bytes[destination_offset];
        return memcpy(destination_bytes, source_bytes, byte_count);
    }
}

typedef struct JThread JThread;
struct JThread {
    unsigned int pad0;
    struct JThread *previous;
    struct JThread *next;
    unsigned char kind;
    unsigned char wait_kind;
    unsigned short pad0e;
    void *object;
    void (*reset)(struct JThread *thread);
    void *method;
    unsigned int pad1c;
    unsigned int pad20;
    unsigned int flags;
    unsigned int *stack;
    void *frames;
    void *wait_target;
    int wait_parameter;
    unsigned short stack_offset;
    short stack_limit;
    unsigned short resume_frames;
    unsigned short frame_depth;
    short frame_limit;
};

void Java_xeno_vm_System_sleep__I(JThread *thread, union { int i; void *p; } *arguments, unsigned int *result)
{
    unsigned int flags = thread->flags;
    int n;

    if (flags & 4) {
        return;
    }
    thread->flags = flags | 4;
    thread->wait_target = 0;
    thread->wait_kind = 1;
    thread->resume_frames = thread->frame_depth;
    n = arguments->i;
    thread->wait_parameter = n < 0 ? 0 : n;
}

static char *getStrIndex(char *string, int length, int ch)
{
    int i;

    for (i = 0; i < length; i++) {
        if (string[i] == ch) {
            return string + i;
        }
        if (string[i] == 0) {
            return 0;
        }
    }
    return 0;
}

extern char tmpString[];
extern char D_004DC090[];
extern char *strncpy(char *, const char *, unsigned int);
extern int sprintf(char *, const char *, ...);
extern int PARSE_int(char *, int, int);
extern int JNI_getRegister(int);

void Java_xeno_vm_System_println__Ljava_lang_String_(void *thread, void **arguments, unsigned int *result)
{
    char buf[16];
    int len;
    int n;
    char *src;
    char *end;
    char *p;
    char *r;
    union { int i; float f; } v;

    src = (char *)((int *)((int *)*arguments)[1])[2];
    len = ((int *)((int *)*arguments)[1])[1];
    end = src + len;
    while (1) {
        {
            p = getStrIndex(src, len, '/');
            r = p + 2;
            if (p == 0) {
                return;
            }
            if (p[1] != '[') {
                continue;
            }
        }
        n = p - src;
        len -= n;
        strncpy(tmpString, src, n);
        tmpString[n] = 0;
        src = getStrIndex(r, len, ']');
        p = (char *)(src - p);
        n = (int)p - 2;
        if (src == 0) {
            return;
        }
        strncpy(tmpString, r, n);
        if (tmpString[0] == '$') {
        } else if (tmpString[0] == '#') {
            v.i = JNI_getRegister(PARSE_int(tmpString + 1, (int)p - 3, 10));
            sprintf(buf, D_004DC090, v.f);
        }
        src++;
        len -= n;
        if (end <= src) {
            return;
        }
    }
}

extern void *classJava_xeno_Chr;
extern void *classJava_xeno_Stage;
extern void *classJava_xeno_util_Menu;
extern void *classJava_xeno_util_Window;
extern int JNI_isInstanceOf(void *object, void *target_class);
extern JThread *JTHREAD_get(void *object);

static void System_waitFor(JThread *thread, void *wait_target, int wait_kind, int wait_parameter)
{
    void *klass;
    JThread *target;

    if (JNI_isInstanceOf(wait_target, classJava_xeno_Chr) == 1) {
        klass = classJava_xeno_Chr;
    } else if (JNI_isInstanceOf(wait_target, classJava_xeno_Stage) == 1) {
        klass = classJava_xeno_Stage;
    } else if (JNI_isInstanceOf(wait_target, classJava_xeno_util_Menu) == 1) {
        thread->wait_kind = 3;
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 4;
        thread->wait_target = wait_target;
        return;
    } else if (JNI_isInstanceOf(wait_target, classJava_xeno_util_Window) == 1) {
        thread->wait_kind = 3;
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 4;
        thread->wait_target = wait_target;
        return;
    } else {
        klass = 0;
    }
    if (klass == 0) {
        return;
    }
    target = JTHREAD_get(wait_target);
    switch (wait_kind) {
    case 0:
        if (target != thread) {
            thread->wait_kind = 2;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
            thread->wait_target = target;
        }
        break;
    case 5:
        thread->wait_kind = wait_kind;
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 4;
        thread->wait_target = target;
        thread->wait_parameter = wait_parameter;
        break;
    }
}

/*
 * The script VM's per-thread context, recovered as `JThread` in
 * src/main/chr.h (main's chr TU). Every Java native receives
 * (thread, arguments, result) as the sibling natives elsewhere in this
 * TU map show.
 */
typedef struct JThread JThread;

extern void System_waitFor(JThread *thread, void *wait_target, int wait_kind,
                           int wait_parameter);

void Java_xeno_vm_System_waitFor__Ljava_lang_Object_(JThread *thread, void *arguments,
                                                      unsigned int *result)
{
    void **object = arguments;

    System_waitFor(thread, *object, 0, 0);
}

extern void SCRIPT_methodClearSet(void);

void Java_xeno_vm_System_methodSignal__I(JThread *thread, void *arguments,
                                         unsigned int *result)
{
    int *flag = arguments;

    if (*flag != 0) {
        SCRIPT_methodClearSet();
    }
}
