/* Game data filled in from the player's disc at start-up (datafill.c).
 *
 * The executable carries the layout of the game's data (labels, sizes and the
 * pointer words, as relocations) but none of its bytes: gen_host_data.py
 * --skeleton writes port/data with every run of the game's own bytes replaced
 * by `.skip n` and listed in a fill table.  xsg_data_fill() copies those runs
 * from SLUS_204.69 and the OV*.OVL files of the disc image. */
#ifndef XSG_DATAFILL_H
#define XSG_DATAFILL_H

#include <stdint.h>

struct xsg_fill {
    char *at;          /* host address of the run */
    uint32_t off;      /* offset in the disc file */
    uint32_t len;      /* bytes on the disc */
    uint32_t host_len; /* bytes the assembler laid out for it (must equal len) */
};

struct xsg_fill_file {
    const char *name;   /* file in the disc's root directory */
    uint32_t size;
    const char *sha256; /* of the whole file, lowercase hex */
};

struct xsg_fill_tab {
    int file; /* index into xsg_fill_files */
    const struct xsg_fill *tab;
};

extern const struct xsg_fill_file xsg_fill_files[];
extern const struct xsg_fill_tab xsg_fill_tabs[];

/* Read a file of the game: <game_dir>/extracted-iso/<name> when present, else
 * from the disc image.  malloc'd; 0 when neither has it. */
int xsg_game_file(const char *name, unsigned char **buf, uint32_t *len);

/* SHA-256 of buf as 64 lowercase hex digits + NUL. */
void xsg_sha256_hex(const unsigned char *buf, uint32_t len, char out[65]);

/* Fill every run; 0 on success, else logs why (wrong or missing file). */
int xsg_data_fill(void);

#endif
