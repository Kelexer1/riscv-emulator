#ifndef EXPANDER_H
#define EXPANDER_H

#include "parser.h"
#include <stddef.h>

/**
 * @brief Encodes the result of pseudoinstruction expansion: a flat list of real-instruction lines
 */
typedef struct {
  ParsedLine* lines;
  size_t count;
  Arena arena;
} ExpandedInput;

/**
 * @brief Expands all pseudoinstructions in a parsed program into their real-instruction equivalents
 *
 * @param parsed The parsed input to expand; ownership is not taken, the caller must free it separately
 * @return ExpandedInput* The expanded input, or NULL if an error occurred
 */
ExpandedInput* expand_pseudoinstructions(ParsedInput* parsed);

/**
 * @brief Frees an expanded input, including its arena and the struct itself
 *
 * @param expanded The expanded input to free
 */
void free_expanded_input(ExpandedInput* expanded);

#endif