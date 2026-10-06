#include "common.h"
#include "shared.h"

/* These cursors begin at the sentinel used before a resource table is found. */
static int infoIndex = -1;
static int infoLength = -1;
extern char D_004C23C0[];
static char *rsrcDefaultPath = D_004C23C0;

typedef struct RsrcManager {
    u8 *heap_origin;
    u8 *heap_cursor;
    u32 heap_bound;
    void *items;
    u16 item_count;
    u16 item_capacity;
    u8 unmodeled_14[12];
} RsrcManager;

typedef struct RsrcItem {
    u8 unmodeled_00[5];
    u8 item_id;
    u16 flags;
    void *data;
    u32 key;
} RsrcItem;

void RSRC_inactiveSource(RsrcManager *manager, int source)
{
    u16 count = manager->item_count;
    int index = 0;
    RsrcItem *item = manager->items;

    if (count != 0) {
        do {
            if (item->key == source) {
                item->key = 0;
                item->flags |= 0x8000;
            }
            index++;
            item++;
        } while (index < manager->item_count);
    }
}

/*
 * The arena is one contiguous block carved out of the caller's memory:
 * general heap allocations grow upward from `memory`, the fixed-size
 * item table (item_capacity RsrcItem slots) sits directly below the
 * manager, and the manager struct itself occupies the last
 * sizeof(RsrcManager) bytes of the block. `items` is therefore reached
 * by walking back item_capacity RsrcItem slots from `manager`, and
 * `heap_bound` is the byte offset where the item table begins, i.e. the
 * heap's usable limit.
 */
RsrcManager *RSRC_create(u8 *memory, u32 memory_size, int item_capacity)
{
    RsrcManager *manager;

    memset(memory, 0, memory_size);
    manager = (RsrcManager *)(memory + memory_size - sizeof(RsrcManager));
    manager->heap_cursor = manager->heap_origin = memory;
    manager->heap_bound = memory_size - item_capacity * sizeof(RsrcItem) - sizeof(RsrcManager);
    manager->items = (RsrcItem *)manager - item_capacity;
    manager->item_capacity = item_capacity;
    manager->item_count = 0;
    return manager;
}

void RSRC_setPrintParam(int index, int length)
{
    infoIndex = index;
    infoLength = length;
}

extern char *normalSTR[][2];
extern char *selectedSTR[][2];
extern char *unselectedSTR[][2];
extern char *markedSTR[][2];
char *strcat(char *, const char *);
void DB_pathGetShortPath(char *, char *, int);

int RSRC_info(RsrcManager *manager, int mode)
{
    char buf0[128];
    char buf1[128];
    char path[128];
    int doPath;
    int start;
    int i;
    int end;
    int count;
    RsrcItem *item;

    start = infoIndex;
    if (start < 0) start = 0;
    doPath = mode & 0xF;
    if (infoLength < 0) {
        end = count = manager->item_count;
    } else {
        count = manager->item_count;
        end = start + infoLength;
        if (count < end) end = count;
    }
    if (start < 0 || start >= count) start = 0;
    i = start;
    while (i < end) {
        buf1[0] = 0;
        buf0[0] = 0;
        item = (RsrcItem *)manager->items + i;
        i++;
        if (item->flags & 2) {
            strcat(buf0, selectedSTR[mode][0]);
            strcat(buf1, selectedSTR[mode][1]);
        } else {
            strcat(buf0, unselectedSTR[mode][0]);
            strcat(buf1, unselectedSTR[mode][1]);
        }
        if (item->flags & 1) {
            strcat(buf0, markedSTR[mode][0]);
            strcat(buf1, markedSTR[mode][1]);
        } else {
            strcat(buf0, normalSTR[mode][0]);
            strcat(buf1, normalSTR[mode][1]);
        }
        if (item->unmodeled_00[4] == 1 && doPath) {
            DB_pathGetShortPath(path, (char *)item->key, 0x16);
        }
    }
    infoIndex = -1;
    infoLength = -1;
    return 0;
}

char *RSRC_getDefaultPath(void)
{
    return rsrcDefaultPath;
}

char *RSRC_setDefaultPath(char *path)
{
    rsrcDefaultPath = path;
    return path;
}

void RSRC_init(RsrcManager *manager)
{
    u16 *item_flags;
    int index = 0;

    manager->heap_cursor = manager->heap_origin;
    manager->item_count = 0;

    if (manager->item_capacity != 0) {
        item_flags = &((RsrcItem *)manager->items)->flags;
        do {
            *item_flags = 0;
            index++;
            item_flags += sizeof(RsrcItem) / sizeof(*item_flags);
        } while (index < manager->item_capacity);
    }
}

int RSRC_check(RsrcManager *manager, int size)
{
    if ((u32)((manager->heap_cursor + size) - manager->heap_origin) < manager->heap_bound) {
        return 0;
    }
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/rsrc", RSRC_alloc);

void *RSRC_searchFile(RsrcManager *manager, char *file_name)
{
  int index = 0;
  u16 count = manager->item_count;
  u32 name_len = strlen(file_name);
  RsrcItem *item = manager->items;
  if (count != 0)
  {
    char *fname = file_name + name_len;
    (void) fname;
    do
    {
      if (item->unmodeled_00[4] == 1)
      {
        if (strncmp((char *) item->key, file_name, name_len) == 0)
        {
          return item->data;
        }
      }
      index++;
      item++;
    }
    while (index < count);
  }
  return 0;
}

static int isLeave(RsrcItem *item)
{
    return item->flags & 2;
}

INCLUDE_ASM("asm/main/nonmatchings/rsrc", RSRC_dispose);

RsrcItem *RSRC_getItemID(RsrcManager *manager, int id)
{
    u16 count = manager->item_count;
    RsrcItem *item = manager->items;
    int index = 0;

    if (count != 0) {
        do {
            if (item->item_id == id)
                return item;
            index++;
            item++;
        } while (index < count);
    }
    return 0;
}

RsrcItem *RSRC_getItem2(RsrcManager *manager, u32 key)
{
    u16 count = manager->item_count;
    RsrcItem *item = manager->items;
    int index = 0;

    if (count != 0) {
        do {
            if (item->key == key)
                return item;
            index++;
            item++;
        } while (index < count);
    }
    return 0;
}

RsrcItem *RSRC_getItem(RsrcManager *manager, void *data)
{
    int index;
    u16 count = manager->item_count;
    RsrcItem *item = manager->items;

    if (data == 0)
        return 0;

    for (index = 0; index < count; index++) {
        if (item->data == data)
            return item;
        item++;
    }
    return 0;
}

extern void *RSRC_loadFileSub(RsrcManager *manager, char *path, char *file_name);

INCLUDE_ASM("asm/main/nonmatchings/rsrc", RSRC_loadFileSub);

void *RSRC_loadFile2(RsrcManager *manager, char *path, char *file_name)
{
    return RSRC_loadFileSub(manager, path, file_name);
}

void *RSRC_loadFile(RsrcManager *manager, char *file_name)
{
    return RSRC_loadFile2(manager, rsrcDefaultPath, file_name);
}

RsrcItem *RSRC_getDirtyItem(RsrcManager *manager, u32 min)
{
    u16 count = manager->item_count;
    RsrcItem *item = manager->items;
    int index;

    if (min == 0) {
        RsrcItem *best = 0;
        min = -1;
        if (count != 0) {
            index = count;
            do {
                index--;
                if (item->flags & 0x8000) {
                    u32 v = *(u32 *)item;
                    if (min >= v) {
                        min = v;
                        best = item;
                    }
                }
                item++;
            } while (index != 0);
        }
        return best;
    }
    index = 0;
    if (count != 0) {
        do {
            if ((item->flags & 0x8000) && *(u32 *)item >= min)
                return item;
            index++;
            item++;
        } while (index < count);
    }
    return 0;
}
