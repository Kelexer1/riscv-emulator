#include "../include/api_disassembler.h"
#include "../include/binary_to_instruction.h"
#include "unity/unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static DecodedInstruction make_r(Opcode op, uint8_t rd, uint8_t rs1, uint8_t rs2) {
  DecodedInstruction ins = {0};
  ins.op = op;
  ins.rd = rd;
  ins.rs1 = rs1;
  ins.rs2 = rs2;
  return ins;
}

static DecodedInstruction make_i(Opcode op, uint8_t rd, uint8_t rs1, int32_t imm) {
  DecodedInstruction ins = {0};
  ins.op = op;
  ins.rd = rd;
  ins.rs1 = rs1;
  ins.imm = (uint32_t)imm;
  return ins;
}

/* ---------------------------------------------------------------------
 * disassemble_instruction
 * ------------------------------------------------------------------- */

void test_disassemble_null_ins_returns_null(void) { TEST_ASSERT_NULL(disassemble_instruction(0, NULL, NULL)); }

void test_disassemble_r_format(void) {
  DecodedInstruction ins = make_r(OP_ADD, 1, 2, 3);
  char* result = disassemble_instruction(0, &ins, NULL);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("add x1, x2, x3", result);
  free(result);
}

void test_disassemble_i_format_no_symbol(void) {
  DecodedInstruction ins = make_i(OP_ADDI, 5, 6, -10);
  char* result = disassemble_instruction(0, &ins, NULL);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("addi x5, x6, -10", result);
  free(result);
}

void test_disassemble_i_format_with_matching_constant_symbol(void) {
  Symbol sym = {0};
  sym.type = SYMBOL_CONSTANT;
  sym.name = "SIZE";
  sym.len = 4;
  sym.value = 10;
  SymbolTable table = {0};
  table.head = &sym;

  DecodedInstruction ins = make_i(OP_ADDI, 5, 6, 10);
  char* result = disassemble_instruction(0, &ins, &table);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("addi x5, x6, SIZE", result);
  free(result);
}

void test_disassemble_i_shift_format(void) {
  DecodedInstruction ins = make_i(OP_SLLI, 1, 2, 4);
  char* result = disassemble_instruction(0, &ins, NULL);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("slli x1, x2, 4", result);
  free(result);
}

void test_disassemble_i_load_format_no_symbol(void) {
  DecodedInstruction ins = make_i(OP_LW, 5, 2, 16);
  char* result = disassemble_instruction(0, &ins, NULL);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("lw x5, 16(x2)", result);
  free(result);
}

void test_disassemble_s_format_no_symbol(void) {
  DecodedInstruction ins = make_r(OP_SW, 0, 2, 5);
  ins.imm = 100;
  char* result = disassemble_instruction(0, &ins, NULL);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("sw x5, 100(x2)", result);
  free(result);
}

void test_disassemble_s_format_with_symbol(void) {
  Symbol sym = {0};
  sym.type = SYMBOL_CONSTANT;
  sym.name = "OFF";
  sym.len = 3;
  sym.value = 100;
  SymbolTable table = {0};
  table.head = &sym;

  DecodedInstruction ins = make_r(OP_SW, 0, 2, 5);
  ins.imm = 100;
  char* result = disassemble_instruction(0, &ins, &table);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("sw x5, OFF(x2)", result);
  free(result);
}

void test_disassemble_b_format_no_symbol(void) {
  DecodedInstruction ins = make_r(OP_BEQ, 0, 1, 2);
  ins.imm = 16;
  char* result = disassemble_instruction(100, &ins, NULL);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("beq x1, x2, 0x74", result); /* 100 + 16 = 116 = 0x74 */
  free(result);
}

void test_disassemble_b_format_with_label_symbol(void) {
  Symbol sym = {0};
  sym.type = SYMBOL_LABEL;
  sym.name = "loop";
  sym.len = 4;
  sym.value = 116;
  SymbolTable table = {0};
  table.head = &sym;

  DecodedInstruction ins = make_r(OP_BEQ, 0, 1, 2);
  ins.imm = 16;
  char* result = disassemble_instruction(100, &ins, &table);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("beq x1, x2, loop", result);
  free(result);
}

void test_disassemble_u_format_no_symbol(void) {
  DecodedInstruction ins = {0};
  ins.op = OP_LUI;
  ins.rd = 5;
  ins.imm = 0x12345000;
  char* result = disassemble_instruction(0, &ins, NULL);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("lui x5, 0x12345000", result);
  free(result);
}

void test_disassemble_j_format_with_label_symbol(void) {
  Symbol sym = {0};
  sym.type = SYMBOL_LABEL;
  sym.name = "func";
  sym.len = 4;
  sym.value = 1000;
  SymbolTable table = {0};
  table.head = &sym;

  DecodedInstruction ins = {0};
  ins.op = OP_JAL;
  ins.rd = 1;
  ins.imm = 1000;
  char* result = disassemble_instruction(0, &ins, &table);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("jal x1, func", result);
  free(result);
}

void test_disassemble_system_format(void) {
  DecodedInstruction ecall = {.op = OP_ECALL};
  DecodedInstruction ebreak = {.op = OP_EBREAK};

  char* r1 = disassemble_instruction(0, &ecall, NULL);
  char* r2 = disassemble_instruction(0, &ebreak, NULL);

  TEST_ASSERT_EQUAL_STRING("ecall", r1);
  TEST_ASSERT_EQUAL_STRING("ebreak", r2);
  free(r1);
  free(r2);
}

void test_disassemble_unknown_op_returns_null(void) {
  DecodedInstruction ins = {.op = OP_UNKNOWN};
  TEST_ASSERT_NULL(disassemble_instruction(0, &ins, NULL));
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_disassemble_null_ins_returns_null);
  RUN_TEST(test_disassemble_r_format);
  RUN_TEST(test_disassemble_i_format_no_symbol);
  RUN_TEST(test_disassemble_i_format_with_matching_constant_symbol);
  RUN_TEST(test_disassemble_i_shift_format);
  RUN_TEST(test_disassemble_i_load_format_no_symbol);
  RUN_TEST(test_disassemble_s_format_no_symbol);
  RUN_TEST(test_disassemble_s_format_with_symbol);
  RUN_TEST(test_disassemble_b_format_no_symbol);
  RUN_TEST(test_disassemble_b_format_with_label_symbol);
  RUN_TEST(test_disassemble_u_format_no_symbol);
  RUN_TEST(test_disassemble_j_format_with_label_symbol);
  RUN_TEST(test_disassemble_system_format);
  RUN_TEST(test_disassemble_unknown_op_returns_null);

  return UNITY_END();
}