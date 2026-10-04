/* Minimal 32-bit ARM ELF loader for Android .so files on Vita (needs kubridge). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/sysmem.h>
#include <kubridge.h>
#include "so_util.h"

#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_ARM_EXIDX 0x70000001
#define PF_X 1
#define PF_W 2
#define SHT_DYNSYM 11
#define DT_PLTRELSZ 2
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_REL 17
#define DT_RELSZ 18
#define DT_JMPREL 23
#define DT_INIT_ARRAY 25
#define DT_INIT_ARRAYSZ 27
#define DT_INIT 12
#define R_ARM_ABS32 2
#define R_ARM_GLOB_DAT 21
#define R_ARM_JUMP_SLOT 22
#define R_ARM_RELATIVE 23

#define ALIGN_UP(x) (((x) + 0xFFF) & ~0xFFFu)
#define ALIGN_DN(x) ((x) & ~0xFFFu)

extern uintptr_t unresolved_stub_addr(void);

static SceUID alloc_fixed(const char *name, uintptr_t addr, size_t size) {
  SceKernelAllocMemBlockKernelOpt opt;
  memset(&opt, 0, sizeof(opt));
  opt.size = sizeof(opt);
  opt.attr = 0x1;                 /* use fixed base address */
  opt.field_C = (SceUInt32)addr;
  SceUID id = kuKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size, &opt);
  if (id < 0) { logf_("alloc_fixed(%s, 0x%08x, 0x%x) failed: 0x%08x\n", name, (unsigned)addr, (unsigned)size, id); return id; }
  void *got = NULL;
  sceKernelGetMemBlockBase(id, &got);
  if ((uintptr_t)got != addr) { logf_("alloc_fixed(%s): got %p, wanted 0x%08x\n", name, got, (unsigned)addr); return -1; }
  return id;
}

int so_load(so_module *m, const char *path, uintptr_t base) {
  memset(m, 0, sizeof(*m));
  FILE *f = fopen(path, "rb");
  if (!f) { logf_("cannot open %s\n", path); return -1; }
  fseek(f, 0, SEEK_END); size_t sz = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *buf = malloc(sz);
  if (!buf || fread(buf, 1, sz, f) != sz) { logf_("read failed\n"); fclose(f); return -2; }
  fclose(f);

  Elf32_Ehdr *eh = (Elf32_Ehdr *)buf;
  if (memcmp(eh->e_ident, "\x7f" "ELF", 4) != 0 || eh->e_machine != 40) { logf_("not an ARM ELF\n"); return -3; }
  Elf32_Phdr *ph = (Elf32_Phdr *)(buf + eh->e_phoff);

  Elf32_Phdr *tx = NULL, *dt = NULL, *dyn = NULL;
  for (int i = 0; i < eh->e_phnum; i++) {
    if (ph[i].p_type == PT_LOAD) {
      if ((ph[i].p_flags & PF_X) && !tx) tx = &ph[i];
      else if ((ph[i].p_flags & PF_W) && !dt) dt = &ph[i];
    } else if (ph[i].p_type == PT_DYNAMIC) dyn = &ph[i];
    else if (ph[i].p_type == PT_ARM_EXIDX) { m->exidx = base + ph[i].p_vaddr; m->exidx_size = ph[i].p_memsz; }
  }
  if (!tx || !dt || !dyn) { logf_("unexpected segment layout\n"); return -4; }

  m->base = base;
  m->text_base = base; m->text_size = ALIGN_UP(tx->p_vaddr + tx->p_memsz);
  m->data_base = base + ALIGN_DN(dt->p_vaddr);
  m->data_size = ALIGN_UP(dt->p_vaddr + dt->p_memsz) - ALIGN_DN(dt->p_vaddr);
  logf_("text 0x%08x +0x%x, data 0x%08x +0x%x\n", (unsigned)m->text_base, (unsigned)m->text_size, (unsigned)m->data_base, (unsigned)m->data_size);

  m->text_blk = alloc_fixed("so_text", m->text_base, m->text_size);
  if (m->text_blk < 0) return -5;
  m->data_blk = alloc_fixed("so_data", m->data_base, m->data_size);
  if (m->data_blk < 0) return -6;

  memset((void *)m->text_base, 0, m->text_size);
  memset((void *)m->data_base, 0, m->data_size);
  memcpy((void *)(base + tx->p_vaddr), buf + tx->p_offset, tx->p_filesz);
  memcpy((void *)(base + dt->p_vaddr), buf + dt->p_offset, dt->p_filesz);

  /* dynamic section */
  Elf32_Dyn *d = (Elf32_Dyn *)(base + dyn->p_vaddr);
  uint32_t hash = 0, relsz = 0, jmprelsz = 0, initarr_sz = 0;
  for (; d->d_tag != 0; d++) {
    switch (d->d_tag) {
      case DT_HASH: hash = d->d_val; break;
      case DT_STRTAB: m->dynstr = (const char *)(base + d->d_val); break;
      case DT_SYMTAB: m->dynsym = (Elf32_Sym *)(base + d->d_val); break;
      case DT_REL: m->rel = (Elf32_Rel *)(base + d->d_val); break;
      case DT_RELSZ: relsz = d->d_val; break;
      case DT_JMPREL: m->jmprel = (Elf32_Rel *)(base + d->d_val); break;
      case DT_PLTRELSZ: jmprelsz = d->d_val; break;
      case DT_INIT_ARRAY: m->init_array = (void (**)(void))(base + d->d_val); break;
      case DT_INIT_ARRAYSZ: initarr_sz = d->d_val; break;
      case DT_INIT: m->init_fn = d->d_val; break;
    }
  }
  m->num_rel = relsz / sizeof(Elf32_Rel);
  m->num_jmprel = jmprelsz / sizeof(Elf32_Rel);
  m->num_init = initarr_sz / 4;

  /* symbol count: section headers first, DT_HASH as fallback */
  if (eh->e_shoff && eh->e_shnum) {
    Elf32_Shdr *sh = (Elf32_Shdr *)(buf + eh->e_shoff);
    for (int i = 0; i < eh->e_shnum; i++)
      if (sh[i].sh_type == SHT_DYNSYM) { m->num_dynsym = sh[i].sh_size / sizeof(Elf32_Sym); break; }
  }
  if (!m->num_dynsym && hash) m->num_dynsym = ((uint32_t *)(base + hash))[1];
  logf_("dynsym=%u rel=%u jmprel=%u init_array=%u\n", m->num_dynsym, m->num_rel, m->num_jmprel, m->num_init);
  free(buf);
  return 0;
}

static const so_import *find_import(const so_import *tbl, size_t n, const char *name) {
  for (size_t i = 0; i < n; i++) if (strcmp(tbl[i].name, name) == 0) return &tbl[i];
  return NULL;
}

static int unresolved_count = 0;

static void apply(so_module *m, Elf32_Rel *r, uint32_t n, const so_import *tbl, size_t tn) {
  for (uint32_t i = 0; i < n; i++) {
    uint32_t type = r[i].r_info & 0xFF, symi = r[i].r_info >> 8;
    uint32_t *where = (uint32_t *)(m->base + r[i].r_offset);
    Elf32_Sym *sym = symi ? &m->dynsym[symi] : NULL;
    uintptr_t S = 0;
    if (sym) {
      if (sym->st_shndx != 0) S = m->base + sym->st_value;
      else {
        const char *name = m->dynstr + sym->st_name;
        const so_import *imp = find_import(tbl, tn, name);
        if (imp) S = imp->addr;
        else { S = unresolved_stub_addr(); logf_("UNRESOLVED import: %s\n", name); unresolved_count++; }
      }
    }
    switch (type) {
      case R_ARM_RELATIVE: *where += m->base; break;
      case R_ARM_ABS32: *where += S; break;
      case R_ARM_GLOB_DAT:
      case R_ARM_JUMP_SLOT: *where = S; break;
      default: logf_("ignoring reloc type %u at 0x%08x\n", type, (unsigned)r[i].r_offset); break;
    }
  }
}

int so_link(so_module *m, const so_import *tbl, size_t n) {
  apply(m, m->rel, m->num_rel, tbl, n);
  apply(m, m->jmprel, m->num_jmprel, tbl, n);
  logf_("link done, %d unresolved import slots\n", unresolved_count);
  return unresolved_count;
}

int so_protect(so_module *m) {
  int r = kuKernelMemProtect((void *)m->text_base, m->text_size, KU_KERNEL_PROT_READ | KU_KERNEL_PROT_EXEC);
  logf_("kuKernelMemProtect(text) = 0x%08x\n", r);
  kuKernelFlushCaches((void *)m->text_base, m->text_size);
  kuKernelFlushCaches((void *)m->data_base, m->data_size);
  return r;
}

void so_run_init(so_module *m) {
  if (m->init_fn) { logf_("DT_INIT\n"); ((void (*)(void))(m->base + m->init_fn))(); }
  for (uint32_t i = 0; i < m->num_init; i++) {
    uintptr_t fn = (uintptr_t)m->init_array[i];
    if (fn == 0 || fn == (uintptr_t)-1) continue;
    ((void (*)(void))fn)();
  }
  logf_("init arrays done (%u)\n", m->num_init);
}

uintptr_t so_symbol(so_module *m, const char *name) {
  for (uint32_t i = 0; i < m->num_dynsym; i++)
    if (m->dynsym[i].st_shndx != 0 && strcmp(m->dynstr + m->dynsym[i].st_name, name) == 0)
      return m->base + m->dynsym[i].st_value;
  return 0;
}

int so_addr_in_text(so_module *m, uintptr_t a) { return a >= m->text_base && a < m->text_base + m->text_size; }
