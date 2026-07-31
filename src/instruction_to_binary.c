#include "../include/instruction_to_binary.h"
#include "../include/logger.h"

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

/**
 * @brief Encodes a R-Type instruction
 *
 * @param ins The instruction as a ParsedLine struct
 * @param out Where to write the encoded instruction binary
 * @return int 1 if success, 0 if the operation failed
 */
static int encode_r(ParsedLine* ins, uint32_t* out) {
  OpcodeEncoding e = OPCODE_TABLE[ins->instruction.op];
  uint32_t rd = ins->instruction.operands[0].reg.reg;
  uint32_t rs1 = ins->instruction.operands[1].reg.reg;
  uint32_t rs2 = ins->instruction.operands[2].reg.reg;

  *out = (e.funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (e.funct3 << 12) | (rd << 7) | e.opcode;
  return 1;
}

/**
 * @brief Encodes an I-Type instruction
 *
 * @param ins The instruction as a ParsedLine struct
 * @param current_addr The current address, used to resolve pc-relative symbols
 * @param symbol_table The symbol table, used to resolve symbol operands
 * @param out Where to write the encoded instruction binary
 * @return int 1 if success, 0 if the operation failed
 */
static int encode_i(ParsedLine* ins, uint32_t current_addr, SymbolTable* symbol_table, uint32_t* out) {
  OpcodeEncoding e = OPCODE_TABLE[ins->instruction.op];
  uint32_t rd = ins->instruction.operands[0].reg.reg;
  uint32_t rs1 = ins->instruction.operands[1].reg.reg;
  Operand* src = &ins->instruction.operands[2];

  int32_t imm;
  if (src->type == OPERAND_SYMBOL) {
    Symbol sym;
    if (!resolve_symbol(symbol_table, src->label.label, src->label.len, &sym)) {
      LOG_ERROR_LINE("Failed to resolve symbol \"%.*s\"", ins->line, (int)src->label.len, src->label.label);
      return 0;
    }

    if (!src->label.pc_relative) {
      int32_t val = (int32_t)sym.value;
      int32_t hi20 = (val + 0x800) >> 12;
      imm = val - (hi20 << 12);
    } else {
      uint32_t auipc_addr = current_addr - 4;
      int32_t offset = (int32_t)(sym.value - auipc_addr);
      int32_t hi20 = (offset + 0x800) >> 12;
      imm = offset - (hi20 << 12);
    }
  } else {
    imm = ins->instruction.operands[2].imm.imm;
    if (imm < -0x800 || imm > 0x7FF) {
      LOG_ERROR_LINE("Immediate %d out of range [%d, %d]", ins->line, imm, -0x800, 0x7FF);
      return 0;
    }
  }

  uint32_t imm_bits = ((uint32_t)imm) & 0xFFF;
  *out = (imm_bits << 20) | (rs1 << 15) | (e.funct3 << 12) | (rd << 7) | e.opcode;
  return 1;
}

/**
 * @brief Encodes an I-Type shift instruction
 *
 * @param ins The instruction as a ParsedLine struct
 * @param out Where to write the encoded instruction binary
 * @return int 1 if success, 0 if the operation failed
 */
static int encode_i_shift(ParsedLine* ins, uint32_t* out) {
  int32_t shamt_raw = ins->instruction.operands[2].imm.imm;
  if (shamt_raw < 0 || shamt_raw > 0x1F) {
    LOG_ERROR_LINE("Shift amount %d out of range [%d, %d]", ins->line, shamt_raw, 0, 0x1F);
    return 0;
  }

  OpcodeEncoding e = OPCODE_TABLE[ins->instruction.op];
  uint32_t rd = ins->instruction.operands[0].reg.reg;
  uint32_t rs1 = ins->instruction.operands[1].reg.reg;
  uint32_t shamt = (uint32_t)shamt_raw;

  *out = (e.funct7 << 25) | (shamt << 20) | (rs1 << 15) | (e.funct3 << 12) | (rd << 7) | e.opcode;
  return 1;
}

/**
 * @brief Encodes an I-Type load instruction
 *
 * @param ins The instruction as a ParsedLine struct
 * @param out Where to write the encoded instruction binary
 * @return int 1 if success, 0 if the operation failed
 */
static int encode_i_load(ParsedLine* ins, uint32_t* out) {
  int32_t imm = ins->instruction.operands[1].mem.offset;
  if (imm < -0x800 || imm > 0x7FF) {
    LOG_ERROR_LINE("Load offset %d out of range [%d, %d]", ins->line, imm, -0x800, 0x7FF);
    return 0;
  }

  OpcodeEncoding e = OPCODE_TABLE[ins->instruction.op];
  uint32_t rd = ins->instruction.operands[0].reg.reg;
  uint32_t rs1 = ins->instruction.operands[1].mem.base_reg;
  uint32_t imm_bits = (uint32_t)imm;

  *out = (imm_bits << 20) | (rs1 << 15) | (e.funct3 << 12) | (rd << 7) | e.opcode;
  return 1;
}

/**
 * @brief Encodes a S-Type instruction
 *
 * @param ins The instruction as a ParsedLine struct
 * @param out Where to write the encoded instruction binary
 * @return int 1 if success, 0 if the operation failed
 */
static int encode_s(ParsedLine* ins, uint32_t* out) {
  int32_t imm = ins->instruction.operands[1].mem.offset;
  if (imm < -0x800 || imm > 0x7FF) {
    LOG_ERROR_LINE("Store offset %d out of range [%d, %d]", ins->line, imm, -0x800, 0x7FF);
    return 0;
  }

  OpcodeEncoding e = OPCODE_TABLE[ins->instruction.op];
  uint32_t rs2 = ins->instruction.operands[0].reg.reg;
  uint32_t rs1 = ins->instruction.operands[1].mem.base_reg;
  uint32_t imm_bits = (uint32_t)imm;

  uint32_t imm_11_5 = (imm_bits >> 5) & 0x7F;
  uint32_t imm_4_0 = imm_bits & 0x1F;

  *out = (imm_11_5 << 25) | (rs2 << 20) | (rs1 << 15) | (e.funct3 << 12) | (imm_4_0 << 7) | e.opcode;
  return 1;
}

/**
 * @brief Encodes a B-Type instruction
 *
 * @param ins The instruction as a ParsedLine struct
 * @param target_addr The resolved absolute target address of the branch
 * @param current_addr The current address, used to compute the branch offset
 * @param out Where to write the encoded instruction binary
 * @return int 1 if success, 0 if the operation failed
 */
static int encode_b(ParsedLine* ins, uint32_t target_addr, uint32_t current_addr, uint32_t* out) {
  int32_t offset = (int32_t)(target_addr - current_addr);
  if (offset < -0x1000 || offset > 0xFFF) {
    LOG_ERROR_LINE("Branch offset %d out of range [%d, %d]", ins->line, offset, -0x1000, 0xFFF);
    return 0;
  }
  if (offset % 4 != 0) {
    LOG_ERROR_LINE("Branch offset %d is not 4-byte aligned", ins->line, offset);
    return 0;
  }

  OpcodeEncoding e = OPCODE_TABLE[ins->instruction.op];
  uint32_t rs1 = ins->instruction.operands[0].reg.reg;
  uint32_t rs2 = ins->instruction.operands[1].reg.reg;

  uint32_t imm = (uint32_t)offset;
  uint32_t imm_12 = (imm >> 12) & 0x1;
  uint32_t imm_11 = (imm >> 11) & 0x1;
  uint32_t imm_10_5 = (imm >> 5) & 0x3F;
  uint32_t imm_4_1 = (imm >> 1) & 0xF;

  *out = (imm_12 << 31) | (imm_10_5 << 25) | (rs2 << 20) | (rs1 << 15) | (e.funct3 << 12) | (imm_4_1 << 8) |
         (imm_11 << 7) | e.opcode;
  return 1;
}

/**
 * @brief Encodes a U-Type instruction
 *
 * @param ins The instruction as a ParsedLine struct
 * @param current_addr The current address, used to resolve pc-relative symbols
 * @param symbol_table The symbol table, used to resolve symbol operands
 * @param out Where to write the encoded instruction binary
 * @return int 1 if success, 0 if the operation failed
 */
static int encode_u(ParsedLine* ins, uint32_t current_addr, SymbolTable* symbol_table, uint32_t* out) {
  OpcodeEncoding e = OPCODE_TABLE[ins->instruction.op];
  uint32_t rd = ins->instruction.operands[0].reg.reg;
  Operand* src = &ins->instruction.operands[1];
  int32_t imm;

  if (src->type == OPERAND_SYMBOL) {
    Symbol sym;
    if (!resolve_symbol(symbol_table, src->label.label, src->label.len, &sym)) {
      LOG_ERROR_LINE("Failed to resolve symbol \"%.*s\"", ins->line, (int)src->label.len, src->label.label);
      return 0;
    }

    if (!src->label.pc_relative) {
      int32_t val = (int32_t)sym.value;
      imm = (val + 0x800) >> 12;
    } else {
      int32_t offset = (int32_t)(sym.value - current_addr);
      imm = (offset + 0x800) >> 12;
    }
  } else {
    imm = src->imm.imm;
    if (imm < 0 || imm > 0xFFFFF) {
      LOG_ERROR_LINE("Immediate %d out of range [%d, %d]", ins->line, imm, 0, 0xFFFFF);
      return 0;
    }
  }

  uint32_t imm_bits = ((uint32_t)imm) & 0xFFFFF;
  *out = (imm_bits << 12) | (rd << 7) | e.opcode;
  return 1;
}

/**
 * @brief Encodes a J-Type instruction
 *
 * @param ins The instruction as a ParsedLine struct
 * @param target_addr The resolved absolute target address of the jump
 * @param current_addr The current address, used to compute the jump offset
 * @param out Where to write the encoded instruction binary
 * @return int 1 if success, 0 if the operation failed
 */
static int encode_j(ParsedLine* ins, uint32_t target_addr, uint32_t current_addr, uint32_t* out) {
  int32_t offset = (int32_t)(target_addr - current_addr);
  if (offset < -0x100000 || offset > 0xFFFFF) {
    LOG_ERROR_LINE("JAL offset %d out of range [%d, %d]", ins->line, offset, -0x100000, 0xFFFFF);
    return 0;
  }
  if (offset % 4 != 0) {
    LOG_ERROR_LINE("JAL offset %d is not 4-byte aligned", ins->line, offset);
    return 0;
  }

  OpcodeEncoding e = OPCODE_TABLE[ins->instruction.op];
  uint32_t rd = ins->instruction.operands[0].reg.reg;

  uint32_t imm = (uint32_t)offset;
  uint32_t imm_20 = (imm >> 20) & 0x1;
  uint32_t imm_19_12 = (imm >> 12) & 0xFF;
  uint32_t imm_11 = (imm >> 11) & 0x1;
  uint32_t imm_10_1 = (imm >> 1) & 0x3FF;

  *out = (imm_20 << 31) | (imm_10_1 << 21) | (imm_11 << 20) | (imm_19_12 << 12) | (rd << 7) | e.opcode;
  return 1;
}

/**
 * @brief Encodes a system instruction
 *
 * @param ins The instruction as a ParsedLine struct
 * @param out Where to write the encoded instruction binary
 * @return int 1 if success, 0 if the operation failed
 */
static int encode_system(ParsedLine* ins, uint32_t* out) {
  uint32_t imm = (ins->instruction.op == OP_EBREAK) ? 0x001 : 0x000;
  *out = (imm << 20) | OPCODE_TABLE[ins->instruction.op].opcode;
  return 1;
}

int instruction_to_binary(ParsedLine* line, uint32_t current_addr, SymbolTable* symbol_table, uint32_t* out) {
  if (!out)
    return 0;

  switch (get_format(line->instruction.op)) {
  case FORMAT_R:
    return encode_r(line, out);
  case FORMAT_I:
  case FORMAT_I_JALR:
    return encode_i(line, current_addr, symbol_table, out);
  case FORMAT_I_SHIFT:
    return encode_i_shift(line, out);
  case FORMAT_I_LOAD:
    return encode_i_load(line, out);
  case FORMAT_S:
    return encode_s(line, out);
  case FORMAT_B:
    Operand* target_b = &line->instruction.operands[2];
    if (target_b->type == OPERAND_SYMBOL) {
      Symbol sym_b;
      if (!resolve_symbol(symbol_table, target_b->label.label, target_b->label.len, &sym_b)) {
        LOG_ERROR_LINE("Failed to resolve symbol \"%.*s\"", line->line, (int)target_b->label.len,
                       target_b->label.label);
        return 0;
      }
      uint32_t target_addr = (sym_b.type == SYMBOL_CONSTANT) ? current_addr + sym_b.value : sym_b.value;
      return encode_b(line, target_addr, current_addr, out);
    } else {
      return encode_b(line, target_b->imm.imm, current_addr, out);
    }
  case FORMAT_U:
    return encode_u(line, current_addr, symbol_table, out);
  case FORMAT_J:
    Operand* target_j = &line->instruction.operands[1];
    if (target_j->type == OPERAND_SYMBOL) {
      Symbol sym_j;
      if (!resolve_symbol(symbol_table, target_j->label.label, target_j->label.len, &sym_j)) {
        LOG_ERROR_LINE("Failed to resolve symbol \"%.*s\"", line->line, (int)target_j->label.len,
                       target_j->label.label);
        return 0;
      }
      uint32_t target_addr = (sym_j.type == SYMBOL_CONSTANT) ? current_addr + sym_j.value : sym_j.value;
      return encode_j(line, target_addr, current_addr, out);
    } else {
      return encode_j(line, target_j->imm.imm, current_addr, out);
    }
  case FORMAT_SYSTEM:
    return encode_system(line, out);
  default:
    return 0;
  }
}