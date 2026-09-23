#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "arena.h"
#include <stdint.h>

/**
 * @brief Represents what kind of information a symbol encodes
 */
typedef enum : uint8_t {
  SYMBOL_LABEL,
  SYMBOL_CONSTANT,
} SymbolType;

/**
 * @brief Represents the section a symbol belongs to
 */
typedef enum : uint8_t {
  SECTION_TEXT,
  SECTION_DATA,
  SECTION_BSS,
  SECTION_RODATA,
} Section;

/**
 * @brief Encodes a single symbol in the assembly in a linked list structure
 */
typedef struct symbol {
  SymbolType type;
  uint32_t line;
  uint32_t value;
  Section section;
  char* name;
  size_t len;
  struct symbol* next;
} Symbol;

/**
 * @brief Encodes a symbol table as a linked list of symbols backed by an arena
 */
typedef struct {
  Symbol* head;
  Arena arena;
} SymbolTable;

/**
 * @brief Determines if a symbol is present in a symbol table, and its value
 *
 * @param symbol_table The head of a symbol table
 * @param symbol The symbol to find
 * @param len The length of the symbol
 * @param out Where to write a copy of the symbol, NULL if the result is not
 * needed
 * @return int 1 if the symbol was found, 0 if not found or an error occurred
 */
int resolve_symbol(SymbolTable* symbol_table, const char* symbol, size_t len, Symbol* out);

/**
 * @brief Finds a symbol of the expected type with a matching value
 *
 * @param symbol_table The symbol table to search
 * @param val The value to match against
 * @param expected_type The symbol type to restrict the search to
 * @return const Symbol* The matching symbol, or NULL if none was found
 */
const Symbol* value_to_symbol(SymbolTable* symbol_table, uint32_t val, SymbolType expected_type);

/**
 * @brief Frees all memory associated with a symbol table, including the arena backing its
 * symbol names (char*'s), and the struct itself
 *
 * @param symbol_table The symbol table
 */
void free_symbol_table(SymbolTable* symbol_table);

#endif