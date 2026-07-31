#define _POSIX_C_SOURCE 200809L

#include "../src/instruction_to_binary.c"
#include "unity/unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static Operand make_reg(int reg) {
  Operand op = {0};
  op.type = OPERAND_REGISTER;
  op.reg.reg = reg;
  return op;
}

static Operand make_imm(int imm) {
  Operand op = {0};
  op.type = OPERAND_IMMEDIATE_LITERAL;
  op.imm.imm = imm;
  return op;
}

static Operand make_symbol(const char* s) {
  Operand op = {0};
  op.type = OPERAND_SYMBOL;
  op.label.label = (char*)s;
  op.label.len = strlen(s);
  return op;
}

static Operand make_mem(int offset, int base_reg) {
  Operand op = {0};
  op.type = OPERAND_MEMORY;
  op.mem.offset = offset;
  op.mem.base_reg = base_reg;
  return op;
}

static ParsedLine make_line(Opcode op, int operand_count, Operand a, Operand b, Operand c) {
  ParsedLine line = {0};
  line.type = LINE_INSTRUCTION;
  line.instruction.type = INSTRUCTION_REAL;
  line.instruction.op = op;
  line.instruction.operand_count = operand_count;
  if (operand_count > 0)
    line.instruction.operands[0] = a;
  if (operand_count > 1)
    line.instruction.operands[1] = b;
  if (operand_count > 2)
    line.instruction.operands[2] = c;
  return line;
}

static Operand zero_operand(void) {
  Operand op = {0};
  return op;
}

/* ---------------------------------------------------------------------
 * get_format
 * ------------------------------------------------------------------- */

void test_get_format_samples(void) {
  TEST_ASSERT_EQUAL(FORMAT_R, get_format(OP_ADD));
  TEST_ASSERT_EQUAL(FORMAT_R, get_format(OP_MUL));
  TEST_ASSERT_EQUAL(FORMAT_I, get_format(OP_ADDI));
  TEST_ASSERT_EQUAL(FORMAT_I_SHIFT, get_format(OP_SLLI));
  TEST_ASSERT_EQUAL(FORMAT_I_LOAD, get_format(OP_LW));
  TEST_ASSERT_EQUAL(FORMAT_I_JALR, get_format(OP_JALR));
  TEST_ASSERT_EQUAL(FORMAT_S, get_format(OP_SW));
  TEST_ASSERT_EQUAL(FORMAT_B, get_format(OP_BEQ));
  TEST_ASSERT_EQUAL(FORMAT_U, get_format(OP_LUI));
  TEST_ASSERT_EQUAL(FORMAT_J, get_format(OP_JAL));
  TEST_ASSERT_EQUAL(FORMAT_SYSTEM, get_format(OP_ECALL));
}

/* ---------------------------------------------------------------------
 * encode_r
 * ------------------------------------------------------------------- */

void test_encode_r_add_fields(void) {
  ParsedLine line = make_line(OP_ADD, 3, make_reg(1), make_reg(2), make_reg(3));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, encode_r(&line, &out));
  TEST_ASSERT_EQUAL_UINT32(0x33, out & 0x7F);
  TEST_ASSERT_EQUAL_UINT32(1, (out >> 7) & 0x1F);
  TEST_ASSERT_EQUAL_UINT32(0x0, (out >> 12) & 0x7);
  TEST_ASSERT_EQUAL_UINT32(2, (out >> 15) & 0x1F);
  TEST_ASSERT_EQUAL_UINT32(3, (out >> 20) & 0x1F);
  TEST_ASSERT_EQUAL_UINT32(0x00, (out >> 25) & 0x7F);
}

void test_encode_r_sub_funct7(void) {
  ParsedLine line = make_line(OP_SUB, 3, make_reg(1), make_reg(2), make_reg(3));
  uint32_t out = 0;

  encode_r(&line, &out);

  TEST_ASSERT_EQUAL_UINT32(0x20, (out >> 25) & 0x7F);
}

void test_encode_r_mul_funct7(void) {
  ParsedLine line = make_line(OP_MUL, 3, make_reg(1), make_reg(2), make_reg(3));
  uint32_t out = 0;

  encode_r(&line, &out);

  TEST_ASSERT_EQUAL_UINT32(0x01, (out >> 25) & 0x7F);
  TEST_ASSERT_EQUAL_UINT32(0x33, out & 0x7F);
}

/* ---------------------------------------------------------------------
 * encode_i
 * ------------------------------------------------------------------- */

void test_encode_i_literal_valid(void) {
  ParsedLine line = make_line(OP_ADDI, 3, make_reg(5), make_reg(6), make_imm(50));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, encode_i(&line, 0, NULL, &out));
  TEST_ASSERT_EQUAL_UINT32(0x13, out & 0x7F);
  TEST_ASSERT_EQUAL_UINT32(5, (out >> 7) & 0x1F);
  TEST_ASSERT_EQUAL_UINT32(6, (out >> 15) & 0x1F);
  TEST_ASSERT_EQUAL_UINT32(50, (out >> 20) & 0xFFF);
}

void test_encode_i_literal_out_of_range_fails(void) {
  ParsedLine line = make_line(OP_ADDI, 3, make_reg(1), make_reg(2), make_imm(2048));
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_i(&line, 0, NULL, &out));
}

void test_encode_i_literal_boundary_values_valid(void) {
  ParsedLine low = make_line(OP_ADDI, 3, make_reg(1), make_reg(2), make_imm(-2048));
  ParsedLine high = make_line(OP_ADDI, 3, make_reg(1), make_reg(2), make_imm(2047));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, encode_i(&low, 0, NULL, &out));
  TEST_ASSERT_EQUAL_INT(1, encode_i(&high, 0, NULL, &out));
}

void test_encode_i_symbol_resolves_and_splits(void) {
  Symbol sym = {0};
  sym.name = "target";
  sym.len = 6;
  sym.value = 8192;
  SymbolTable table = {0};
  table.head = &sym;

  /* ADDI paired 4 bytes after an AUIPC at current_addr-4. */
  uint32_t current_addr = 4;
  ParsedLine line = make_line(OP_ADDI, 3, make_reg(1), make_reg(1), make_symbol("target"));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, encode_i(&line, current_addr, &table, &out));

  int32_t auipc_addr = (int32_t)current_addr - 4;
  int32_t offset = (int32_t)sym.value - auipc_addr;
  int32_t hi20 = (offset + 0x800) >> 12;
  int32_t expected_lo = offset - (hi20 << 12);
  int32_t encoded_lo_bits = (int32_t)((out >> 20) & 0xFFF);
  /* sign-extend the 12-bit field for comparison */
  if (encoded_lo_bits & 0x800)
    encoded_lo_bits -= 0x1000;
  TEST_ASSERT_EQUAL_INT(expected_lo, encoded_lo_bits);
}

void test_encode_i_symbol_not_found_fails(void) {
  SymbolTable table = {0};
  table.head = NULL;
  ParsedLine line = make_line(OP_ADDI, 3, make_reg(1), make_reg(1), make_symbol("missing"));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(0, encode_i(&line, 4, &table, &out));
}

/* ---------------------------------------------------------------------
 * encode_i_shift
 * ------------------------------------------------------------------- */

void test_encode_i_shift_valid_boundaries(void) {
  ParsedLine low = make_line(OP_SLLI, 3, make_reg(1), make_reg(2), make_imm(0));
  ParsedLine high = make_line(OP_SLLI, 3, make_reg(1), make_reg(2), make_imm(31));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, encode_i_shift(&low, &out));
  TEST_ASSERT_EQUAL_INT(1, encode_i_shift(&high, &out));
  TEST_ASSERT_EQUAL_UINT32(31, (out >> 20) & 0x1F);
}

void test_encode_i_shift_out_of_range_fails(void) {
  ParsedLine neg = make_line(OP_SLLI, 3, make_reg(1), make_reg(2), make_imm(-1));
  ParsedLine over = make_line(OP_SLLI, 3, make_reg(1), make_reg(2), make_imm(32));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(0, encode_i_shift(&neg, &out));
  TEST_ASSERT_EQUAL_INT(0, encode_i_shift(&over, &out));
}

/* ---------------------------------------------------------------------
 * encode_i_load
 * ------------------------------------------------------------------- */

void test_encode_i_load_valid(void) {
  ParsedLine line = make_line(OP_LW, 2, make_reg(5), make_mem(16, 2), zero_operand());
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, encode_i_load(&line, &out));
  TEST_ASSERT_EQUAL_UINT32(5, (out >> 7) & 0x1F);
  TEST_ASSERT_EQUAL_UINT32(2, (out >> 15) & 0x1F);
  TEST_ASSERT_EQUAL_UINT32(16, (out >> 20) & 0xFFF);
}

void test_encode_i_load_out_of_range_fails(void) {
  ParsedLine line = make_line(OP_LW, 2, make_reg(5), make_mem(4096, 2), zero_operand());
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_i_load(&line, &out));
}

/* ---------------------------------------------------------------------
 * encode_s
 * ------------------------------------------------------------------- */

void test_encode_s_valid_bit_split(void) {
  ParsedLine line = make_line(OP_SW, 2, make_reg(5), make_mem(100, 2), zero_operand());
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, encode_s(&line, &out));
  uint32_t imm_4_0 = (out >> 7) & 0x1F;
  uint32_t imm_11_5 = (out >> 25) & 0x7F;
  uint32_t reconstructed = (imm_11_5 << 5) | imm_4_0;
  TEST_ASSERT_EQUAL_UINT32(100, reconstructed);
  TEST_ASSERT_EQUAL_UINT32(5, (out >> 20) & 0x1F); /* rs2 */
  TEST_ASSERT_EQUAL_UINT32(2, (out >> 15) & 0x1F); /* rs1/base */
}

void test_encode_s_out_of_range_fails(void) {
  ParsedLine line = make_line(OP_SW, 2, make_reg(5), make_mem(-2049, 2), zero_operand());
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_s(&line, &out));
}

/* ---------------------------------------------------------------------
 * encode_b
 * ------------------------------------------------------------------- */

void test_encode_b_valid_bit_split(void) {
  ParsedLine line = make_line(OP_BEQ, 2, make_reg(1), make_reg(2), zero_operand());
  uint32_t out = 0;

  /* target 100 bytes ahead of current_addr=0 */
  TEST_ASSERT_EQUAL_INT(1, encode_b(&line, 100, 0, &out));

  uint32_t imm_11 = (out >> 7) & 0x1;
  uint32_t imm_4_1 = (out >> 8) & 0xF;
  uint32_t imm_10_5 = (out >> 25) & 0x3F;
  uint32_t imm_12 = (out >> 31) & 0x1;
  uint32_t reconstructed = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
  TEST_ASSERT_EQUAL_UINT32(100, reconstructed);
  TEST_ASSERT_EQUAL_UINT32(1, (out >> 15) & 0x1F);
  TEST_ASSERT_EQUAL_UINT32(2, (out >> 20) & 0x1F);
}

void test_encode_b_out_of_range_fails(void) {
  ParsedLine line = make_line(OP_BEQ, 2, make_reg(1), make_reg(2), zero_operand());
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_b(&line, 100000, 0, &out));
}

void test_encode_b_unaligned_fails(void) {
  ParsedLine line = make_line(OP_BEQ, 2, make_reg(1), make_reg(2), zero_operand());
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_b(&line, 101, 0, &out));
}

/* ---------------------------------------------------------------------
 * encode_u
 * ------------------------------------------------------------------- */

void test_encode_u_literal_valid(void) {
  ParsedLine line = make_line(OP_LUI, 2, make_reg(5), make_imm(0x12345), zero_operand());
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, encode_u(&line, 0, NULL, &out));
  TEST_ASSERT_EQUAL_UINT32(0x12345, (out >> 12) & 0xFFFFF);
  TEST_ASSERT_EQUAL_UINT32(5, (out >> 7) & 0x1F);
}

void test_encode_u_literal_negative_fails(void) {
  ParsedLine line = make_line(OP_LUI, 2, make_reg(5), make_imm(-1), zero_operand());
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_u(&line, 0, NULL, &out));
}

void test_encode_u_literal_too_large_fails(void) {
  ParsedLine line = make_line(OP_LUI, 2, make_reg(5), make_imm(0x100000), zero_operand());
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_u(&line, 0, NULL, &out));
}

void test_encode_u_symbol_not_found_fails(void) {
  SymbolTable table = {0};
  table.head = NULL;
  ParsedLine line = make_line(OP_AUIPC, 2, make_reg(5), make_symbol("missing"), zero_operand());
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_u(&line, 0, &table, &out));
}

/* ---------------------------------------------------------------------
 * encode_j
 * ------------------------------------------------------------------- */

void test_encode_j_valid_bit_split(void) {
  ParsedLine line = make_line(OP_JAL, 1, make_reg(1), zero_operand(), zero_operand());
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, encode_j(&line, 1000, 0, &out));

  uint32_t imm_11 = (out >> 20) & 0x1;
  uint32_t imm_19_12 = (out >> 12) & 0xFF;
  uint32_t imm_10_1 = (out >> 21) & 0x3FF;
  uint32_t imm_20 = (out >> 31) & 0x1;
  uint32_t reconstructed = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
  TEST_ASSERT_EQUAL_UINT32(1000, reconstructed);
  TEST_ASSERT_EQUAL_UINT32(1, (out >> 7) & 0x1F);
}

void test_encode_j_out_of_range_fails(void) {
  ParsedLine line = make_line(OP_JAL, 1, make_reg(1), zero_operand(), zero_operand());
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_j(&line, 2000000, 0, &out));
}

void test_encode_j_unaligned_fails(void) {
  ParsedLine line = make_line(OP_JAL, 1, make_reg(1), zero_operand(), zero_operand());
  uint32_t out = 0;
  TEST_ASSERT_EQUAL_INT(0, encode_j(&line, 101, 0, &out));
}

/* ---------------------------------------------------------------------
 * encode_system
 * ------------------------------------------------------------------- */

void test_encode_system_ecall(void) {
  ParsedLine line = make_line(OP_ECALL, 0, zero_operand(), zero_operand(), zero_operand());
  uint32_t out = 0;

  encode_system(&line, &out);

  TEST_ASSERT_EQUAL_UINT32(0, (out >> 20) & 0xFFF);
  TEST_ASSERT_EQUAL_UINT32(0x73, out & 0x7F);
}

void test_encode_system_ebreak(void) {
  ParsedLine line = make_line(OP_EBREAK, 0, zero_operand(), zero_operand(), zero_operand());
  uint32_t out = 0;

  encode_system(&line, &out);

  TEST_ASSERT_EQUAL_UINT32(0x001, (out >> 20) & 0xFFF);
}

/* ---------------------------------------------------------------------
 * instruction_to_binary (dispatch)
 * ------------------------------------------------------------------- */

void test_instruction_to_binary_null_out_fails(void) {
  ParsedLine line = make_line(OP_ADD, 3, make_reg(1), make_reg(2), make_reg(3));
  TEST_ASSERT_EQUAL_INT(0, instruction_to_binary(&line, 0, NULL, NULL));
}

void test_instruction_to_binary_r_format(void) {
  ParsedLine line = make_line(OP_ADD, 3, make_reg(1), make_reg(2), make_reg(3));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, instruction_to_binary(&line, 0, NULL, &out));
  TEST_ASSERT_EQUAL_UINT32(0x33, out & 0x7F);
}

void test_instruction_to_binary_b_format_resolves_symbol(void) {
  Symbol sym = {0};
  sym.name = "loop";
  sym.len = 4;
  sym.value = 100;
  SymbolTable table = {0};
  table.head = &sym;

  ParsedLine line = make_line(OP_BEQ, 3, make_reg(1), make_reg(2), make_symbol("loop"));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, instruction_to_binary(&line, 0, &table, &out));

  uint32_t imm_11 = (out >> 7) & 0x1;
  uint32_t imm_4_1 = (out >> 8) & 0xF;
  uint32_t imm_10_5 = (out >> 25) & 0x3F;
  uint32_t imm_12 = (out >> 31) & 0x1;
  uint32_t reconstructed = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
  TEST_ASSERT_EQUAL_UINT32(100, reconstructed);
}

void test_instruction_to_binary_b_format_missing_symbol_fails(void) {
  SymbolTable table = {0};
  table.head = NULL;
  ParsedLine line = make_line(OP_BEQ, 3, make_reg(1), make_reg(2), make_symbol("missing"));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(0, instruction_to_binary(&line, 0, &table, &out));
}

void test_instruction_to_binary_j_format_resolves_symbol(void) {
  Symbol sym = {0};
  sym.name = "func";
  sym.len = 4;
  sym.value = 2000;
  SymbolTable table = {0};
  table.head = &sym;

  ParsedLine line = make_line(OP_JAL, 2, make_reg(1), make_symbol("func"), zero_operand());
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, instruction_to_binary(&line, 0, &table, &out));
}

void test_instruction_to_binary_b_format_literal_target(void) {
  ParsedLine line = make_line(OP_BEQ, 3, make_reg(1), make_reg(2), make_imm(100));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, instruction_to_binary(&line, 0, NULL, &out));

  uint32_t imm_11 = (out >> 7) & 0x1;
  uint32_t imm_4_1 = (out >> 8) & 0xF;
  uint32_t imm_10_5 = (out >> 25) & 0x3F;
  uint32_t imm_12 = (out >> 31) & 0x1;
  uint32_t reconstructed = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
  TEST_ASSERT_EQUAL_UINT32(100, reconstructed);
}

void test_instruction_to_binary_j_format_literal_target(void) {
  ParsedLine line = make_line(OP_JAL, 2, make_reg(1), make_imm(1000), zero_operand());
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, instruction_to_binary(&line, 0, NULL, &out));

  uint32_t imm_11 = (out >> 20) & 0x1;
  uint32_t imm_19_12 = (out >> 12) & 0xFF;
  uint32_t imm_10_1 = (out >> 21) & 0x3FF;
  uint32_t imm_20 = (out >> 31) & 0x1;
  uint32_t reconstructed = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
  TEST_ASSERT_EQUAL_UINT32(1000, reconstructed);
}

void test_instruction_to_binary_system_format(void) {
  ParsedLine line = make_line(OP_ECALL, 0, zero_operand(), zero_operand(), zero_operand());
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(1, instruction_to_binary(&line, 0, NULL, &out));
}

void test_instruction_to_binary_unknown_opcode_should_be_rejected(void) {
  ParsedLine line = make_line(OP_UNKNOWN, 3, make_reg(0), make_reg(0), make_reg(0));
  uint32_t out = 0;

  TEST_ASSERT_EQUAL_INT(0, instruction_to_binary(&line, 0, NULL, &out));
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_get_format_samples);

  RUN_TEST(test_encode_r_add_fields);
  RUN_TEST(test_encode_r_sub_funct7);
  RUN_TEST(test_encode_r_mul_funct7);

  RUN_TEST(test_encode_i_literal_valid);
  RUN_TEST(test_encode_i_literal_out_of_range_fails);
  RUN_TEST(test_encode_i_literal_boundary_values_valid);
  RUN_TEST(test_encode_i_symbol_resolves_and_splits);
  RUN_TEST(test_encode_i_symbol_not_found_fails);

  RUN_TEST(test_encode_i_shift_valid_boundaries);
  RUN_TEST(test_encode_i_shift_out_of_range_fails);

  RUN_TEST(test_encode_i_load_valid);
  RUN_TEST(test_encode_i_load_out_of_range_fails);

  RUN_TEST(test_encode_s_valid_bit_split);
  RUN_TEST(test_encode_s_out_of_range_fails);

  RUN_TEST(test_encode_b_valid_bit_split);
  RUN_TEST(test_encode_b_out_of_range_fails);
  RUN_TEST(test_encode_b_unaligned_fails);

  RUN_TEST(test_encode_u_literal_valid);
  RUN_TEST(test_encode_u_literal_negative_fails);
  RUN_TEST(test_encode_u_literal_too_large_fails);
  RUN_TEST(test_encode_u_symbol_not_found_fails);

  RUN_TEST(test_encode_j_valid_bit_split);
  RUN_TEST(test_encode_j_out_of_range_fails);
  RUN_TEST(test_encode_j_unaligned_fails);

  RUN_TEST(test_encode_system_ecall);
  RUN_TEST(test_encode_system_ebreak);

  RUN_TEST(test_instruction_to_binary_null_out_fails);
  RUN_TEST(test_instruction_to_binary_r_format);
  RUN_TEST(test_instruction_to_binary_b_format_resolves_symbol);
  RUN_TEST(test_instruction_to_binary_b_format_missing_symbol_fails);
  RUN_TEST(test_instruction_to_binary_j_format_resolves_symbol);
  RUN_TEST(test_instruction_to_binary_b_format_literal_target);
  RUN_TEST(test_instruction_to_binary_j_format_literal_target);
  RUN_TEST(test_instruction_to_binary_system_format);
  RUN_TEST(test_instruction_to_binary_unknown_opcode_should_be_rejected);

  return UNITY_END();
}