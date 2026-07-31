#define _POSIX_C_SOURCE 200809L

#include "../include/helper.h"
#include "unity/unity.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

/* ---------------------------------------------------------------------
 * to_lowercase
 * ------------------------------------------------------------------- */

void test_to_lowercase_mixed_case(void) {
  char* result = to_lowercase("HeLLo", 5);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("hello", result);
  free(result);
}

void test_to_lowercase_already_lower(void) {
  char* result = to_lowercase("hello", 5);

  TEST_ASSERT_EQUAL_STRING("hello", result);
  free(result);
}

void test_to_lowercase_zero_length(void) {
  char* result = to_lowercase("ABC", 0);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("", result);
  free(result);
}

void test_to_lowercase_only_reads_n_chars(void) {
  char* result = to_lowercase("ABCDEF", 3);

  TEST_ASSERT_EQUAL_STRING("abc", result);
  free(result);
}

void test_to_lowercase_non_alpha_unaffected(void) {
  char* result = to_lowercase("A1_B2", 5);

  TEST_ASSERT_EQUAL_STRING("a1_b2", result);
  free(result);
}

/* ---------------------------------------------------------------------
 * strlenequal
 * ------------------------------------------------------------------- */

void test_strlenequal_equal_strings(void) { TEST_ASSERT_EQUAL_INT(1, strlenequal("abc", 3, "abc", 3)); }

void test_strlenequal_different_lengths(void) { TEST_ASSERT_EQUAL_INT(0, strlenequal("abc", 3, "abcd", 4)); }

void test_strlenequal_same_length_different_content(void) { TEST_ASSERT_EQUAL_INT(0, strlenequal("abc", 3, "abd", 3)); }

void test_strlenequal_both_zero_length(void) { TEST_ASSERT_EQUAL_INT(1, strlenequal("", 0, "", 0)); }

void test_strlenequal_substring_comparison(void) { TEST_ASSERT_EQUAL_INT(1, strlenequal("abcXX", 3, "abcYY", 3)); }

/* ---------------------------------------------------------------------
 * directive_size
 * ------------------------------------------------------------------- */

void test_directive_size_null_line(void) { TEST_ASSERT_EQUAL_UINT32(0, directive_size(NULL, 0)); }

void test_directive_size_byte(void) {
  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_BYTE;
  line.directive.arg_count = 5;

  TEST_ASSERT_EQUAL_UINT32(5, directive_size(&line, 0));
}

void test_directive_size_half(void) {
  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_HALF;
  line.directive.arg_count = 3;

  TEST_ASSERT_EQUAL_UINT32(6, directive_size(&line, 0));
}

void test_directive_size_word(void) {
  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_WORD;
  line.directive.arg_count = 4;

  TEST_ASSERT_EQUAL_UINT32(16, directive_size(&line, 0));
}

void test_directive_size_string(void) {
  DirectiveArg arg = {0};
  arg.len = 11;

  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_STRING;
  line.directive.arg_count = 1;
  line.directive.args = &arg;

  TEST_ASSERT_EQUAL_UINT32(12, directive_size(&line, 0));
}

void test_directive_size_ascii(void) {
  DirectiveArg arg = {0};
  arg.len = 11;

  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_ASCII;
  line.directive.arg_count = 1;
  line.directive.args = &arg;

  TEST_ASSERT_EQUAL_UINT32(11, directive_size(&line, 0));
}

void test_directive_size_zero(void) {
  DirectiveArg arg = {0};
  arg.imm = 64;

  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_ZERO;
  line.directive.arg_count = 1;
  line.directive.args = &arg;

  TEST_ASSERT_EQUAL_UINT32(64, directive_size(&line, 0));
}

void test_directive_size_string_zero_args_returns_zero(void) {
  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_STRING;
  line.directive.arg_count = 0;
  line.directive.args = NULL;

  TEST_ASSERT_EQUAL_UINT32(0, directive_size(&line, 0));
}

void test_directive_size_ascii_zero_args_returns_zero(void) {
  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_ASCII;
  line.directive.arg_count = 0;
  line.directive.args = NULL;

  TEST_ASSERT_EQUAL_UINT32(0, directive_size(&line, 0));
}

void test_directive_size_zero_zero_args_returns_zero(void) {
  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_ZERO;
  line.directive.arg_count = 0;
  line.directive.args = NULL;

  TEST_ASSERT_EQUAL_UINT32(0, directive_size(&line, 0));
}

void test_directive_size_align_needs_padding(void) {
  DirectiveArg arg = {0};
  arg.imm = 2; /* align = 1 << 2 = 4 */

  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_ALIGN;
  line.directive.args = &arg;

  TEST_ASSERT_EQUAL_UINT32(3, directive_size(&line, 5)); /* 5 -> 8 */
}

void test_directive_size_align_already_aligned(void) {
  DirectiveArg arg = {0};
  arg.imm = 3; /* align = 8 */

  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_ALIGN;
  line.directive.args = &arg;

  TEST_ASSERT_EQUAL_UINT32(0, directive_size(&line, 16));
}

void test_directive_size_unknown_directive_returns_zero(void) {
  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = DIRECTIVE_TEXT;

  TEST_ASSERT_EQUAL_UINT32(0, directive_size(&line, 0));
}

/* ---------------------------------------------------------------------
 * print_binary
 * ------------------------------------------------------------------- */

static const char* capture_print_binary(uint32_t num) {
  static char buffer[64];
  char path[] = "/tmp/print_binary_test_XXXXXX";
  int fd = mkstemp(path);
  int saved_stdout = dup(STDOUT_FILENO);

  fflush(stdout);
  dup2(fd, STDOUT_FILENO);

  print_binary(num);

  fflush(stdout);
  dup2(saved_stdout, STDOUT_FILENO);
  close(saved_stdout);

  memset(buffer, 0, sizeof(buffer));
  lseek(fd, 0, SEEK_SET);
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
  (void)bytes_read;
  close(fd);
  unlink(path);

  return buffer;
}

void test_print_binary_zero(void) {
  TEST_ASSERT_EQUAL_STRING("00000000000000000000000000000000", capture_print_binary(0));
}

void test_print_binary_all_ones(void) {
  TEST_ASSERT_EQUAL_STRING("11111111111111111111111111111111", capture_print_binary(0xFFFFFFFF));
}

void test_print_binary_one(void) {
  TEST_ASSERT_EQUAL_STRING("00000000000000000000000000000001", capture_print_binary(1));
}

void test_print_binary_msb_only(void) {
  TEST_ASSERT_EQUAL_STRING("10000000000000000000000000000000", capture_print_binary(0x80000000));
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_to_lowercase_mixed_case);
  RUN_TEST(test_to_lowercase_already_lower);
  RUN_TEST(test_to_lowercase_zero_length);
  RUN_TEST(test_to_lowercase_only_reads_n_chars);
  RUN_TEST(test_to_lowercase_non_alpha_unaffected);

  RUN_TEST(test_strlenequal_equal_strings);
  RUN_TEST(test_strlenequal_different_lengths);
  RUN_TEST(test_strlenequal_same_length_different_content);
  RUN_TEST(test_strlenequal_both_zero_length);
  RUN_TEST(test_strlenequal_substring_comparison);

  RUN_TEST(test_directive_size_null_line);
  RUN_TEST(test_directive_size_byte);
  RUN_TEST(test_directive_size_half);
  RUN_TEST(test_directive_size_word);
  RUN_TEST(test_directive_size_string);
  RUN_TEST(test_directive_size_string_zero_args_returns_zero);
  RUN_TEST(test_directive_size_ascii);
  RUN_TEST(test_directive_size_ascii_zero_args_returns_zero);
  RUN_TEST(test_directive_size_zero);
  RUN_TEST(test_directive_size_zero_zero_args_returns_zero);
  RUN_TEST(test_directive_size_align_needs_padding);
  RUN_TEST(test_directive_size_align_already_aligned);
  RUN_TEST(test_directive_size_unknown_directive_returns_zero);

  RUN_TEST(test_print_binary_zero);
  RUN_TEST(test_print_binary_all_ones);
  RUN_TEST(test_print_binary_one);
  RUN_TEST(test_print_binary_msb_only);

  return UNITY_END();
}