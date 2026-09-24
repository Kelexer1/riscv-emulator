#include "../include/binary_to_instruction.h"
#include "../include/instruction_to_binary.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief The maximum number of opcodes that may share the same 7-bit RISC-V opcode field
 */
#define MAX_BUCKET 18

/**
 * @brief A single entry in the decode table, mapping an opcode/funct3/funct7 combination to its
 * instruction format
 */
typedef struct {
  Opcode op;
  uint8_t funct3;
  uint8_t funct7;
  InstructionFormat fmt;
} DecodeEntry;

/**
 * @brief A lookup table, bucketed by 7-bit RISC-V opcode field, of DecodeEntry candidates;
 * populated by init_decode_table
 */
static DecodeEntry decode_table[128][MAX_BUCKET];

/**
 * @brief The number of populated entries in each bucket of decode_table
 */
static uint8_t decode_table_count[128];

void init_decode_table(void) {
  memset(decode_table_count, 0, sizeof(decode_table_count));
  for (Opcode o = 0; o < OP_EBREAK; o++) {
    OpcodeEncoding e = OPCODE_TABLE[o];
    InstructionFormat fmt = get_format(o);
    uint8_t idx = decode_table_count[e.opcode]++;
    if (idx >= MAX_BUCKET) {
      fprintf(stderr, "decode_table bucket overflow for opcode 0x%02x (idx %d >= MAX_OPERANDS %d)\n", e.opcode, idx,
              MAX_BUCKET);
      abort();
    }
    decode_table[e.opcode][idx] = (DecodeEntry){
        .op = o,
        .funct3 = e.funct3,
        .funct7 = e.funct7,
        .fmt = fmt,
    };
  }
}

/**
 * @brief Decodes the register operands of an R-Type instruction
 *
 * @param ins The raw instruction word
 * @return DecodedInstruction A partially populated instruction with rd, rs1, and rs2 set
 */
static DecodedInstruction decode_r(uint32_t ins) {
  return (DecodedInstruction){
      .rd = (ins >> 7) & 0x1F,
      .rs1 = (ins >> 15) & 0x1F,
      .rs2 = (ins >> 20) & 0x1F,
  };
}

/**
 * @brief Decodes the operands of an I-Type instruction
 *
 * @param ins The raw instruction word
 * @return DecodedInstruction A partially populated instruction with rd, rs1, and imm set
 */
static DecodedInstruction decode_i(uint32_t ins) {
  return (DecodedInstruction){
      .rd = (ins >> 7) & 0x1F,
      .rs1 = (ins >> 15) & 0x1F,
      .imm = (int32_t)((ins >> 20) << 20) >> 20,
  };
}

/**
 * @brief Decodes the operands of an I-Type shift instruction
 *
 * @param ins The raw instruction word
 * @return DecodedInstruction A partially populated instruction with rd, rs1, and the shift
 * amount in imm set
 */
static DecodedInstruction decode_i_shift(uint32_t ins) {
  return (DecodedInstruction){.rd = (ins >> 7) & 0x1F, .rs1 = (ins >> 15) & 0x1F, .imm = (int32_t)((ins >> 20) & 0x1F)};
}

/**
 * @brief Decodes the operands of an S-Type instruction
 *
 * @param ins The raw instruction word
 * @return DecodedInstruction A partially populated instruction with rs1, rs2, and imm set
 */
static DecodedInstruction decode_s(uint32_t ins) {
  uint32_t imm_4_0 = (ins >> 7) & 0x1F;
  uint32_t imm_11_5 = (ins >> 25) & 0x7F;
  uint32_t raw = (imm_11_5 << 5) | imm_4_0;
  return (DecodedInstruction){
      .rs1 = (ins >> 15) & 0x1F,
      .rs2 = (ins >> 20) & 0x1F,
      .imm = (int32_t)(raw << 20) >> 20,
  };
}

/**
 * @brief Decodes the operands of a B-Type instruction
 *
 * @param ins The raw instruction word
 * @return DecodedInstruction A partially populated instruction with rs1, rs2, and the branch
 * offset in imm set
 */
static DecodedInstruction decode_b(uint32_t ins) {
  uint32_t imm_11 = (ins >> 7) & 0x1;
  uint32_t imm_4_1 = (ins >> 8) & 0xF;
  uint32_t imm_10_5 = (ins >> 25) & 0x3F;
  uint32_t imm_12 = (ins >> 31) & 0x1;
  uint32_t raw = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
  return (DecodedInstruction){
      .rs1 = (ins >> 15) & 0x1F,
      .rs2 = (ins >> 20) & 0x1F,
      .imm = (int32_t)(raw << 19) >> 19,
  };
}

/**
 * @brief Decodes the operands of a U-Type instruction
 *
 * @param ins The raw instruction word
 * @return DecodedInstruction A partially populated instruction with rd and imm set
 */
static DecodedInstruction decode_u(uint32_t ins) {
  return (DecodedInstruction){
      .rd = (ins >> 7) & 0x1F,
      .imm = (int32_t)(ins & 0xFFFFF000),
  };
}

/**
 * @brief Decodes the operands of a J-Type instruction
 *
 * @param ins The raw instruction word
 * @return DecodedInstruction A partially populated instruction with rd and the jump offset in
 * imm set
 */
static DecodedInstruction decode_j(uint32_t ins) {
  uint32_t imm_19_12 = (ins >> 12) & 0xFF;
  uint32_t imm_11 = (ins >> 20) & 0x1;
  uint32_t imm_10_1 = (ins >> 21) & 0x3FF;
  uint32_t imm_20 = (ins >> 31) & 0x1;
  uint32_t raw = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
  return (DecodedInstruction){
      .rd = (ins >> 7) & 0x1F,
      .imm = (int32_t)(raw << 11) >> 11,
  };
}

int binary_to_instruction(uint32_t bin, DecodedInstruction* out) {
  if (!out)
    return 0;

  uint8_t opcode_bin = bin & 0x7F;

  if (opcode_bin == 0x73) {
    *out = (DecodedInstruction){.op = ((bin >> 20) & 0x1) ? OP_EBREAK : OP_ECALL};
    return 1;
  }

  if (opcode_bin == 0x0F) {
    uint8_t misc_funct3 = (bin >> 12) & 0x7;
    if (misc_funct3 == 0) {
      *out = (DecodedInstruction){.op = OP_FENCE, .imm = (int32_t)(bin >> 20)};
      return 1;
    }
    if (misc_funct3 == 1) {
      *out = (DecodedInstruction){.op = OP_FENCE_I};
      return 1;
    }
    return 0;
  }

  uint8_t funct3_bin = (bin >> 12) & 0x7;
  uint8_t funct7_bin = (bin >> 25) & 0x7F;

  DecodeEntry* entries = decode_table[opcode_bin];
  uint8_t bucket_size = decode_table_count[opcode_bin];
  DecodeEntry* match = NULL;

  for (uint8_t i = 0; i < bucket_size; i++) {
    DecodeEntry* e = &entries[i];
    InstructionFormat fmt = e->fmt;
    if ((fmt == FORMAT_R || fmt == FORMAT_I || fmt == FORMAT_I_SHIFT || fmt == FORMAT_I_LOAD || fmt == FORMAT_I_JALR ||
         fmt == FORMAT_S || fmt == FORMAT_B) &&
        e->funct3 != funct3_bin)
      continue;
    if ((fmt == FORMAT_R || fmt == FORMAT_I_SHIFT) && e->funct7 != funct7_bin)
      continue;
    match = e;
    break;
  }
  if (!match)
    return 0;

  switch (match->fmt) {
  case FORMAT_R:
    *out = decode_r(bin);
    break;
  case FORMAT_I:
  case FORMAT_I_LOAD:
  case FORMAT_I_JALR:
    *out = decode_i(bin);
    break;
  case FORMAT_I_SHIFT:
    *out = decode_i_shift(bin);
    break;
  case FORMAT_S:
    *out = decode_s(bin);
    break;
  case FORMAT_B:
    *out = decode_b(bin);
    break;
  case FORMAT_U:
    *out = decode_u(bin);
    break;
  case FORMAT_J:
    *out = decode_j(bin);
    break;
  default:
    return 0;
  }

  out->op = match->op;
  return 1;
}