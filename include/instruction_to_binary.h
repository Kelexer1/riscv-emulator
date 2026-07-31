#ifndef INSTRUCTION_TO_BINARY_H
#define INSTRUCTION_TO_BINARY_H

#include "first_pass.h"
#include "parser.h"
#include <stdint.h>

/**
 * @brief A struct encoding various binary field values for different Opcodes
 */
typedef struct {
  uint8_t opcode;
  uint8_t funct3;
  uint8_t funct7;
} OpcodeEncoding;

/**
 * @brief A lookup table mapping Opcodes to their OpcodeEncoding structs for
 * binary representation
 */
static const OpcodeEncoding OPCODE_TABLE[] = {
    // R-Type
    [OP_ADD] = {0x33, 0x0, 0x00},
    [OP_SUB] = {0x33, 0x0, 0x20},
    [OP_SLL] = {0x33, 0x1, 0x00},
    [OP_SLT] = {0x33, 0x2, 0x00},
    [OP_SLTU] = {0x33, 0x3, 0x00},
    [OP_XOR] = {0x33, 0x4, 0x00},
    [OP_SRL] = {0x33, 0x5, 0x00},
    [OP_SRA] = {0x33, 0x5, 0x20},
    [OP_OR] = {0x33, 0x6, 0x00},
    [OP_AND] = {0x33, 0x7, 0x00},

    // R-Type (M extension)
    [OP_MUL] = {0x33, 0x0, 0x01},
    [OP_MULH] = {0x33, 0x1, 0x01},
    [OP_MULHSU] = {0x33, 0x2, 0x01},
    [OP_MULHU] = {0x33, 0x3, 0x01},
    [OP_DIV] = {0x33, 0x4, 0x01},
    [OP_DIVU] = {0x33, 0x5, 0x01},
    [OP_REM] = {0x33, 0x6, 0x01},
    [OP_REMU] = {0x33, 0x7, 0x01},

    // I-Type
    [OP_ADDI] = {0x13, 0x0, 0x00},
    [OP_SLTI] = {0x13, 0x2, 0x00},
    [OP_SLTIU] = {0x13, 0x3, 0x00},
    [OP_XORI] = {0x13, 0x4, 0x00},
    [OP_ORI] = {0x13, 0x6, 0x00},
    [OP_ANDI] = {0x13, 0x7, 0x00},
    [OP_SLLI] = {0x13, 0x1, 0x00},
    [OP_SRLI] = {0x13, 0x5, 0x00},
    [OP_SRAI] = {0x13, 0x5, 0x20},

    // I-Type (Load)
    [OP_LB] = {0x03, 0x0, 0x00},
    [OP_LH] = {0x03, 0x1, 0x00},
    [OP_LW] = {0x03, 0x2, 0x00},
    [OP_LBU] = {0x03, 0x4, 0x00},
    [OP_LHU] = {0x03, 0x5, 0x00},

    // S-Type (Store)
    [OP_SB] = {0x23, 0x0, 0x00},
    [OP_SH] = {0x23, 0x1, 0x00},
    [OP_SW] = {0x23, 0x2, 0x00},

    // B-Type
    [OP_BEQ] = {0x63, 0x0, 0x00},
    [OP_BNE] = {0x63, 0x1, 0x00},
    [OP_BLT] = {0x63, 0x4, 0x00},
    [OP_BGE] = {0x63, 0x5, 0x00},
    [OP_BLTU] = {0x63, 0x6, 0x00},
    [OP_BGEU] = {0x63, 0x7, 0x00},

    // J-Type
    [OP_JAL] = {0x6F, 0x0, 0x00},

    // I-Type (JALR)
    [OP_JALR] = {0x67, 0x0, 0x00},

    // U-Type
    [OP_LUI] = {0x37, 0x0, 0x00},
    [OP_AUIPC] = {0x17, 0x0, 0x00},

    // System
    [OP_ECALL] = {0x73, 0x0, 0x00},
    [OP_EBREAK] = {0x73, 0x0, 0x00},
};

/**
 * @brief Represents the category an instruction falls into, such as R-Type for
 * register->register instructions
 */
typedef enum : uint8_t {
  FORMAT_UNKNOWN,
  FORMAT_R,
  FORMAT_I,
  FORMAT_I_SHIFT,
  FORMAT_I_LOAD,
  FORMAT_I_JALR,
  FORMAT_S,
  FORMAT_B,
  FORMAT_U,
  FORMAT_J,
  FORMAT_SYSTEM
} InstructionFormat;

/**
 * @brief Represents a single assembled instruction in a format ideal for
 * simulation
 */
typedef struct {
  uint8_t success;

  uint32_t address;
  Opcode op;
  int operands[MAX_OPERANDS];
  size_t operand_count;
} AssembledInstruction;

/**
 * @brief Converts an instruction parsed line into a 32-bit binary sequence
 *
 * @param line The parsed line, where line->type == LINE_INSTRUCTION
 * @param current_addr The current address
 * @param symbol_table The symbol table
 * @param out Where to fill the binary
 * @return int 1 if success, 0 if the operation failed
 */
int instruction_to_binary(ParsedLine* line, uint32_t current_addr, SymbolTable* symbol_table, uint32_t* out);

/**
 * @brief Returns the instruction format for a given opcode
 *
 * @param op The opcode
 * @return InstructionFormat The instruction format
 */
InstructionFormat get_format(Opcode op);

#endif