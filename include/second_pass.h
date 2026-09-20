#ifndef SECOND_PASS_H
#define SECOND_PASS_H

#include "expander.h"
#include "first_pass.h"
#include <stddef.h>

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

/**
 * @brief Finalizes the assembling process by encoding an expanded program into memory segments
 *
 * @param expanded The expanded program to finalize
 * @param symbol_table The symbol table, used to resolve instruction operands and the entry point
 * @return AssembledProgram* The assembled program, or NULL if an error occurred
 *
 * @note On success, ownership of symbol_table transfers to the returned AssembledProgram and is
 * freed together with it via free_assembled_program. On failure, symbol_table is not freed and
 * remains owned by the caller
 */
AssembledProgram* finalize_assembly(ExpandedInput* expanded, SymbolTable* symbol_table);

/**
 * @brief Frees all associated memory used by an assembled program, including its symbol table
 *
 * @param program The assembled program
 */
void free_assembled_program(AssembledProgram* program);

#endif