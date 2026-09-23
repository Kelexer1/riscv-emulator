#ifndef ELF_SYMBOLS_H
#define ELF_SYMBOLS_H

#include "elf32_load.h"
#include "symbol_table.h"

/**
 * @brief Builds a first-pass style symbol table from the .symtab of a parsed ELF image
 *
 * @param img The parsed ELF image; its backing buffer only needs to outlive this call
 * @return SymbolTable* The symbol table (empty if the ELF has no symbols), or NULL on allocation failure
 *
 * @note Symbol values are section-relative for section symbols and absolute for constants. The line field is 0
 */
SymbolTable* symbol_table_from_elf(const Elf32Image* img);

#endif