#include "common.h"
#include "shared.h"
#include "stage_1.h"

void *STAGE_create(u16 stage_id)
{
    defaultStage[2] = stage_id;
    return defaultStage;
}

extern SceneString *NAME_Constructor;
extern SceneType *TYPE_Void;
extern SceneClass *classJava_xeno_Chr;
extern SceneClass *classJava_xeno_Stage;
extern unsigned char tcamera[];
extern char D_004DA628[];
extern char D_004DA630[];
extern char D_004DA638[];
extern char D_004C2420[];
extern JavaField *lookupClassField(void *class_object, void *name, int flags);
extern SceneClass *getClassFromSignature(const char *signature, void *class_loader);
extern int instanceOf(SceneClass *scene_class, SceneClass *parent_class);
extern SceneClass *loadClass(SceneString *name, int initialize);
extern SceneObject newObject(SceneClass *scene_class);
extern SceneMethod *findMethod(SceneClass *scene_class, SceneString *name, void *signature_or_type);
extern SceneString *loadConstString(const char *bytes, int length);
extern void JNI_initThread(SceneVm *vm);
extern void JNI_callMethod(SceneVm *vm, SceneMethod *method, SceneObject *arguments, int *output);

void STAGE_instance(SceneVm *vm, SceneObject stage_object)
{
    SceneObject arguments[4];
    SceneObject self;
    SceneClass *stage_class;
    SceneMethod *method;
    SceneField *field;
    SceneString *method_name;
    JavaField *camera_field;
    int remaining;

    self = stage_object;
    method = findMethod(((SceneObjectHeader *)stage_object)->class_ref->scene_class,
                        NAME_Constructor, TYPE_Void);
    if (method != 0) {
        arguments[0] = stage_object;
        JNI_initThread(vm);
        JNI_callMethod(vm, method, arguments, 0);
    }

    field = (SceneField *)lookupClassField(classJava_xeno_Stage, loadConstString(D_004DA628, -1), 0);
    if (field != 0) {
        *(int *)tcamera =
            (int)loadClass(loadConstString(D_004C2420, -1), 0)->instance_class_ref;
        *(unsigned char **)(stage_object + field->instance_offset) = tcamera;
    }

    stage_class = ((SceneObjectHeader *)stage_object)->class_ref->scene_class;
    remaining = stage_class->field_count - stage_class->static_field_count;
    remaining--;
    field = stage_class->fields + stage_class->static_field_count;
    while (remaining >= 0) {
        if (field->flags & 0x8000) {
            SceneClass *resolved = getClassFromSignature(
                field->type_or_descriptor->signature, stage_class->class_loader);
            if (resolved == 0) {
                goto next;
            }
            field->type_or_descriptor = (SceneTypeDescriptor *)resolved;
            field->flags &= 0x7fff;
        }

        if (instanceOf((SceneClass *)field->type_or_descriptor, classJava_xeno_Chr)) {
            SceneMethod *init_method;
            SceneObject field_object;

            init_method = findMethod((SceneClass *)field->type_or_descriptor, loadConstString(D_004DA630, -1),
                                     loadConstString(D_004DA638, -1));
            if (init_method != 0) {
                SceneClass *component_class;
                SceneField *nested;
                int nested_remaining;

                field_object = newObject((SceneClass *)field->type_or_descriptor);
                arguments[0] = field_object;
                JNI_initThread(vm);
                JNI_callMethod(vm, init_method, arguments, 0);
                *(SceneObject *)(self + field->instance_offset) = field_object;
                component_class = (SceneClass *)field->type_or_descriptor;
                nested_remaining = component_class->field_count -
                                   component_class->static_field_count;
                nested_remaining--;
                nested = component_class->fields + component_class->static_field_count;
                while (nested_remaining >= 0) {
                    if (nested->flags & 0x8000) {
                        nested->type_or_descriptor =
                            (SceneTypeDescriptor *)getClassFromSignature(
                                nested->type_or_descriptor->signature,
                                stage_class->class_loader);
                        nested->flags &= 0x7fff;
                    }
                    if ((SceneClass *)nested->type_or_descriptor == stage_class) {
                        *(SceneObject *)(field_object + nested->instance_offset) = self;
                    }
                    nested++;
                    nested_remaining--;
                }
            }
        }
    next:
        field++;
        remaining--;
    }

    stage_object = self;
    stage_class = ((SceneObjectHeader *)self)->class_ref->scene_class;
    method = findMethod(stage_class, loadConstString(D_004DA630, -1), loadConstString(D_004DA638, -1));
    if (method != 0) {
        arguments[0] = stage_object;
        JNI_initThread(vm);
        JNI_callMethod(vm, method, arguments, 0);
    }
}
