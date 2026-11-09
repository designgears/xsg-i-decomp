/*
 * The VM class record (struct JavaClass), shared by main/tu226
 * (src/main/init_vm.c, which defines the static _dummyClass record) and
 * main/tu227 (src/main/find_native_method.c, the class loader).
 */

#ifndef SRC_MAIN_JAVA_CLASS_H
#define SRC_MAIN_JAVA_CLASS_H

#include "shared.h"

/* Header word every heap object newObject creates begins with. */
typedef struct ObjectHeader {
    void *classPointer;
} ObjectHeader;

typedef struct JavaMethod JavaMethod;

typedef struct JavaFieldEntry FieldEntry;

typedef struct DispatchTable DispatchTable;

typedef union ConstantPoolEntry ConstantPoolEntry;

/*
 * The full layout of the VM class record (include/shared.h's SceneClass is
 * the shared view of the same 0x40 bytes).
 */
struct JavaClass {
    ObjectHeader header;
    struct ClassNameKey *name;
    u16 accessFlags;
    u16 flags;
    void *classLoader;
    JavaClass *superClass;
    ConstantPoolEntry *constants;
    DispatchTable *classPointer;
    FieldEntry *fields;
    JavaMethod *methods;
    JavaClass **interfaces;
    u16 constantCount;
    u16 dispatchSize;
    u16 methodCount;
    u16 interfaceCount;
    u16 fieldCount;
    u16 staticFieldCount;
    u16 declaredInterfaceCount;
    s16 state;
    int instanceSize;
    struct ElementType *elementClass;
};

#endif /* SRC_MAIN_JAVA_CLASS_H */
