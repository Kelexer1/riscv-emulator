#include "../include/elf32_load.h"
#include "../include/elf32.h"

#include <string.h>

static int in_range(size_t len, uint64_t off, uint64_t size) { return off <= len && size <= len - off; }

Elf32Status elf32_parse(const uint8_t* buf, size_t len, Elf32Image* out) {
  Elf32_Ehdr eh;

  memset(out, 0, sizeof *out);
  if (len < 4 || buf[EI_MAG0] != ELFMAG0 || buf[EI_MAG1] != ELFMAG1 || buf[EI_MAG2] != ELFMAG2 ||
      buf[EI_MAG3] != ELFMAG3)
    return ELF32_ERR_MAGIC;
  if (len < sizeof eh)
    return ELF32_ERR_TRUNCATED;
  memcpy(&eh, buf, sizeof eh);

  if (eh.e_ident[EI_CLASS] != ELFCLASS32)
    return ELF32_ERR_CLASS;
  if (eh.e_ident[EI_DATA] != ELFDATA2LSB)
    return ELF32_ERR_ENDIAN;
  if (eh.e_ident[EI_VERSION] != EV_CURRENT || eh.e_version != EV_CURRENT)
    return ELF32_ERR_VERSION;
  if (eh.e_machine != EM_RISCV)
    return ELF32_ERR_MACHINE;
  if (eh.e_type != ET_EXEC)
    return ELF32_ERR_TYPE;
  if (eh.e_flags & (EF_RISCV_RVC | EF_RISCV_FLOAT_ABI_MASK | EF_RISCV_RVE))
    return ELF32_ERR_FLAGS;

  if (eh.e_phentsize != sizeof(Elf32_Phdr))
    return ELF32_ERR_PHDR;
  if (!in_range(len, eh.e_phoff, (uint64_t)eh.e_phnum * sizeof(Elf32_Phdr)))
    return ELF32_ERR_TRUNCATED;

  for (size_t i = 0; i < eh.e_phnum; i++) {
    Elf32_Phdr ph;
    memcpy(&ph, buf + eh.e_phoff + i * sizeof ph, sizeof ph);
    if (ph.p_type != PT_LOAD)
      continue;
    if (ph.p_filesz > ph.p_memsz)
      return ELF32_ERR_SEGMENT;
    if ((uint64_t)ph.p_vaddr + ph.p_memsz > 0x100000000ULL)
      return ELF32_ERR_SEGMENT;
    if (ph.p_filesz && !in_range(len, ph.p_offset, ph.p_filesz))
      return ELF32_ERR_TRUNCATED;
    if (out->nsegments == ELF32_MAX_SEGMENTS)
      return ELF32_ERR_TOO_MANY_SEGMENTS;

    Elf32LoadSegment* seg = &out->segments[out->nsegments++];
    seg->vaddr = ph.p_vaddr;
    seg->filesz = ph.p_filesz;
    seg->memsz = ph.p_memsz;
    seg->flags = ph.p_flags;
    seg->data = ph.p_filesz ? buf + ph.p_offset : NULL;
  }
  if (out->nsegments == 0)
    return ELF32_ERR_NO_SEGMENTS;
  out->entry = eh.e_entry;

  if (eh.e_shoff != 0 && eh.e_shnum != 0) {
    if (eh.e_shentsize != sizeof(Elf32_Shdr))
      return ELF32_ERR_SHDR;
    if (!in_range(len, eh.e_shoff, (uint64_t)eh.e_shnum * sizeof(Elf32_Shdr)))
      return ELF32_ERR_SHDR;
    out->shdrs = buf + eh.e_shoff;
    out->nshdrs = eh.e_shnum;

    for (size_t i = 0; i < eh.e_shnum; i++) {
      Elf32_Shdr sh, strsh;
      memcpy(&sh, buf + eh.e_shoff + i * sizeof sh, sizeof sh);
      if (sh.sh_type != SHT_SYMTAB)
        continue;
      if (sh.sh_entsize != sizeof(Elf32_Sym) || sh.sh_link >= eh.e_shnum)
        return ELF32_ERR_SYMTAB;
      memcpy(&strsh, buf + eh.e_shoff + (size_t)sh.sh_link * sizeof strsh, sizeof strsh);
      if (strsh.sh_type != SHT_STRTAB)
        return ELF32_ERR_SYMTAB;
      if (!in_range(len, sh.sh_offset, sh.sh_size) || !in_range(len, strsh.sh_offset, strsh.sh_size))
        return ELF32_ERR_SYMTAB;
      out->symtab = buf + sh.sh_offset;
      out->nsyms = sh.sh_size / sizeof(Elf32_Sym);
      out->strtab = (const char*)buf + strsh.sh_offset;
      out->strtab_len = strsh.sh_size;
      break;
    }
  }
  return ELF32_OK;
}

int elf32_symbol(const Elf32Image* img, size_t index, Elf32SymbolView* out) {
  Elf32_Sym s;

  if (index >= img->nsyms)
    return -1;
  memcpy(&s, img->symtab + index * sizeof s, sizeof s);
  if (s.st_name >= img->strtab_len)
    return -1;
  if (!memchr(img->strtab + s.st_name, 0, img->strtab_len - s.st_name))
    return -1;

  out->name = img->strtab + s.st_name;
  out->value = s.st_value;
  out->size = s.st_size;
  out->type = ELF32_ST_TYPE(s.st_info);
  out->bind = ELF32_ST_BIND(s.st_info);
  out->shndx = s.st_shndx;
  return 0;
}

int elf32_section(const Elf32Image* img, size_t index, Elf32SectionView* out) {
  Elf32_Shdr sh;

  if (index >= img->nshdrs)
    return -1;
  memcpy(&sh, img->shdrs + index * sizeof sh, sizeof sh);
  out->addr = sh.sh_addr;
  out->size = sh.sh_size;
  out->type = sh.sh_type;
  out->flags = sh.sh_flags;
  return 0;
}

const char* elf32_status_str(Elf32Status status) {
  switch (status) {
  case ELF32_OK:
    return "ok";
  case ELF32_ERR_TRUNCATED:
    return "file truncated or offsets out of range";
  case ELF32_ERR_MAGIC:
    return "not an ELF file";
  case ELF32_ERR_CLASS:
    return "not a 32-bit ELF";
  case ELF32_ERR_ENDIAN:
    return "not little-endian";
  case ELF32_ERR_VERSION:
    return "unsupported ELF version";
  case ELF32_ERR_MACHINE:
    return "not a RISC-V ELF";
  case ELF32_ERR_TYPE:
    return "not an executable (ET_EXEC)";
  case ELF32_ERR_FLAGS:
    return "unsupported e_flags (RVC, float ABI, or RVE)";
  case ELF32_ERR_PHDR:
    return "bad program header table";
  case ELF32_ERR_SEGMENT:
    return "malformed PT_LOAD segment";
  case ELF32_ERR_TOO_MANY_SEGMENTS:
    return "too many PT_LOAD segments";
  case ELF32_ERR_NO_SEGMENTS:
    return "no PT_LOAD segments";
  case ELF32_ERR_SHDR:
    return "bad section header table";
  case ELF32_ERR_SYMTAB:
    return "malformed symbol table";
  }
  return "unknown error";
}