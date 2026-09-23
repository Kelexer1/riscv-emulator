#ifndef ELF32_LOAD_H
#define ELF32_LOAD_H

#include <stddef.h>
#include <stdint.h>

#define ELF32_MAX_SEGMENTS 16

typedef enum {
  ELF32_OK = 0,
  ELF32_ERR_TRUNCATED,
  ELF32_ERR_MAGIC,
  ELF32_ERR_CLASS,
  ELF32_ERR_ENDIAN,
  ELF32_ERR_VERSION,
  ELF32_ERR_MACHINE,
  ELF32_ERR_TYPE,
  ELF32_ERR_FLAGS,
  ELF32_ERR_PHDR,
  ELF32_ERR_SEGMENT,
  ELF32_ERR_TOO_MANY_SEGMENTS,
  ELF32_ERR_NO_SEGMENTS,
  ELF32_ERR_SHDR,
  ELF32_ERR_SYMTAB
} Elf32Status;

typedef struct {
  uint32_t vaddr;
  uint32_t filesz;
  uint32_t memsz;
  uint32_t flags;
  const uint8_t* data;
} Elf32LoadSegment;

typedef struct {
  const char* name;
  uint32_t value;
  uint32_t size;
  uint8_t type;
  uint8_t bind;
  uint16_t shndx;
} Elf32SymbolView;

typedef struct {
  uint32_t addr;
  uint32_t size;
  uint32_t type;
  uint32_t flags;
} Elf32SectionView;

typedef struct {
  uint32_t entry;
  Elf32LoadSegment segments[ELF32_MAX_SEGMENTS];
  size_t nsegments;
  const uint8_t* shdrs;
  size_t nshdrs;
  const uint8_t* symtab;
  size_t nsyms;
  const char* strtab;
  size_t strtab_len;
} Elf32Image;

Elf32Status elf32_parse(const uint8_t* buf, size_t len, Elf32Image* out);
int elf32_symbol(const Elf32Image* img, size_t index, Elf32SymbolView* out);
int elf32_section(const Elf32Image* img, size_t index, Elf32SectionView* out);
const char* elf32_status_str(Elf32Status status);

#endif