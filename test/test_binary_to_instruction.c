#include "../include/binary_to_instruction.h"
#include "../include/instruction_to_binary.h"
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

static Operand zero_operand(void) {
  Operand op = {0};
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

/* ---------------------------------------------------------------------
 * binary_to_instruction
 * ------------------------------------------------------------------- */

void test_binary_to_instruction_null_out_fails(void) { TEST_ASSERT_EQUAL_INT(0, binary_to_instruction(0x33, NULL)); }

void test_binary_to_instruction_add_roundtrip(void) {
  ParsedLine line = make_line(OP_ADD, 3, make_reg(1), make_reg(2), make_reg(3));
  uint32_t bin;
  instruction_to_binary(&line, 0, NULL, &bin);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_ADD, out.op);
  TEST_ASSERT_EQUAL_UINT8(1, out.rd);
  TEST_ASSERT_EQUAL_UINT8(2, out.rs1);
  TEST_ASSERT_EQUAL_UINT8(3, out.rs2);
}

void test_binary_to_instruction_sub_distinguished_by_funct7(void) {
  ParsedLine line = make_line(OP_SUB, 3, make_reg(1), make_reg(2), make_reg(3));
  uint32_t bin;
  instruction_to_binary(&line, 0, NULL, &bin);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_SUB, out.op);
}

void test_binary_to_instruction_addi_positive_imm(void) {
  ParsedLine line = make_line(OP_ADDI, 3, make_reg(5), make_reg(6), make_imm(100));
  uint32_t bin;
  instruction_to_binary(&line, 0, NULL, &bin);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_ADDI, out.op);
  TEST_ASSERT_EQUAL_UINT8(5, out.rd);
  TEST_ASSERT_EQUAL_UINT8(6, out.rs1);
  TEST_ASSERT_EQUAL_INT32(100, (int32_t)out.imm);
}

void test_binary_to_instruction_addi_negative_imm_sign_extends(void) {
  ParsedLine line = make_line(OP_ADDI, 3, make_reg(1), make_reg(2), make_imm(-50));
  uint32_t bin;
  instruction_to_binary(&line, 0, NULL, &bin);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL_INT32(-50, (int32_t)out.imm);
}

void test_binary_to_instruction_srli_vs_srai(void) {
  ParsedLine srli = make_line(OP_SRLI, 3, make_reg(1), make_reg(2), make_imm(4));
  ParsedLine srai = make_line(OP_SRAI, 3, make_reg(1), make_reg(2), make_imm(4));
  uint32_t bin_srli, bin_srai;
  instruction_to_binary(&srli, 0, NULL, &bin_srli);
  instruction_to_binary(&srai, 0, NULL, &bin_srai);

  DecodedInstruction out_srli, out_srai;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin_srli, &out_srli));
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin_srai, &out_srai));
  TEST_ASSERT_EQUAL(OP_SRLI, out_srli.op);
  TEST_ASSERT_EQUAL(OP_SRAI, out_srai.op);
}

void test_binary_to_instruction_lw_roundtrip(void) {
  ParsedLine line = make_line(OP_LW, 2, make_reg(5), make_mem(16, 2), zero_operand());
  uint32_t bin;
  instruction_to_binary(&line, 0, NULL, &bin);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_LW, out.op);
  TEST_ASSERT_EQUAL_UINT8(5, out.rd);
  TEST_ASSERT_EQUAL_UINT8(2, out.rs1);
  TEST_ASSERT_EQUAL_INT32(16, (int32_t)out.imm);
}

void test_binary_to_instruction_sw_roundtrip(void) {
  ParsedLine line = make_line(OP_SW, 2, make_reg(5), make_mem(100, 2), zero_operand());
  uint32_t bin;
  instruction_to_binary(&line, 0, NULL, &bin);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_SW, out.op);
  TEST_ASSERT_EQUAL_UINT8(5, out.rs2);
  TEST_ASSERT_EQUAL_UINT8(2, out.rs1);
  TEST_ASSERT_EQUAL_INT32(100, (int32_t)out.imm);
}

void test_binary_to_instruction_beq_roundtrip(void) {
  Symbol sym = {0};
  sym.name = "target";
  sym.len = 6;
  sym.value = 100;
  SymbolTable table = {0};
  table.head = &sym;

  ParsedLine line = make_line(OP_BEQ, 3, make_reg(1), make_reg(2), make_symbol("target"));
  uint32_t bin;
  TEST_ASSERT_EQUAL_INT(1, instruction_to_binary(&line, 0, &table, &bin));

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_BEQ, out.op);
  TEST_ASSERT_EQUAL_UINT8(1, out.rs1);
  TEST_ASSERT_EQUAL_UINT8(2, out.rs2);
  TEST_ASSERT_EQUAL_INT32(100, (int32_t)out.imm);
}

void test_binary_to_instruction_lui_roundtrip(void) {
  ParsedLine line = make_line(OP_LUI, 2, make_reg(5), make_imm(0x12345), zero_operand());
  uint32_t bin;
  instruction_to_binary(&line, 0, NULL, &bin);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_LUI, out.op);
  TEST_ASSERT_EQUAL_UINT8(5, out.rd);
  TEST_ASSERT_EQUAL_UINT32(0x12345000, out.imm); /* decode_u yields the shifted value */
}

void test_binary_to_instruction_jal_roundtrip(void) {
  Symbol sym = {0};
  sym.name = "func";
  sym.len = 4;
  sym.value = 1000;
  SymbolTable table = {0};
  table.head = &sym;

  ParsedLine line = make_line(OP_JAL, 2, make_reg(1), make_symbol("func"), zero_operand());
  uint32_t bin;
  TEST_ASSERT_EQUAL_INT(1, instruction_to_binary(&line, 0, &table, &bin));

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_JAL, out.op);
  TEST_ASSERT_EQUAL_UINT8(1, out.rd);
  TEST_ASSERT_EQUAL_INT32(1000, (int32_t)out.imm);
}

void test_binary_to_instruction_ecall_ebreak(void) {
  ParsedLine ecall_line = make_line(OP_ECALL, 0, zero_operand(), zero_operand(), zero_operand());
  ParsedLine ebreak_line = make_line(OP_EBREAK, 0, zero_operand(), zero_operand(), zero_operand());
  uint32_t bin_ecall, bin_ebreak;
  instruction_to_binary(&ecall_line, 0, NULL, &bin_ecall);
  instruction_to_binary(&ebreak_line, 0, NULL, &bin_ebreak);

  DecodedInstruction out_ecall, out_ebreak;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin_ecall, &out_ecall));
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin_ebreak, &out_ebreak));
  TEST_ASSERT_EQUAL(OP_ECALL, out_ecall.op);
  TEST_ASSERT_EQUAL(OP_EBREAK, out_ebreak.op);
}

void test_binary_to_instruction_reserved_opcode_fails(void) {
  uint32_t bin = 0x7F; /* opcode bits = 1111111, unused by this ISA subset */
  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(0, binary_to_instruction(bin, &out));
}

void test_binary_to_instruction_invalid_funct7_for_add_fails(void) {
  /* opcode=0x33 (R-type), funct3=0 (ADD/SUB family), funct7=0x10 --
   * neither ADD's (0x00) nor SUB's (0x20) funct7, so no table entry
   * should match. */
  uint32_t bin = (0x10u << 25) | (0u << 20) | (0u << 15) | (0u << 12) | (0u << 7) | 0x33u;
  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(0, binary_to_instruction(bin, &out));
}

void test_binary_to_instruction_all_zero_word_should_be_rejected(void) {
  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(0, binary_to_instruction(0x00000000, &out));
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  init_decode_table();

  UNITY_BEGIN();

  RUN_TEST(test_binary_to_instruction_null_out_fails);
  RUN_TEST(test_binary_to_instruction_add_roundtrip);
  RUN_TEST(test_binary_to_instruction_sub_distinguished_by_funct7);
  RUN_TEST(test_binary_to_instruction_addi_positive_imm);
  RUN_TEST(test_binary_to_instruction_addi_negative_imm_sign_extends);
  RUN_TEST(test_binary_to_instruction_srli_vs_srai);
  RUN_TEST(test_binary_to_instruction_lw_roundtrip);
  RUN_TEST(test_binary_to_instruction_sw_roundtrip);
  RUN_TEST(test_binary_to_instruction_beq_roundtrip);
  RUN_TEST(test_binary_to_instruction_lui_roundtrip);
  RUN_TEST(test_binary_to_instruction_jal_roundtrip);
  RUN_TEST(test_binary_to_instruction_ecall_ebreak);
  RUN_TEST(test_binary_to_instruction_reserved_opcode_fails);
  RUN_TEST(test_binary_to_instruction_invalid_funct7_for_add_fails);
  RUN_TEST(test_binary_to_instruction_all_zero_word_should_be_rejected);

  return UNITY_END();
}