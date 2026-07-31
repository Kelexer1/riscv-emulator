#include "../include/riscv.h"
#include "unity/unity.h"

void setUp(void) {}
void tearDown(void) {}

/* ---------------------------------------------------------------------
 * parse_register
 * ------------------------------------------------------------------- */

void test_parse_register_numeric_form(void) {
  TEST_ASSERT_EQUAL_INT(0, parse_register("x0", 2));
  TEST_ASSERT_EQUAL_INT(31, parse_register("x31", 3));
}

void test_parse_register_alias_form(void) {
  TEST_ASSERT_EQUAL_INT(0, parse_register("zero", 4));
  TEST_ASSERT_EQUAL_INT(2, parse_register("sp", 2));
  TEST_ASSERT_EQUAL_INT(8, parse_register("fp", 2));
  TEST_ASSERT_EQUAL_INT(10, parse_register("a0", 2));
}

void test_parse_register_invalid_name(void) { TEST_ASSERT_EQUAL_INT(-1, parse_register("notareg", 7)); }

void test_parse_register_case_sensitive(void) {
  TEST_ASSERT_EQUAL_INT(-1, parse_register("SP", 2));
  TEST_ASSERT_EQUAL_INT(-1, parse_register("ZERO", 4));
}

void test_parse_register_len_shorter_than_name_fails(void) {
  /* "s" is a prefix of "sp" but not a full match. */
  TEST_ASSERT_EQUAL_INT(-1, parse_register("sp", 1));
}

void test_parse_register_len_stops_at_embedded_null(void) { TEST_ASSERT_EQUAL_INT(2, parse_register("sp\0extra", 8)); }

void test_parse_register_zero_len_fails(void) { TEST_ASSERT_EQUAL_INT(-1, parse_register("sp", 0)); }

void test_parse_register_x32_out_of_range_fails(void) { TEST_ASSERT_EQUAL_INT(-1, parse_register("x32", 3)); }

/* ---------------------------------------------------------------------
 * parse_mnemonic / mnemonic_to_str
 * ------------------------------------------------------------------- */

void test_parse_mnemonic_valid(void) {
  TEST_ASSERT_EQUAL(OP_ADD, parse_mnemonic("add", 3));
  TEST_ASSERT_EQUAL(OP_ECALL, parse_mnemonic("ecall", 5));
  TEST_ASSERT_EQUAL(OP_MULHSU, parse_mnemonic("mulhsu", 6));
}

void test_parse_mnemonic_invalid_returns_unknown(void) { TEST_ASSERT_EQUAL(OP_UNKNOWN, parse_mnemonic("notanop", 7)); }

void test_parse_mnemonic_case_sensitive(void) { TEST_ASSERT_EQUAL(OP_UNKNOWN, parse_mnemonic("ADD", 3)); }

void test_parse_mnemonic_prefix_of_longer_name(void) {
  /* len=3 on "addi" source should match "add" exactly, not "addi". */
  TEST_ASSERT_EQUAL(OP_ADD, parse_mnemonic("addi", 3));
}

void test_mnemonic_to_str_valid(void) {
  TEST_ASSERT_EQUAL_STRING("add", mnemonic_to_str(OP_ADD));
  TEST_ASSERT_EQUAL_STRING("ecall", mnemonic_to_str(OP_ECALL));
}

void test_mnemonic_to_str_unknown_returns_null(void) { TEST_ASSERT_NULL(mnemonic_to_str(OP_UNKNOWN)); }

/* ---------------------------------------------------------------------
 * parse_directive
 * ------------------------------------------------------------------- */

void test_parse_directive_valid(void) {
  TEST_ASSERT_EQUAL(DIRECTIVE_TEXT, parse_directive(".text", 5));
  TEST_ASSERT_EQUAL(DIRECTIVE_WORD, parse_directive(".word", 5));
}

void test_parse_directive_aliases_map_to_same_enum(void) {
  TEST_ASSERT_EQUAL(DIRECTIVE_STRING, parse_directive(".string", 7));
  TEST_ASSERT_EQUAL(DIRECTIVE_STRING, parse_directive(".asciz", 6));
  TEST_ASSERT_EQUAL(DIRECTIVE_ZERO, parse_directive(".zero", 5));
  TEST_ASSERT_EQUAL(DIRECTIVE_ZERO, parse_directive(".space", 6));
  TEST_ASSERT_EQUAL(DIRECTIVE_EQU, parse_directive(".equ", 4));
  TEST_ASSERT_EQUAL(DIRECTIVE_EQU, parse_directive(".set", 4));
  TEST_ASSERT_EQUAL(DIRECTIVE_ALIGN, parse_directive(".align", 6));
  TEST_ASSERT_EQUAL(DIRECTIVE_ALIGN, parse_directive(".balign", 7));
  TEST_ASSERT_EQUAL(DIRECTIVE_ALIGN, parse_directive(".p2align", 8));
}

void test_parse_directive_invalid_returns_unknown(void) {
  TEST_ASSERT_EQUAL(DIRECTIVE_UNKNOWN, parse_directive(".foo", 4));
}

void test_parse_directive_missing_dot_fails(void) { TEST_ASSERT_EQUAL(DIRECTIVE_UNKNOWN, parse_directive("text", 4)); }

/* ---------------------------------------------------------------------
 * parse_pseudo
 * ------------------------------------------------------------------- */

void test_parse_pseudo_valid(void) {
  TEST_ASSERT_EQUAL(PSEUDO_NOP, parse_pseudo("nop", 3));
  TEST_ASSERT_EQUAL(PSEUDO_MOV, parse_pseudo("mv", 2));
  TEST_ASSERT_EQUAL(PSEUDO_RET, parse_pseudo("ret", 3));
  TEST_ASSERT_EQUAL(PSEUDO_LLA, parse_pseudo("lla", 3));
}

void test_parse_pseudo_shares_name_with_real_opcode(void) {
  /* "jal" is both a real Opcode and a PseudoOpcode; each parser should
   * resolve it independently within its own table. */
  TEST_ASSERT_EQUAL(PSEUDO_JAL, parse_pseudo("jal", 3));
  TEST_ASSERT_EQUAL(OP_JAL, parse_mnemonic("jal", 3));
}

void test_parse_pseudo_invalid_returns_unknown(void) {
  TEST_ASSERT_EQUAL(PSEUDO_UNKNOWN, parse_pseudo("notapseudo", 10));
}

void test_parse_pseudo_case_sensitive(void) { TEST_ASSERT_EQUAL(PSEUDO_UNKNOWN, parse_pseudo("NOP", 3)); }

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_parse_register_numeric_form);
  RUN_TEST(test_parse_register_alias_form);
  RUN_TEST(test_parse_register_invalid_name);
  RUN_TEST(test_parse_register_case_sensitive);
  RUN_TEST(test_parse_register_len_shorter_than_name_fails);
  RUN_TEST(test_parse_register_len_stops_at_embedded_null);
  RUN_TEST(test_parse_register_zero_len_fails);
  RUN_TEST(test_parse_register_x32_out_of_range_fails);

  RUN_TEST(test_parse_mnemonic_valid);
  RUN_TEST(test_parse_mnemonic_invalid_returns_unknown);
  RUN_TEST(test_parse_mnemonic_case_sensitive);
  RUN_TEST(test_parse_mnemonic_prefix_of_longer_name);
  RUN_TEST(test_mnemonic_to_str_valid);
  RUN_TEST(test_mnemonic_to_str_unknown_returns_null);

  RUN_TEST(test_parse_directive_valid);
  RUN_TEST(test_parse_directive_aliases_map_to_same_enum);
  RUN_TEST(test_parse_directive_invalid_returns_unknown);
  RUN_TEST(test_parse_directive_missing_dot_fails);

  RUN_TEST(test_parse_pseudo_valid);
  RUN_TEST(test_parse_pseudo_shares_name_with_real_opcode);
  RUN_TEST(test_parse_pseudo_invalid_returns_unknown);
  RUN_TEST(test_parse_pseudo_case_sensitive);

  return UNITY_END();
}