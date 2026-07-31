#include "../include/api_disassembler.h"
#include "../include/instruction_to_binary.h"
#include "../include/riscv.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Formats a string using vsnprintf semantics into a newly allocated, exactly-sized buffer
 *
 * @param fmt The printf-style format string
 * @param ... The format arguments
 * @return char* A newly allocated, null-terminated formatted string, must be freed by the
 * caller, NULL if formatting or allocation failed
 */
static char* alloc_sprintf(const char* fmt, ...) {
  va_list args1, args2;
  va_start(args1, fmt);
  va_copy(args2, args1);

  int len = vsnprintf(NULL, 0, fmt, args1);
  va_end(args1);
  if (len < 0) {
    va_end(args2);
    return NULL;
  }

  char* buf = malloc((size_t)len + 1);
  if (!buf) {
    va_end(args2);
    return NULL;
  }

  vsnprintf(buf, (size_t)len + 1, fmt, args2);
  va_end(args2);
  return buf;
}

char* disassemble_instruction(uint32_t addr, DecodedInstruction* ins, SymbolTable* symbol_table) {
  if (!ins)
    return NULL;

  const char* op_name = mnemonic_to_str(ins->op);
  if (!op_name)
    return NULL;

  uint8_t rd = ins->rd;
  uint8_t rs1 = ins->rs1;
  uint8_t rs2 = ins->rs2;
  int32_t imm = (int32_t)ins->imm;

  InstructionFormat fmt = get_format(ins->op);
  char* result;
  const Symbol* sym = NULL;

  switch (fmt) {
  case FORMAT_R:
    result = alloc_sprintf("%s x%d, x%d, x%d", op_name, rd, rs1, rs2);
    break;
  case FORMAT_I:
    sym = value_to_symbol(symbol_table, imm, SYMBOL_CONSTANT);
    if (sym)
      result = alloc_sprintf("%s x%d, x%d, %.*s", op_name, rd, rs1, (int)sym->len, sym->name);
    else
      result = alloc_sprintf("%s x%d, x%d, %d", op_name, rd, rs1, imm);
    break;
  case FORMAT_I_SHIFT:
    result = alloc_sprintf("%s x%d, x%d, %d", op_name, rd, rs1, imm);
    break;
  case FORMAT_I_LOAD:
  case FORMAT_I_JALR:
    sym = value_to_symbol(symbol_table, imm, SYMBOL_CONSTANT);
    if (sym) {
      result = alloc_sprintf("%s x%d, %.*s(x%d)", op_name, rd, (int)sym->len, sym->name, rs1);
    } else {
      result = alloc_sprintf("%s x%d, %d(x%d)", op_name, rd, imm, rs1);
    }
    break;
  case FORMAT_S:
    sym = value_to_symbol(symbol_table, imm, SYMBOL_CONSTANT);
    if (sym)
      result = alloc_sprintf("%s x%d, %.*s(x%d)", op_name, rs2, (int)sym->len, sym->name, rs1);
    else
      result = alloc_sprintf("%s x%d, %d(x%d)", op_name, rs2, imm, rs1);
    break;
  case FORMAT_B:
    uint32_t target_b = addr + (uint32_t)imm;
    sym = value_to_symbol(symbol_table, (int32_t)target_b, SYMBOL_LABEL);
    if (sym)
      result = alloc_sprintf("%s x%d, x%d, %.*s", op_name, rs1, rs2, (int)sym->len, sym->name);
    else
      result = alloc_sprintf("%s x%d, x%d, 0x%X", op_name, rs1, rs2, target_b);
    break;
  case FORMAT_U:
    sym = value_to_symbol(symbol_table, imm, SYMBOL_CONSTANT);
    if (sym)
      result = alloc_sprintf("%s x%d, %.*s", op_name, rd, (int)sym->len, sym->name);
    else
      result = alloc_sprintf("%s x%d, 0x%X", op_name, rd, (uint32_t)imm);
    break;
  case FORMAT_J:
    uint32_t target_j = addr + (uint32_t)imm;
    sym = value_to_symbol(symbol_table, (int32_t)target_j, SYMBOL_LABEL);
    if (sym)
      result = alloc_sprintf("%s x%d, %.*s", op_name, rd, (int)sym->len, sym->name);
    else
      result = alloc_sprintf("%s x%d, 0x%X", op_name, rd, target_j);
    break;
  case FORMAT_SYSTEM:
    result = alloc_sprintf("%s", op_name);
    break;
  default:
    return NULL;
  }

  return result;
}