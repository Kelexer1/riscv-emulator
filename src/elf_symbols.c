#include "../include/elf_symbols.h"
#include "../include/elf32.h"

#include <stdlib.h>
#include <string.h>

#define MAX_NAMES_SIZE (16u * 1024u * 1024u)

typedef struct {
  const char* name;
  size_t len;
  SymbolType type;
  Section section;
  uint32_t value;
} ConvertedSymbol;

static Section section_from_header(const Elf32SectionView* sec) {
  if (sec->flags & SHF_EXECINSTR)
    return SECTION_TEXT;
  if (sec->type == SHT_NOBITS)
    return SECTION_BSS;
  if (sec->flags & SHF_WRITE)
    return SECTION_DATA;
  return SECTION_RODATA;
}

static int convert_symbol(const Elf32Image* img, size_t index, ConvertedSymbol* out) {
  Elf32SymbolView sym;
  if (elf32_symbol(img, index, &sym) != 0)
    return 0;
  if (sym.name[0] == '\0' || sym.type == STT_SECTION || sym.type == STT_FILE || sym.shndx == SHN_UNDEF)
    return 0;

  out->name = sym.name;
  out->len = strlen(sym.name);
  out->type = SYMBOL_CONSTANT;
  out->section = SECTION_TEXT;
  out->value = sym.value;

  if (sym.shndx != SHN_ABS) {
    Elf32SectionView sec;
    if (sym.shndx >= SHN_LORESERVE || elf32_section(img, sym.shndx, &sec) != 0)
      return 0;
    if (!(sec.flags & SHF_ALLOC) || sym.value < sec.addr)
      return 0;
    out->type = SYMBOL_LABEL;
    out->section = section_from_header(&sec);
    out->value = sym.value - sec.addr;
  }
  return 1;
}

SymbolTable* symbol_table_from_elf(const Elf32Image* img) {
  if (!img)
    return NULL;

  size_t names_size = 0;
  for (size_t i = 1; i < img->nsyms; i++) {
    ConvertedSymbol cs;
    if (!convert_symbol(img, i, &cs))
      continue;
    if (cs.len + 1 > MAX_NAMES_SIZE - names_size)
      return NULL;
    names_size += cs.len + 1;
  }

  SymbolTable* table = calloc(1, sizeof *table);
  if (!table)
    return NULL;
  if (!arena_init(&table->arena, names_size)) {
    free(table);
    return NULL;
  }

  Symbol** tail = &table->head;
  for (size_t i = 1; i < img->nsyms; i++) {
    ConvertedSymbol cs;
    if (!convert_symbol(img, i, &cs))
      continue;

    Symbol* node = malloc(sizeof *node);
    if (!node) {
      free_symbol_table(table);
      return NULL;
    }
    node->name = arena_alloc(&table->arena, cs.name, cs.len);
    if (!node->name) {
      free(node);
      free_symbol_table(table);
      return NULL;
    }
    node->type = cs.type;
    node->line = 0;
    node->value = cs.value;
    node->section = cs.section;
    node->len = cs.len;
    node->next = NULL;

    *tail = node;
    tail = &node->next;
  }
  return table;
}