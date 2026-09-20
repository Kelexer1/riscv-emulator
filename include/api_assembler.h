#ifndef API_ASSEMBLER_H
#define API_ASSEMBLER_H

#include "../include/second_pass.h"

/**
 * @brief Runs the full assembly pipeline on an assembly file as a string, including error checking, and returns an
 * assembled program struct, or NULL if an error occurred.
 *
 * @param assembly The program assembly as a string
 * @return AssembledProgram* The assembled program
 *
 * @note Takes ownership of @p assembly; the string is freed internally regardless of
 * whether assembly succeeds or fails. The caller must not free or reuse it afterward.
 */
AssembledProgram* assemble(char* assembly);

/**
 * @brief Returns the source code line corresponding to the address of an instruction (given as offset into .text)
 *
 * @param prog The assembled program
 * @param text_offset The offset as bytes into text
 * @param out_line Where to write the source line
 * @return int 1 if successful, 0 otherwise
 */
int addr_to_line(const AssembledProgram* prog, uint32_t text_offset, uint32_t* out_line);

/**
 * @brief Returns the best virtual address (as an offset into .text) corresponding to a given line, searching downwards
 * if that line is not directly at an address
 *
 * @param prog The assembled program
 * @param line The source code line
 * @param out_offset Where to write the offset into .text
 * @return int 1 if successful, 0 otherwise
 */
int line_to_addr(const AssembledProgram* prog, uint32_t line, uint32_t* out_offset);

#endif