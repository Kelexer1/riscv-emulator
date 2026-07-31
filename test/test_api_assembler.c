#include "../include/api_assembler.h"
#include "unity/unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* assemble() takes ownership of and unconditionally free()s the buffer passed to it, so every input here must
 * be a fresh heap allocation */
static char* dup_str(const char* s) {
  size_t len = strlen(s);
  char* copy = malloc(len + 1);
  memcpy(copy, s, len + 1);
  return copy;
}

/* ---------------------------------------------------------------------
 * assemble
 * ------------------------------------------------------------------- */

void test_assemble_null_input_returns_null(void) { TEST_ASSERT_NULL(assemble(NULL)); }

void test_assemble_simple_program_succeeds(void) {
  AssembledProgram* prog = assemble(dup_str("_start:\n  add x0, x0, x0\n"));

  TEST_ASSERT_NOT_NULL(prog);
  TEST_ASSERT_EQUAL_UINT(4, prog->text.size);
  TEST_ASSERT_EQUAL_UINT32(0, prog->entry_offset);

  free_assembled_program(prog);
}

void test_assemble_pseudo_instruction_expands_end_to_end(void) {
  /* li expands to lui + addi -> 2 instructions -> 8 bytes */
  AssembledProgram* prog = assemble(dup_str("_start:\n  li a0, 100\n"));

  TEST_ASSERT_NOT_NULL(prog);
  TEST_ASSERT_EQUAL_UINT(8, prog->text.size);

  free_assembled_program(prog);
}

void test_assemble_data_only_program_succeeds(void) {
  AssembledProgram* prog = assemble(dup_str(".data\nmyvar: .word 42\n"));

  TEST_ASSERT_NOT_NULL(prog);
  TEST_ASSERT_EQUAL_UINT(0, prog->text.size);
  TEST_ASSERT_EQUAL_UINT(4, prog->data.size);
  TEST_ASSERT_EQUAL_UINT32(0, prog->entry_offset); /* no _start/main defined */

  free_assembled_program(prog);
}

void test_assemble_unresolved_symbol_fails(void) {
  AssembledProgram* prog = assemble(dup_str("_start:\n  beq x0, x0, undefined_label\n"));
  TEST_ASSERT_NULL(prog);
}

void test_assemble_duplicate_label_fails(void) {
  AssembledProgram* prog = assemble(dup_str("dup:\n  add x0, x0, x0\ndup:\n  add x0, x0, x0\n"));
  TEST_ASSERT_NULL(prog);
}

void test_assemble_empty_string_fails_gracefully(void) {
  AssembledProgram* prog = assemble(dup_str(""));
  if (prog)
    free_assembled_program(prog);
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_assemble_null_input_returns_null);
  RUN_TEST(test_assemble_simple_program_succeeds);
  RUN_TEST(test_assemble_pseudo_instruction_expands_end_to_end);
  RUN_TEST(test_assemble_data_only_program_succeeds);
  RUN_TEST(test_assemble_unresolved_symbol_fails);
  RUN_TEST(test_assemble_duplicate_label_fails);
  RUN_TEST(test_assemble_empty_string_fails_gracefully);

  return UNITY_END();
}