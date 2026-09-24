#include "../include/binary_to_instruction.h"
#include "unity/unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ---------------------------------------------------------------------
 * Raw RISC-V encoders, standing in for the assembler's instruction_to_binary
 * (which now lives in the assembler repo and is no longer available here).
 * Encodings follow the standard RV32I bit layouts.
 * ------------------------------------------------------------------- */

static uint32_t encode_r(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encode_i(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  return ((uint32_t)(imm & 0xFFF) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encode_s(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
  uint32_t u = (uint32_t)imm;
  return (((u >> 5) & 0x7F) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | ((u & 0x1F) << 7) | opcode;
}

static uint32_t encode_b(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
  uint32_t u = (uint32_t)imm;
  return (((u >> 12) & 0x1) << 31) | (((u >> 5) & 0x3F) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) |
         (((u >> 1) & 0xF) << 8) | (((u >> 11) & 0x1) << 7) | opcode;
}

static uint32_t encode_u(uint32_t imm20, uint32_t rd, uint32_t opcode) { return (imm20 << 12) | (rd << 7) | opcode; }

static uint32_t encode_j(int32_t imm, uint32_t rd, uint32_t opcode) {
  uint32_t u = (uint32_t)imm;
  return (((u >> 20) & 0x1) << 31) | (((u >> 1) & 0x3FF) << 21) | (((u >> 11) & 0x1) << 20) |
         (((u >> 12) & 0xFF) << 12) | (rd << 7) | opcode;
}

#define OPCODE_R 0x33u
#define OPCODE_I_ARITH 0x13u
#define OPCODE_LOAD 0x03u
#define OPCODE_STORE 0x23u
#define OPCODE_BRANCH 0x63u
#define OPCODE_LUI 0x37u
#define OPCODE_JAL 0x6Fu
#define OPCODE_SYSTEM 0x73u

#define FUNCT7_ADD 0x00u
#define FUNCT7_SUB 0x20u
#define FUNCT7_SRL 0x00u
#define FUNCT7_SRA 0x20u

/* ---------------------------------------------------------------------
 * binary_to_instruction
 * ------------------------------------------------------------------- */

void test_binary_to_instruction_null_out_fails(void) { TEST_ASSERT_EQUAL_INT(0, binary_to_instruction(0x33, NULL)); }

void test_binary_to_instruction_add_roundtrip(void) {
  uint32_t bin = encode_r(FUNCT7_ADD, 3, 2, 0x0, 1, OPCODE_R);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_ADD, out.op);
  TEST_ASSERT_EQUAL_UINT8(1, out.rd);
  TEST_ASSERT_EQUAL_UINT8(2, out.rs1);
  TEST_ASSERT_EQUAL_UINT8(3, out.rs2);
}

void test_binary_to_instruction_sub_distinguished_by_funct7(void) {
  uint32_t bin = encode_r(FUNCT7_SUB, 3, 2, 0x0, 1, OPCODE_R);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_SUB, out.op);
}

void test_binary_to_instruction_addi_positive_imm(void) {
  uint32_t bin = encode_i(100, 6, 0x0, 5, OPCODE_I_ARITH);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_ADDI, out.op);
  TEST_ASSERT_EQUAL_UINT8(5, out.rd);
  TEST_ASSERT_EQUAL_UINT8(6, out.rs1);
  TEST_ASSERT_EQUAL_INT32(100, (int32_t)out.imm);
}

void test_binary_to_instruction_addi_negative_imm_sign_extends(void) {
  uint32_t bin = encode_i(-50, 2, 0x0, 1, OPCODE_I_ARITH);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL_INT32(-50, (int32_t)out.imm);
}

void test_binary_to_instruction_srli_vs_srai(void) {
  uint32_t bin_srli = encode_r(FUNCT7_SRL, 4, 2, 0x5, 1, OPCODE_I_ARITH);
  uint32_t bin_srai = encode_r(FUNCT7_SRA, 4, 2, 0x5, 1, OPCODE_I_ARITH);

  DecodedInstruction out_srli, out_srai;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin_srli, &out_srli));
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin_srai, &out_srai));
  TEST_ASSERT_EQUAL(OP_SRLI, out_srli.op);
  TEST_ASSERT_EQUAL(OP_SRAI, out_srai.op);
}

void test_binary_to_instruction_lw_roundtrip(void) {
  uint32_t bin = encode_i(16, 2, 0x2, 5, OPCODE_LOAD);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_LW, out.op);
  TEST_ASSERT_EQUAL_UINT8(5, out.rd);
  TEST_ASSERT_EQUAL_UINT8(2, out.rs1);
  TEST_ASSERT_EQUAL_INT32(16, (int32_t)out.imm);
}

void test_binary_to_instruction_sw_roundtrip(void) {
  uint32_t bin = encode_s(100, 5, 2, 0x2, OPCODE_STORE);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_SW, out.op);
  TEST_ASSERT_EQUAL_UINT8(5, out.rs2);
  TEST_ASSERT_EQUAL_UINT8(2, out.rs1);
  TEST_ASSERT_EQUAL_INT32(100, (int32_t)out.imm);
}

void test_binary_to_instruction_beq_roundtrip(void) {
  uint32_t bin = encode_b(100, 2, 1, 0x0, OPCODE_BRANCH);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_BEQ, out.op);
  TEST_ASSERT_EQUAL_UINT8(1, out.rs1);
  TEST_ASSERT_EQUAL_UINT8(2, out.rs2);
  TEST_ASSERT_EQUAL_INT32(100, (int32_t)out.imm);
}

void test_binary_to_instruction_lui_roundtrip(void) {
  uint32_t bin = encode_u(0x12345, 5, OPCODE_LUI);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_LUI, out.op);
  TEST_ASSERT_EQUAL_UINT8(5, out.rd);
  TEST_ASSERT_EQUAL_UINT32(0x12345000, out.imm); /* decode_u yields the shifted value */
}

void test_binary_to_instruction_jal_roundtrip(void) {
  uint32_t bin = encode_j(1000, 1, OPCODE_JAL);

  DecodedInstruction out;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(bin, &out));
  TEST_ASSERT_EQUAL(OP_JAL, out.op);
  TEST_ASSERT_EQUAL_UINT8(1, out.rd);
  TEST_ASSERT_EQUAL_INT32(1000, (int32_t)out.imm);
}

void test_binary_to_instruction_ecall_ebreak(void) {
  uint32_t bin_ecall = encode_i(0, 0, 0x0, 0, OPCODE_SYSTEM);
  uint32_t bin_ebreak = encode_i(1, 0, 0x0, 0, OPCODE_SYSTEM);

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

void test_decode_fence_i(void) {
  DecodedInstruction d;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(0x0000100F, &d));
  TEST_ASSERT_EQUAL_INT(OP_FENCE_I, d.op);
}

void test_decode_fence(void) {
  DecodedInstruction d;
  TEST_ASSERT_EQUAL_INT(1, binary_to_instruction(0x0FF0000F, &d));
  TEST_ASSERT_EQUAL_INT(OP_FENCE, d.op);
}

void test_decode_misc_mem_reserved_funct3(void) {
  DecodedInstruction d;
  TEST_ASSERT_EQUAL_INT(0, binary_to_instruction(0x0000200F, &d));
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
  RUN_TEST(test_decode_fence_i);
  RUN_TEST(test_decode_fence);
  RUN_TEST(test_decode_misc_mem_reserved_funct3);

  return UNITY_END();
}