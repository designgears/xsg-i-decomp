/* Windows port: the ELF32 definitions host_main.c needs to copy SLUS_204.69's segments. */
#ifndef XSG_W32_ELF_H
#define XSG_W32_ELF_H
#include <stdint.h>
#define EI_NIDENT 16
#define ELFMAG "\177ELF"
#define SELFMAG 4
#define PT_LOAD 1
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type, e_machine;
    uint32_t e_version, e_entry, e_phoff, e_shoff, e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf32_Ehdr;
typedef struct {
    uint32_t p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align;
} Elf32_Phdr;
#endif
