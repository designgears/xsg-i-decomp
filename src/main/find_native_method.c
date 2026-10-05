#include "common.h"
#include "find_native_method.h"

extern ClassEntry **classEntryPool;

int strcmp(const char *, const char *);

/* One row of the default_native[] table: a native method's exported name
 * and the address of its native implementation. */
typedef struct NativeMethodEntry {
    const char *name;
    void *function;
} NativeMethodEntry;

extern NativeMethodEntry default_native[];

/*
 * Looks up name in the default_native[] table, which a NULL name terminates,
 * and returns the matching native implementation, or NULL if the table is
 * empty or no entry's name matches.
 */
void *findNativeMethod(const char *name)
{
    NativeMethodEntry *entry = default_native;

    if (entry->name != 0) {
        do {
            if (strcmp(entry->name, name) == 0) {
                return entry->function;
            }
            entry++;
        } while (entry->name != 0);
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/find_native_method", resolveNativeMethod);

/*
 * A string constant of a class file's constant pool, as this function reads
 * one: resolveConstants walks the pool's tag bytes, hands every entry tagged
 * 8 to this function and retags it 0x18 once the slot holds the instance
 * built here.
 */
typedef struct StringConstant {
    u8 unmodeled_00[6];
    u16 length;     /* +0x6: the constant's own character count */
    int messageId;  /* +0x8: the message id the constant stands for */
} StringConstant;

/*
 * The value record a java.lang.String instance refers to.  Only the two
 * words written here are modeled: src/main/chr.h documents the same +0x8
 * word as the message id Java_xeno_Chr_talkto and _touchto read back out of
 * a string argument (main 0x003007b8 and 0x00300820).
 */
typedef struct StringValue {
    u8 unmodeled_00[4];
    int length;     /* +0x4: the constant's length, widened to a word */
    int messageId;  /* +0x8: the constant's message id */
} StringValue;

/*
 * The java.lang.String instance itself: the object header newObject seeds,
 * then the value record at +0x4 the same two natives read.
 */
typedef struct StringInstance {
    ObjectHeader header;
    StringValue *value;  /* +0x4 */
} StringInstance;

extern ClassEntry *classString;

/*
 * Turns a string constant into the java.lang.String instance that stands for
 * it and returns it; resolveStaticField and resolveConstants both store the
 * result back into the constant-pool slot they passed.
 */
StringInstance *Const2JavaString(StringConstant *constant)
{
    StringInstance *string;
    StringValue *value;
    int length;
    int messageId;

    value = xmalloc(sizeof(StringValue), 0xE);
    length = constant->length;
    messageId = constant->messageId;
    *(int *)&value->length = length;
    *(int *)&value->messageId = messageId;
    string = newObject(classString);
    string->value = value;
    return string;
}

void *newObject(ClassEntry *clazz)
{
    ObjectHeader *instance = xmalloc(clazz->instanceSize, 0xE);

    instance->classPointer = clazz->classPointer;
    return instance;
}

extern ClassEntry *classClass;

/*
 * Allocates the 0x40-byte class entry findClass, initWrapperClass and
 * lookupArray fill in: a class with no static fields and no instance size of
 * its own yet, carrying the class pointer java.lang.Class itself carries.
 */
ClassEntry *newClass(void)
{
    ClassEntry *clazz;

    clazz = xmalloc(0x40, 0xE);
    clazz->instanceSize = 0;
    clazz->staticFieldCount = 0;
    clazz->classPointer = (void *)*(int *)&classClass->classPointer;
    return clazz;
}

void *newArray(ElementType *elementType, int length)
{
    ArrayHeader *array;
    void *classPointer;

    if (elementType->flags & 0x100) {
        array = xmalloc(elementType->elementSize * length + 0xC, 0xE);
    } else {
        array = xmalloc(length * 4 + 0xC, 0xE);
    }
    classPointer = lookupArray(elementType)->classPointer;
    array->length = length;
    array->classPointer = classPointer;
    array->data = (void *)(array + 1);
    return array;
}

INCLUDE_ASM("asm/main/nonmatchings/find_native_method", processClass);

static int *findSuperMethod(int *key, char *clazz)
{
    int i;
    int *m;

    while ((clazz = *(char **)(clazz + 0x10)) != 0) {
        i = *(u16 *)(clazz + 0x2C);
        m = *(int **)(clazz + 0x20);
        while (--i >= 0) {
            if (m[0] == key[0] && m[1] == key[1]) {
                return m;
            }
        }
    }
    return 0;
}

extern void *NAME_Constructor;
typedef struct DMethod {
    u32 name;
    int sig;
    u8 pad08[8];
    u16 flags;
    s16 index;
} DMethod;
typedef struct DClass {
    u8 pad00[0x10];
    struct DClass *super;
    u8 pad14[4];
    void **dispatch;
    u8 pad1C[4];
    DMethod *methods;
    u8 pad24[6];
    u16 dispatchCount;
    u16 methodCount;
} DClass;

static void buildDispatchMethodTable(DClass *cls)
{
    int n;
    int k;
    int *s;
    u32 name;
    void **table;
    void **t;
    void **src;
    int j;
    DClass *sup;
    DMethod *m;
    DMethod *m2;
    int i;

    if (cls->super != 0) {
        cls->dispatchCount = cls->super->dispatchCount;
    } else {
        cls->dispatchCount = 0;
    }
    i = cls->methodCount;
    m = cls->methods;
    while (--i >= 0) {
        if ((m->flags & 8) || *(int *)m == *(int *)&NAME_Constructor) {
            m->index = -1;
        } else {
            s = findSuperMethod((int *)m, (char *)cls);
            if (s != 0) {
                m->index = ((DMethod *)s)->index;
            } else {
                m->index = cls->dispatchCount++;
            }
        }
    }
    table = xmalloc(cls->dispatchCount * 4 + 8, 0xF);
    t = table + 1;
    *(DClass **)table = cls;

    cls->dispatch = table;
    if (cls->super != 0) {
        sup = cls->super;

        src = sup->dispatch + 1;
        for (j = 0; j < sup->dispatchCount; j++) {
            t[j] = src[j];
        }
    }
    i = cls->methodCount;
    m = cls->methods;
    while (--i >= 0) {
        if (m->index >= 0) {
            t[m->index] = m;
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/find_native_method", resolveStaticField);

static void resolveInstanceField(char *cls)
{
    int i;
    int off;
    int sz;
    char *f;
    int nstatic;

    nstatic = *(unsigned short *)(cls + 0x32);
    i = *(unsigned short *)(cls + 0x30) - nstatic - 1;
    off = *(int *)(cls + 0x38);
    f = *(char **)(cls + 0x1C) + nstatic * 0x14;
    if (off == 0) {
        off = 4;
    }
    for (; i >= 0; i--) {
        sz = ((unsigned int)*(int *)(f + 0xC) + 3) >> 2 << 2;
        off = (off + sz - 1) / sz * sz;
        *(int *)(f + 0x10) = off;
        off += sz;
        f += 0x14;
    }
    *(int *)(cls + 0x38) = off;
}

typedef union RCSlot {
    StringConstant *s;
    unsigned short h;
} RCSlot;
typedef struct RCClass {
    u8 pad[0x14];
    RCSlot *pool;
    u8 pad2[0x10];
    unsigned short count;
} RCClass;

static int resolveConstants(RCClass *cls)
{
    int i = 0;
    RCSlot *pool = cls->pool;
    int n = cls->count;

    if (n != 0) {
        do {
            u8 *tag = *(u8 **)pool + i;
            if (*tag == 8) {
                *tag = 0x18;
                pool[i].s = (StringConstant *)Const2JavaString(pool[i].s);
                n = cls->count;
            }
            i++;
        } while (i < n);
    }
    return 1;
}

void getClassFromSignature(const char *signature);

void *loadArray(void *obj)
{
    void *cls;

    cls = ((void *(*)(const char *))getClassFromSignature)(*(char **)((char *)obj + 8) + 1);
    if (cls != 0) {
        return lookupArray(cls);
    }
    return 0;
}

void *findClass(ClassEntry *entry);
ClassEntry *lookupClassEntry();
int processClass(int classHandle, int state);

int loadClass(int classKey, int skipLoad)
{
    ClassEntry *entry;
    void *resolved;
    int result;

    entry = lookupClassEntry(classKey);
    resolved = entry->resolvedClass;
    if (resolved == 0) {
        if (skipLoad == 0) {
            resolved = findClass(entry);
        }
        if (resolved != 0) {
            entry->resolvedClass = resolved;
        }
    }
    if (resolved == 0) {
        result = 0;
    } else {
        result = (processClass((int)resolved, 0xB) == 0) ? 0 : (int)resolved;
    }
    return result;
}

void loadStaticClass(void **arg0, s8 *arg1) {
    ClassEntry *e;
    void *c;

    e = lookupClassEntry(loadConstString(arg1, -1), 0);
    if (e->resolvedClass == 0) {
        c = findClass(e);
        if (c == 0) {
            *arg0 = 0;
            return;
        }
        e->resolvedClass = c;
        *arg0 = c;
    }
    processClass((int) e->resolvedClass, 0xB);
}

void reloadClassEntry(void *heapBoundary)
{
    ClassEntry **bucket = classEntryPool;
    int bucketsRemaining = 511;

    do {
        ClassEntry *entry = *bucket;

        if (entry != 0) {
            ClassEntry *current = entry;

            if ((unsigned int)current >= (unsigned int)heapBoundary) {
                do {
                    current = current->next;
                } while (current != 0 &&
                         (unsigned int)current >= (unsigned int)heapBoundary);
            }
            *bucket = current;
        }
        bucket++;
        bucketsRemaining--;
    } while (bucketsRemaining >= 0);
}

typedef struct ClassKey {
    u8 pad[4];
    u16 hash;
} ClassKey;
typedef struct ClassEntryView {
    ClassKey *key;
    int arg;
    void *resolvedClass;
    struct ClassEntryView *next;
} ClassEntryView;

ClassEntry *lookupClassEntry(ClassKey *key, int arg)
{
    ClassEntryView **pool = (ClassEntryView **)classEntryPool;
    ClassEntryView *e;
    int idx;

    if (pool == 0) {
        int i;
        ClassEntryView **p;
        pool = xmalloc(0x800, 2);
        classEntryPool = (ClassEntry **)pool;
        i = 0x1FF;
        p = &pool[0x1FF];
        do {
            i--;
            *p = 0;
            p--;
        } while (i >= 0);
    }
    idx = key->hash & 0x1FF;
    e = pool[idx];
    for (; e != 0; e = e->next) {
        if (key == e->key) {
            if (arg == e->arg) {
                break;
            }
        }
    }
    if (e != 0) {
        return (ClassEntry *)e;
    }
    e = xmalloc(0x10, 2);
    e->key = key;
    e->arg = arg;
    e->next = ((ClassEntryView **)classEntryPool)[idx];
    e->resolvedClass = 0;
    ((ClassEntryView **)classEntryPool)[idx] = e;
    return (ClassEntry *)e;
}

extern struct SceneClass *classObject;
extern struct SceneString *loadConstString2(const char *bytes, int length);
extern char D_004DC050[];
extern char D_004DC058[];
extern char D_004DC060[];
typedef struct ArrayClassView {
    u8 pad00[4];
    struct SceneString *name;
    u16 flags;
    u8 pad0A[2];
    int key;
    void *super;
    u8 pad14[0x22];
    u16 state;
    u8 pad38[4];
    ElementType *elementType;
} ArrayClassView;

ClassEntry *lookupArray(ElementType *elementType)
{
    char buf[1024];
    struct SceneString *name;
    ClassEntry *entry;
    ArrayClassView *cls;
    char *s;
    struct SceneClass *obj;

    if (elementType->flags & 0x100) {
        sprintf(buf, D_004DC050, *(s8 *)((char *)elementType + 0x39));
    } else {
        s = *(char **)(*(char **)((char *)elementType + 4) + 8);
        if (*s == '[') {
            sprintf(buf, D_004DC058, s);
        } else {
            sprintf(buf, D_004DC060, s);
        }
    }
    name = loadConstString2(buf, -1);
    entry = lookupClassEntry(name, *(int *)((char *)elementType + 0xC));
    if (entry->resolvedClass == 0) {
        cls = (ArrayClassView *)newClass();
        obj = classObject;
        cls->flags = 0x411;
        entry->resolvedClass = cls;
        cls->name = name;
        cls->key = *(int *)((char *)elementType + 0xC);
        cls->state = 1;
        cls->super = obj;
        buildDispatchMethodTable((DClass *)cls);
        cls->elementType = elementType;
    }
    return entry->resolvedClass;
}

INCLUDE_ASM("asm/main/nonmatchings/find_native_method", classFromSig);

void *classFromSig(const char **cursor);

void getClassFromSignature(const char *signature)
{
    const char *cursor = signature;

    classFromSig(&cursor);
}

typedef struct SFField {
    u8 pad[0xC];
    int size;
    int value;
} SFField;

static void allocStaticField(char *cls)
{
    int n;
    SFField *f;
    unsigned int total;
    char *mem;
    int sz;

    n = *(unsigned short *)(cls + 0x32);
    if (n != 0) {
        f = *(SFField **)(cls + 0x1C);
        total = 0;
        while (--n >= 0) {
            sz = (unsigned int)(f->size + 3) >> 2;
            sz *= 4;
            total += sz;
            f->size = ((int)(total - 1) / sz) * sz;
            f = (SFField *)((char *)f + 0x14);
        }
        mem = xmalloc(total, 10);
        f = *(SFField **)(cls + 0x1C);
        n = *(unsigned short *)(cls + 0x32);
        while (--n >= 0) {
            total = f->size;
            f->size = f->value;
            f->value = (int)(mem + total);
            f = (SFField *)((char *)f + 0x14);
        }
    }
}

int sizeofDescripter(u8 **cursor);

void methodDescripter(u8 *descriptor, s16 *paramSize, s16 *returnSize, s8 *returnType)
{
    u8 *cursor = descriptor;

    *paramSize = (s16)sizeofDescripter(&cursor);
    *returnType = (s8)*cursor;
    *returnSize = (s16)sizeofDescripter(&cursor);
}

int sizeofDescripterType(u8 **cursor);

static int sizeofDescripter(u8 **cursor)
{
    int n = 0;
    int r;
    for (;;) {
        r = sizeofDescripterType(cursor);
        if (r < 0) {
            return n;
        }
        n += r;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/find_native_method", instanceOf);

INCLUDE_ASM("asm/main/nonmatchings/find_native_method", sizeofDescripterType);
