#ifndef RISCV_H
#define RISCV_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Represents a specific supported instruction type (or an invalid one)
 */
typedef enum : uint8_t {
  OP_UNKNOWN,
  OP_ADD,
  OP_SUB,
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_SLT,
  OP_SLTU,
  OP_SLL,
  OP_SRL,
  OP_SRA,
  OP_MUL,
  OP_MULH,
  OP_MULHU,
  OP_MULHSU,
  OP_DIV,
  OP_DIVU,
  OP_REM,
  OP_REMU,
  OP_ADDI,
  OP_ANDI,
  OP_ORI,
  OP_XORI,
  OP_SLTI,
  OP_SLTIU,
  OP_SLLI,
  OP_SRLI,
  OP_SRAI,
  OP_LB,
  OP_LH,
  OP_LW,
  OP_LBU,
  OP_LHU,
  OP_SB,
  OP_SH,
  OP_SW,
  OP_BEQ,
  OP_BNE,
  OP_BLT,
  OP_BGE,
  OP_BLTU,
  OP_BGEU,
  OP_JAL,
  OP_JALR,
  OP_LUI,
  OP_AUIPC,
  OP_ECALL,
  OP_EBREAK,
} Opcode;

/**
 * @brief Represents a specific supported pseudoinstruction (or an invalid one)
 * that requires decomposition into one or more instructions
 */
typedef enum : uint8_t {
  PSEUDO_UNKNOWN,
  PSEUDO_NOP,
  PSEUDO_MOV,
  PSEUDO_NOT,
  PSEUDO_NEG,
  PSEUDO_SEQZ,
  PSEUDO_SNEZ,
  PSEUDO_SLTZ,
  PSEUDO_SGTZ,
  PSEUDO_BEQZ,
  PSEUDO_BNEZ,
  PSEUDO_BLEZ,
  PSEUDO_BGEZ,
  PSEUDO_BLTZ,
  PSEUDO_BGTZ,
  PSEUDO_BGT,
  PSEUDO_BLE,
  PSEUDO_BGTU,
  PSEUDO_BLEU,
  PSEUDO_J,
  PSEUDO_JAL,
  PSEUDO_JR,
  PSEUDO_JALR,
  PSEUDO_RET,
  PSEUDO_CALL,
  PSEUDO_TAIL,
  PSEUDO_LI,
  PSEUDO_LA,
  PSEUDO_LLA,
} PseudoOpcode;

/**
 * @brief Represents a specific supported directive type (or an invalid one)
 */
typedef enum : uint8_t {
  DIRECTIVE_UNKNOWN,
  DIRECTIVE_TEXT,
  DIRECTIVE_DATA,
  DIRECTIVE_BSS,
  DIRECTIVE_RODATA,
  DIRECTIVE_BYTE,
  DIRECTIVE_HALF,
  DIRECTIVE_WORD,
  DIRECTIVE_STRING,
  DIRECTIVE_ASCII,
  DIRECTIVE_ZERO,
  DIRECTIVE_EQU,
  DIRECTIVE_ALIGN
} Directive;

/**
 * @brief Parses a register given its string representation as either a raw
 * numeric, or an alias name, case sensitive
 *
 * @param s The string representation of the register, not necessarily
 * null-terminated
 * @param len The number of characters in s
 * @return int The register as an integer from [0, 31], -1 if s is not a valid
 * register or an error occurred
 */
int parse_register(const char* s, size_t len);

/**
 * @brief Parses an instruction name given its string representation, case
 * sensitive
 *
 * @param s The string representation of the instruction, not necessarily
 * null-terminated
 * @param len The number of characters in s
 * @return Opcode The mnemonic as an Opcode enum, OP_UNKNOWN if s is not a
 * valid instruction or an error occurred
 */
Opcode parse_mnemonic(const char* s, size_t len);

/**
 * @brief Converts an Opcode enum value back to its mnemonic string representation
 *
 * @param op The opcode
 * @return const char* The mnemonic string, NULL if op does not match any supported instruction
 */
const char* mnemonic_to_str(Opcode op);

/**
 * @brief Parses a directive name given its string representation, case
 * sensitive
 *
 * @param s The string representation of the instruction, not necessarily
 * null-terminated
 * @param len The number of characters in s
 * @return Directive The directive as a Directive enum, DIRECTIVE_UNKNOWN if s
 * is not a valid directive or an error occurred
 */
Directive parse_directive(const char* s, size_t len);

/**
 * @brief Parses a pseudo instruction name given its string representation, case
 * sensitive
 *
 * @param s The string representation of the pseudo instruction, not necessarily
 * null-terminated
 * @param len The number of characters in s
 * @return PseudoOpcode The pseudo instruction as a PseudoOp enum,
 * PSEUDO_UNKNOWN if s if not a valid pseudo instruction
 */
PseudoOpcode parse_pseudo(const char* s, size_t len);

#endif