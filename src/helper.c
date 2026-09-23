#include "../include/helper.h"

#include <stdio.h>

int strlenequal(const char* a, size_t a_len, const char* b, size_t b_len) {
  if (a_len != b_len)
    return 0;

  for (size_t i = 0; i < a_len; i++) {
    if (a[i] != b[i])
      return 0;
  }

  return 1;
}

void print_binary(uint32_t num) {
  for (int i = 31; i >= 0; i--) {
    fputc((num >> i) & 1 ? '1' : '0', stdout);
  }
}