#ifndef API_DISASSEMBLER_H
#define API_DISASSEMBLER_H

#include "binary_to_instruction.h"
#include "symbol_table.h"

/**
 * @brief Formats a decoded instruction into a human-readable disassembly string
 *
 * @param addr The address of the instruction, used to resolve branch/jump targets
 * @param ins The decoded instruction to disassemble
 * @param symbol_table The symbol table used to resolve immediates and targets to symbol names, if possible
 * @return char* A newly allocated disassembly string, or NULL on failure. The caller is responsible for freeing it
 */
char* disassemble_instruction(uint32_t addr, DecodedInstruction* ins, SymbolTable* symbol_table);

#endif