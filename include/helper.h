#ifndef HELPER_H
#define HELPER_H

#include "parser.h"
#include <stddef.h>

/**
 * @brief Returns a lowercase copy of the given string up to a null-terminator
 * or n chars, null-terminated. Must be freed by caller
 *
 * @param s The char sequence, not necessarily null-terminated
 * @param n The number of characters in the string, counted from the start of s
 * @return char* A pointer to a null-terminated string
 */
char* to_lowercase(const char* s, size_t n);

/**
 * @brief Checks equality between two strings defined as character arrays with
 * sizes
 *
 * @param a The first string to compare
 * @param a_len The size of the first string
 * @param b The second string to compare
 * @param b_len The size of the second string
 * @return int 1 if the strings are equal, 0 if they are not equal or an error
 * occurred
 */
int strlenequal(const char* a, size_t a_len, const char* b, size_t b_len);

/**
 * @brief Computes the number of bytes a directive line occupies in its section
 *
 * @param line The parsed line containing the directive; must be of type LINE_DIRECTIVE
 * @param curr_size The current size in bytes of the section, used to compute alignment padding
 * @return uint32_t The number of bytes the directive occupies, 0 if the directive has no size or
 * an error occurred
 */
uint32_t directive_size(ParsedLine* line, uint32_t curr_size);

/**
 * @brief Prints the 32-bit binary representation of a number to stdout, most significant bit first
 *
 * @param num The number to print
 */
void print_binary(uint32_t num);

#endif