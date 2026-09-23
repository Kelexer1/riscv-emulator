#ifndef HELPER_H
#define HELPER_H

#include <stddef.h>
#include <stdint.h>

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
 * @brief Prints the 32-bit binary representation of a number to stdout, most significant bit first
 *
 * @param num The number to print
 */
void print_binary(uint32_t num);

#endif