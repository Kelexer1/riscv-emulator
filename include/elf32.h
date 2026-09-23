#ifndef ELF32_H
#define ELF32_H

#include <stdint.h>

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "elf32.h serializes structs directly and requires a little-endian host"
#endif

typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;

#define EI_NIDENT 16

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_ABIVERSION 8
#define EI_PAD 9

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS32 1
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ELFOSABI_NONE 0

#define ET_EXEC 2
#define EM_RISCV 243

#define EF_RISCV_RVC 0x0001
#define EF_RISCV_FLOAT_ABI_MASK 0x0006
#define EF_RISCV_RVE 0x0008

#define PT_NULL 0
#define PT_LOAD 1

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_NOBITS 8

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4

#define SHN_UNDEF 0
#define SHN_LORESERVE 0xff00
#define SHN_ABS 0xfff1

#define STB_LOCAL 0
#define STB_GLOBAL 1

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_SECTION 3
#define STT_FILE 4

#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) & 0xf)
#define ELF32_ST_INFO(b, t) (((b) << 4) | ((t) & 0xf))

typedef struct {
  unsigned char e_ident[EI_NIDENT];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
} Elf32_Ehdr;

typedef struct {
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
} Elf32_Phdr;

typedef struct {
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr;
  Elf32_Off sh_offset;
  Elf32_Word sh_size;
  Elf32_Word sh_link;
  Elf32_Word sh_info;
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;
} Elf32_Shdr;

typedef struct {
  Elf32_Word st_name;
  Elf32_Addr st_value;
  Elf32_Word st_size;
  unsigned char st_info;
  unsigned char st_other;
  Elf32_Half st_shndx;
} Elf32_Sym;

_Static_assert(sizeof(Elf32_Ehdr) == 52, "Elf32_Ehdr must be 52 bytes");
_Static_assert(sizeof(Elf32_Phdr) == 32, "Elf32_Phdr must be 32 bytes");
_Static_assert(sizeof(Elf32_Shdr) == 40, "Elf32_Shdr must be 40 bytes");
_Static_assert(sizeof(Elf32_Sym) == 16, "Elf32_Sym must be 16 bytes");

#endif