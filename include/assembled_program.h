#ifndef ASSEMBLED_PROGRAM_H
#define ASSEMBLED_PROGRAM_H

#include "symbol_table.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Represents a single distinct piece of memory, such as .text, or .data
 */
typedef struct {
  uint8_t* data;
  size_t size;
} MemorySegment;

/**
 * @brief Represents an assembled program in the form of memory segments
 */
typedef struct {
  SymbolTable* symbol_table;
  MemorySegment text;
  uint32_t entry_offset;

  uint32_t* text_lines; // lines[vaddr / 4] == line num
  size_t text_lines_size;

  MemorySegment data;
  MemorySegment bss;
  MemorySegment rodata;
} AssembledProgram;

#endif