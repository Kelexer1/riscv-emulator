#ifndef BINARY_TO_INSTRUCTION_H
#define BINARY_TO_INSTRUCTION_H

#include "riscv.h"

/**
 * @brief Encodes a decoded instruction's opcode and operands
 */
typedef struct {
  Opcode op;
  uint8_t rd;
  uint8_t rs1;
  uint8_t rs2;
  uint32_t imm;
} DecodedInstruction;

/**
 * @brief Decodes a raw 32-bit instruction word into its opcode and operands
 *
 * @param bin The raw instruction word to decode
 * @param out The decoded instruction, populated on success
 * @return int 1 if successful, 0 if the instruction is invalid or unrecognized
 *
 * @note init_decode_table must be called once before this function is used
 */
int binary_to_instruction(uint32_t bin, DecodedInstruction* out);

/**
 * @brief Builds the internal opcode/funct3/funct7 lookup table used by binary_to_instruction
 *
 * Must be called once before binary_to_instruction is used
 */
void init_decode_table(void);

#endif