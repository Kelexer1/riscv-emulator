#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "riscv.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_OPERANDS 4
#define VARIABLE_ARGS -1

/**
 * @brief Represents what kind of instruction could possibly be represented
 */
typedef enum : uint8_t { INSTRUCTION_REAL, INSTRUCTION_PSEUDO, INSTRUCTION_AMBIGUOUS } InstructionType;

/**
 * @brief Represents what kind of information is contained on a parsed line
 */
typedef enum : uint8_t { LINE_INSTRUCTION, LINE_DIRECTIVE, LINE_LABEL } LineType;

/**
 * @brief Represents what kind of information is contained in a single operator
 */
typedef enum : uint8_t {
  OPERAND_REGISTER = 1 << 0,
  OPERAND_IMMEDIATE_LITERAL = 1 << 1,
  OPERAND_SYMBOL = 1 << 2,
  OPERAND_IMMEDIATE = OPERAND_IMMEDIATE_LITERAL | OPERAND_SYMBOL,
  OPERAND_MEMORY = 1 << 3,
  OPERAND_STRING = 1 << 4
} OperandType;

/**
 * @brief Encodes all the information needed for a single operand
 */
typedef struct {
  OperandType type;
  union {
    struct {
      int reg;
    } reg;

    struct {
      int imm;
    } imm;

    struct {
      char* label;
      size_t len;
      int pc_relative;
    } label;

    struct {
      int offset;
      int base_reg;
    } mem;
  };
} Operand;

/**
 * @brief Encodes all the information needed for a single parsed instruction
 */
typedef struct {
  InstructionType type;
  union {
    Opcode op;
    PseudoOpcode pseudo;
  };
  int operand_count;
  Operand operands[MAX_OPERANDS];
} ParsedInstruction;

/**
 * @brief Encodes all the information needed for a single directive arg
 */
typedef struct directiveArg {
  OperandType type;
  int imm;
  char* start;
  size_t len;
  struct directiveArg* next;
} DirectiveArg;

/**
 * @brief Encodes all the information needed for a single parsed directive
 */
typedef struct {
  Directive directive;
  int arg_count;
  DirectiveArg* args;
} ParsedDirective;

/**
 * @brief Encodes all the information needed for one whole line of assembly
 */
typedef struct {
  LineType type;
  int line;
  char* label;
  size_t len;
  union {
    ParsedInstruction instruction;
    ParsedDirective directive;
  };
} ParsedLine;

/**
 * @brief Encodes the result of parsing a tokenized program into structured lines
 */
typedef struct {
  ParsedLine* lines;
  size_t count;
  Arena arena;
} ParsedInput;

/**
 * @brief Parses a tokenized assembly program into a structured array of ParsedLine structs
 *
 * @param tokenized The tokenized program to parse
 * @return ParsedInput* The parsed input, or NULL if an error occurred
 */
ParsedInput* parse_tokenized_input(TokenizedInput* tokenized);

/**
 * @brief Copies a symbol operand's label text into an arena, making it independently owned
 *
 * @param arena The arena to allocate into
 * @param op The operand to copy; modified in place if it is of type OPERAND_SYMBOL, otherwise
 * left unchanged
 * @return int 1 if successful (including when no copy was needed), 0 if allocation failed
 */
int copy_operand(Arena* arena, Operand* op);

/**
 * @brief Deep-copies a directive arg linked list into a new list backed by an arena
 *
 * @param arena The arena to allocate the new list and any owned text into
 * @param head The head of the directive arg list to copy
 * @return DirectiveArg* The head of the newly allocated copy, or NULL if head is NULL or an
 * error occurred
 */
DirectiveArg* copy_directive_args(Arena* arena, const DirectiveArg* head);

/**
 * @brief Copies a parsed line into dest, deep-copying any owned text and operand data into an arena
 *
 * @param arena The arena to allocate any owned text into
 * @param src The parsed line to copy
 * @param dest Where to write the copy
 * @return int 1 if successful, 0 if an error occurred
 */
int copy_parsed_line(Arena* arena, const ParsedLine* src, ParsedLine* dest);

/**
 * @brief Frees all memory associated with a directive arg linked list
 *
 * @param head The head of the arg list
 */
void free_directive_args(DirectiveArg* head);

/**
 * @brief Frees all memory associated with a parsed input, including any directive arg lists,
 * the arena, and the struct itself
 *
 * @param parsed The parsed input to free
 */
void free_parsed_input(ParsedInput* parsed);

#endif