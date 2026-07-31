#include "../include/riscv.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief A lookup mapping for a single register name to number
 */
typedef struct {
  const char* name;
  int reg;
} RegMap;

/**
 * @brief A lookup table mapping all valid register names to their corresponding
 * integer values from [0, 31]
 */
static const RegMap REGISTERS[] = {
    {"x0", 0},   {"zero", 0}, {"x1", 1},   {"ra", 1},   {"x2", 2},   {"sp", 2},   {"x3", 3},   {"gp", 3},   {"x4", 4},
    {"tp", 4},   {"x5", 5},   {"t0", 5},   {"x6", 6},   {"t1", 6},   {"x7", 7},   {"t2", 7},   {"x8", 8},   {"s0", 8},
    {"fp", 8},   {"x9", 9},   {"s1", 9},   {"x10", 10}, {"a0", 10},  {"x11", 11}, {"a1", 11},  {"x12", 12}, {"a2", 12},
    {"x13", 13}, {"a3", 13},  {"x14", 14}, {"a4", 14},  {"x15", 15}, {"a5", 15},  {"x16", 16}, {"a6", 16},  {"x17", 17},
    {"a7", 17},  {"x18", 18}, {"s2", 18},  {"x19", 19}, {"s3", 19},  {"x20", 20}, {"s4", 20},  {"x21", 21}, {"s5", 21},
    {"x22", 22}, {"s6", 22},  {"x23", 23}, {"s7", 23},  {"x24", 24}, {"s8", 24},  {"x25", 25}, {"s9", 25},  {"x26", 26},
    {"s10", 26}, {"x27", 27}, {"s11", 27}, {"x28", 28}, {"t3", 28},  {"x29", 29}, {"t4", 29},  {"x30", 30}, {"t5", 30},
    {"x31", 31}, {"t6", 31}};

/**
 * @brief A lookup mapping for a single instruction mnemonic to an Opcode enum
 */
typedef struct {
  const char* name;
  Opcode op;
} MnemonicMap;

/**
 * @brief A lookup table mapping all valid instruction mnemonics to their
 * corresponding Opcode enum values
 */
static const MnemonicMap MNEMONICS[] = {
    {"add", OP_ADD},     {"sub", OP_SUB},       {"and", OP_AND},     {"or", OP_OR},         {"xor", OP_XOR},
    {"slt", OP_SLT},     {"sltu", OP_SLTU},     {"sll", OP_SLL},     {"srl", OP_SRL},       {"sra", OP_SRA},

    {"mul", OP_MUL},     {"mulh", OP_MULH},     {"mulhu", OP_MULHU}, {"mulhsu", OP_MULHSU},

    {"div", OP_DIV},     {"divu", OP_DIVU},     {"rem", OP_REM},     {"remu", OP_REMU},

    {"addi", OP_ADDI},   {"andi", OP_ANDI},     {"ori", OP_ORI},     {"xori", OP_XORI},     {"slti", OP_SLTI},
    {"sltiu", OP_SLTIU}, {"slli", OP_SLLI},     {"srli", OP_SRLI},   {"srai", OP_SRAI},

    {"lb", OP_LB},       {"lh", OP_LH},         {"lw", OP_LW},       {"lbu", OP_LBU},       {"lhu", OP_LHU},

    {"sb", OP_SB},       {"sh", OP_SH},         {"sw", OP_SW},

    {"beq", OP_BEQ},     {"bne", OP_BNE},       {"blt", OP_BLT},     {"bge", OP_BGE},       {"bltu", OP_BLTU},
    {"bgeu", OP_BGEU},

    {"jal", OP_JAL},

    {"jalr", OP_JALR},

    {"lui", OP_LUI},     {"auipc", OP_AUIPC},

    {"ecall", OP_ECALL}, {"ebreak", OP_EBREAK},
};

/**
 * @brief A lookup mapping for a single directive name to a Directive enum
 */
typedef struct {
  const char* name;
  Directive directive;
} DirectiveMap;

/**
 * @brief A lookup table mapping all valid directive names to their
 * corresponding Directive enum values
 */
static const DirectiveMap DIRECTIVES[] = {
    {".text", DIRECTIVE_TEXT},     {".data", DIRECTIVE_DATA},     {".bss", DIRECTIVE_BSS},
    {".rodata", DIRECTIVE_RODATA}, {".byte", DIRECTIVE_BYTE},     {".half", DIRECTIVE_HALF},
    {".word", DIRECTIVE_WORD},     {".string", DIRECTIVE_STRING}, {".asciz", DIRECTIVE_STRING},
    {".ascii", DIRECTIVE_ASCII},   {".zero", DIRECTIVE_ZERO},     {".space", DIRECTIVE_ZERO},
    {".equ", DIRECTIVE_EQU},       {".set", DIRECTIVE_EQU},       {".align", DIRECTIVE_ALIGN},
    {".balign", DIRECTIVE_ALIGN},  {".p2align", DIRECTIVE_ALIGN},
};

/**
 * @brief A lookup mapping for a single pseudo instruction name to a PseudoOp
 * enum
 */
typedef struct {
  const char* name;
  PseudoOpcode pseudo;
} PseudoMap;

/**
 * @brief A lookup table mappin all valid pseudo instructions to their
 * corresponding PseudoMap enum values
 */
static const PseudoMap PSEUDO_INSTRUCTIONS[] = {
    {"nop", PSEUDO_NOP},

    {"mv", PSEUDO_MOV},    {"not", PSEUDO_NOT},   {"neg", PSEUDO_NEG},

    {"seqz", PSEUDO_SEQZ}, {"snez", PSEUDO_SNEZ}, {"sltz", PSEUDO_SLTZ}, {"sgtz", PSEUDO_SGTZ},

    {"beqz", PSEUDO_BEQZ}, {"bnez", PSEUDO_BNEZ}, {"blez", PSEUDO_BLEZ}, {"bgez", PSEUDO_BGEZ},
    {"bltz", PSEUDO_BLTZ}, {"bgtz", PSEUDO_BGTZ},

    {"bgt", PSEUDO_BGT},   {"ble", PSEUDO_BLE},   {"bgtu", PSEUDO_BGTU}, {"bleu", PSEUDO_BLEU},

    {"j", PSEUDO_J},       {"jal", PSEUDO_JAL},   {"jr", PSEUDO_JR},     {"jalr", PSEUDO_JALR},
    {"ret", PSEUDO_RET},   {"call", PSEUDO_CALL}, {"tail", PSEUDO_TAIL},

    {"li", PSEUDO_LI},     {"la", PSEUDO_LA},     {"lla", PSEUDO_LLA},
};

int parse_register(const char* s, size_t len) {
  size_t s_len = 0;
  while (s_len < len && s[s_len] != '\0')
    s_len++;
  for (size_t i = 0; i < sizeof(REGISTERS) / sizeof(REGISTERS[0]); i++) {
    if (strlen(REGISTERS[i].name) == s_len && strncmp(s, REGISTERS[i].name, s_len) == 0)
      return REGISTERS[i].reg;
  }

  return -1;
}

Opcode parse_mnemonic(const char* s, size_t len) {
  size_t s_len = 0;
  while (s_len < len && s[s_len] != '\0')
    s_len++;
  for (size_t i = 0; i < sizeof(MNEMONICS) / sizeof(MNEMONICS[0]); i++) {
    if (strlen(MNEMONICS[i].name) == s_len && strncmp(s, MNEMONICS[i].name, s_len) == 0)
      return MNEMONICS[i].op;
  }

  return OP_UNKNOWN;
}

const char* mnemonic_to_str(Opcode op) {
  for (size_t i = 0; i < sizeof(MNEMONICS) / sizeof(MNEMONICS[0]); i++) {
    if (MNEMONICS[i].op == op)
      return MNEMONICS[i].name;
  }

  return NULL;
}

Directive parse_directive(const char* s, size_t len) {
  size_t s_len = 0;
  while (s_len < len && s[s_len] != '\0')
    s_len++;
  for (size_t i = 0; i < sizeof(DIRECTIVES) / sizeof(DIRECTIVES[0]); i++) {
    if (strlen(DIRECTIVES[i].name) == s_len && strncmp(s, DIRECTIVES[i].name, s_len) == 0)
      return DIRECTIVES[i].directive;
  }

  return DIRECTIVE_UNKNOWN;
}

PseudoOpcode parse_pseudo(const char* s, size_t len) {
  size_t s_len = 0;
  while (s_len < len && s[s_len] != '\0')
    s_len++;
  for (size_t i = 0; i < sizeof(PSEUDO_INSTRUCTIONS) / sizeof(PSEUDO_INSTRUCTIONS[0]); i++) {
    if (strlen(PSEUDO_INSTRUCTIONS[i].name) == s_len && strncmp(s, PSEUDO_INSTRUCTIONS[i].name, s_len) == 0)
      return PSEUDO_INSTRUCTIONS[i].pseudo;
  }

  return PSEUDO_UNKNOWN;
}