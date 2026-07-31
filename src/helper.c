#include "../include/helper.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* to_lowercase(const char* s, size_t n) {
  char* s_cpy = malloc(n + 1);
  if (!s_cpy)
    return NULL;

  strncpy(s_cpy, s, n);

  for (size_t i = 0; i < n; i++)
    s_cpy[i] = tolower(s_cpy[i]);
  s_cpy[n] = '\0';
  return s_cpy;
}

int strlenequal(const char* a, size_t a_len, const char* b, size_t b_len) {
  if (a_len != b_len)
    return 0;

  for (size_t i = 0; i < a_len; i++) {
    if (a[i] != b[i])
      return 0;
  }

  return 1;
}

uint32_t directive_size(ParsedLine* line, uint32_t curr_size) {
  if (!line)
    return 0;

  switch (line->directive.directive) {
  case DIRECTIVE_BYTE:
    return line->directive.arg_count;
  case DIRECTIVE_HALF:
    return line->directive.arg_count * 2;
  case DIRECTIVE_WORD:
    return line->directive.arg_count * 4;
  case DIRECTIVE_STRING:
    if (!line->directive.arg_count)
      return 0;
    return line->directive.args->len + 1;
  case DIRECTIVE_ASCII:
    if (!line->directive.arg_count)
      return 0;
    return line->directive.args->len;
  case DIRECTIVE_ZERO:
    if (!line->directive.arg_count)
      return 0;
    return line->directive.args->imm;
  case DIRECTIVE_ALIGN:
    uint32_t align = 1 << line->directive.args->imm;
    uint32_t padded = (curr_size + align - 1) & ~(align - 1);
    return padded - curr_size;
  default:
    break;
  }

  return 0;
}

void print_binary(uint32_t num) {
  for (int i = 31; i >= 0; i--) {
    fputc((num >> i) & 1 ? '1' : '0', stdout);
  }
}