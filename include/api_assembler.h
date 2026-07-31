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

#endif