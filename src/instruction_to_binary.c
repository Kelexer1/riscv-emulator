#include "../include/instruction_to_binary.h"

/**
 * @brief Returns the format for a given Opcode
 *
 * @param op The opcode
 * @return InstructionFormat The instruction format
 */
InstructionFormat get_format(Opcode op) {
  switch (op) {
  case OP_ADD:
  case OP_SUB:
  case OP_SLL:
  case OP_SLT:
  case OP_SLTU:
  case OP_XOR:
  case OP_SRL:
  case OP_SRA:
  case OP_OR:
  case OP_AND:
  case OP_MUL:
  case OP_MULH:
  case OP_MULHU:
  case OP_MULHSU:
  case OP_DIV:
  case OP_DIVU:
  case OP_REM:
  case OP_REMU:
    return FORMAT_R;

  case OP_ADDI:
  case OP_SLTI:
  case OP_SLTIU:
  case OP_XORI:
  case OP_ORI:
  case OP_ANDI:
    return FORMAT_I;

  case OP_SLLI:
  case OP_SRLI:
  case OP_SRAI:
    return FORMAT_I_SHIFT;

  case OP_LB:
  case OP_LH:
  case OP_LW:
  case OP_LBU:
  case OP_LHU:
    return FORMAT_I_LOAD;

  case OP_JALR:
    return FORMAT_I_JALR;

  case OP_SB:
  case OP_SH:
  case OP_SW:
    return FORMAT_S;

  case OP_BEQ:
  case OP_BNE:
  case OP_BLT:
  case OP_BGE:
  case OP_BLTU:
  case OP_BGEU:
    return FORMAT_B;

  case OP_LUI:
  case OP_AUIPC:
    return FORMAT_U;

  case OP_JAL:
    return FORMAT_J;

  case OP_ECALL:
  case OP_EBREAK:
    return FORMAT_SYSTEM;
  default:
    return FORMAT_UNKNOWN;
  }
}