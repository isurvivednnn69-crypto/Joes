#ifndef SO_UTIL_H
#define SO_UTIL_H
#include <stdint.h>
#include <stddef.h>
#include <psp2/types.h>

/* minimal ELF32 definitions (newlib on vitasdk has no <elf.h>) */
typedef struct { uint8_t e_ident[16]; uint16_t e_type, e_machine; uint32_t e_version, e_entry, e_phoff, e_shoff, e_flags;
  uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx; } Elf32_Ehdr;
typedef struct { uint32_t p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align; } Elf32_Phdr;
typedef struct { uint32_t sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize; } Elf32_Shdr;
typedef struct { uint32_t st_name, st_value, st_size; uint8_t st_info, st_other; uint16_t st_shndx; } Elf32_Sym;
typedef struct { uint32_t r_offset, r_info; } Elf32_Rel;
typedef struct { int32_t d_tag; uint32_t d_val; } Elf32_Dyn;

typedef struct {
  uintptr_t base;                 /* load base (vaddr 0) */
  uintptr_t text_base; size_t text_size;
  uintptr_t data_base; size_t data_size;
  SceUID text_blk, data_blk;
  Elf32_Sym *dynsym; uint32_t num_dynsym; const char *dynstr;
  Elf32_Rel *rel; uint32_t num_rel;
  Elf32_Rel *jmprel; uint32_t num_jmprel;
  void (**init_array)(void); uint32_t num_init;
  uint32_t init_fn;
  uintptr_t exidx; uint32_t exidx_size;
} so_module;

typedef struct { const char *name; uintptr_t addr; } so_import;

int  so_load(so_module *m, const char *path, uintptr_t base);
int  so_link(so_module *m, const so_import *tbl, size_t n);   /* relocate + resolve imports, logs unresolved */
int  so_protect(so_module *m);                                /* make text R+X, flush caches */
void so_run_init(so_module *m);
uintptr_t so_symbol(so_module *m, const char *name);
int  so_addr_in_text(so_module *m, uintptr_t a);

void logf_(const char *fmt, ...);
#endif
