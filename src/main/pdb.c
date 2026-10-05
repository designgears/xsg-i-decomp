#include "common.h"
#include "pdb.h"

enum {
    PDB_MAGIC = 0x30304C46,
    PDB_VERSION_RELOCATED = 1
};

void PDB_getEntry(int class_id, void **groups, int *group_count)
{
    PDBHeader *header;
    PDBGroup *group;

    header = (PDBHeader *)class_id;
    if (header->magic != PDB_MAGIC) {
        *groups = 0;
        *group_count = 0;
        return;
    }

    group = header->groups;
    *groups = group;
    *group_count = header->group_count;

    if (header->version != PDB_VERSION_RELOCATED) {
        int group_index;

        for (group_index = 0; group_index < header->group_count; group_index++) {
            PDBEntryRecord *entry;
            int entries_remaining;

            group->name = &((char *)header)[(unsigned int)group->name];
            entry = group->entries;
            entries_remaining = group->entry_count;
            if (entries_remaining > 0) {
                do {
                    entry->name = &((char *)header)[(unsigned int)entry->name];
                    entry->data = &((char *)header)[(unsigned int)entry->data];
                    entry = &entry[1];
                    entries_remaining--;
                } while (entries_remaining > 0);
            }
            group = (PDBGroup *)entry;
        }
        header->version = PDB_VERSION_RELOCATED;
    }
}

extern char D_004DC068[];
static char *getStrIndex(char *string, unsigned int length, int ch);
unsigned int strlen(const char *);
char *strncpy(char *, const char *, unsigned int);
int strncmp(const char *, const char *, unsigned int);

PDBEntryRecord *PDB_findFile(int class_id, char *path)
{
    char dir[256];
    char name[256];
    PDBGroup *group;
    int group_count;
    int len;
    char *sep;
    int dir_len;
    int name_len;
    int i;
    int j;
    PDBEntryRecord *entry;

    len = strlen(path);
    sep = getStrIndex(path, len, '/');
    if (sep != 0) {
        dir_len = sep - path;
        name_len = len - dir_len - 1;
        strncpy(dir, path, dir_len);
        strncpy(name, sep + 1, name_len);
    } else {
        dir_len = 1;
        name_len = len;
        strncpy(dir, D_004DC068, dir_len);
        strncpy(name, path, name_len);
    }
    dir[dir_len] = 0;
    name[name_len] = 0;
    PDB_getEntry(class_id, (void **)&group, &group_count);
    for (i = 0; i < group_count; i++) {
        entry = group->entries;
        if (strncmp(dir, group->name, group->name_length) == 0) {
            for (j = group->entry_count; j > 0; j--) {
                if (strncmp(name, entry->name, entry->name_length) == 0) {
                    return entry;
                }
                entry++;
            }
        } else {
            entry += group->entry_count;
        }
        group = (PDBGroup *)entry;
    }
    return 0;
}

static char *getStrIndex(char *string, unsigned int length, int ch)
{
    int i;

    for (i = length - 1; i >= 0; i--) {
        if (string[i] == ch) {
            return string + i;
        }
    }
    return 0;
}
