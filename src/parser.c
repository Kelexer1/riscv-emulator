#include "../include/parser.h"
#include "../include/dynamic_array.h"
#include "../include/lexer.h"
#include "../include/logger.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief A bitmask for different types of operands that an instruction or
 * directive supports
 */
typedef uint32_t OperandMask;

/**
 * @brief A lookup mapping for a single Opcode to its operand counts and
 * accepted values
 */
typedef struct {
  Opcode op;
  int operand_count;
  OperandMask operands[MAX_OPERANDS];
} InstructionFormat;

/**
 * @brief A lookup table mapping all Opcode values to their formats
 */
static const InstructionFormat INSTRUCTION_FORMATS[] = {
    {OP_ADD, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_SUB, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_AND, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_OR, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_XOR, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_SLT, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_SLTU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_SLL, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_SRL, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_SRA, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},

    {OP_MUL, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_MULH, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_MULHU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_MULHSU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},

    {OP_DIV, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_DIVU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_REM, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},
    {OP_REMU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER}},

    {OP_ADDI, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_ANDI, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_ORI, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_XORI, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_SLTI, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_SLTIU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_SLLI, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_SRLI, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_SRAI, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},

    {OP_LB, 2, {OPERAND_REGISTER, OPERAND_MEMORY}},
    {OP_LH, 2, {OPERAND_REGISTER, OPERAND_MEMORY}},
    {OP_LW, 2, {OPERAND_REGISTER, OPERAND_MEMORY}},
    {OP_LBU, 2, {OPERAND_REGISTER, OPERAND_MEMORY}},
    {OP_LHU, 2, {OPERAND_REGISTER, OPERAND_MEMORY}},

    {OP_SB, 2, {OPERAND_REGISTER, OPERAND_MEMORY}},
    {OP_SH, 2, {OPERAND_REGISTER, OPERAND_MEMORY}},
    {OP_SW, 2, {OPERAND_REGISTER, OPERAND_MEMORY}},

    {OP_BEQ, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_BNE, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_BLT, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_BGE, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_BLTU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_BGEU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},

    {OP_JAL, 2, {OPERAND_REGISTER, OPERAND_IMMEDIATE}},

    {OP_JALR, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_IMMEDIATE}},

    {OP_LUI, 2, {OPERAND_REGISTER, OPERAND_IMMEDIATE}},
    {OP_AUIPC, 2, {OPERAND_REGISTER, OPERAND_IMMEDIATE}},

    {OP_ECALL, 0, {0}},
    {OP_EBREAK, 0, {0}},
};

/**
 * @brief A lookup mapping for a single Directive to information about its
 * arguments
 */
typedef struct {
  Directive directive;
  int min_args;
  int max_args;
  OperandMask arg_types[MAX_OPERANDS];
  OperandMask variable_type;
} DirectiveFormat;

/**
 * @brief A lookup table mapping all Directive values to their formats
 */
static const DirectiveFormat DIRECTIVE_FORMATS[] = {
    {DIRECTIVE_TEXT, 0, 0, {0}, 0},
    {DIRECTIVE_DATA, 0, 0, {0}, 0},
    {DIRECTIVE_BSS, 0, 0, {0}, 0},
    {DIRECTIVE_RODATA, 0, 0, {0}, 0},
    {DIRECTIVE_BYTE, 1, VARIABLE_ARGS, {OPERAND_IMMEDIATE | OPERAND_STRING}, OPERAND_IMMEDIATE | OPERAND_STRING},
    {DIRECTIVE_HALF, 1, VARIABLE_ARGS, {OPERAND_IMMEDIATE}, OPERAND_IMMEDIATE},
    {DIRECTIVE_WORD, 1, VARIABLE_ARGS, {OPERAND_IMMEDIATE}, OPERAND_IMMEDIATE},
    {DIRECTIVE_STRING, 1, 1, {OPERAND_STRING}, 0},
    {DIRECTIVE_ASCII, 1, 1, {OPERAND_STRING}, 0},
    {DIRECTIVE_ZERO, 1, 1, {OPERAND_IMMEDIATE}, 0},
    {DIRECTIVE_EQU, 2, 2, {OPERAND_SYMBOL, OPERAND_IMMEDIATE_LITERAL}, 0},
    {DIRECTIVE_ALIGN, 1, 2, {OPERAND_IMMEDIATE, OPERAND_IMMEDIATE}, 0},
};

/**
 * @brief A lookup mapping for a single PseudoOpcode to its operand counts and
 * accepted values
 */
typedef struct {
  PseudoOpcode pseudo;
  int operand_count;
  OperandMask operands[MAX_OPERANDS];
} PseudoInstructionFormat;

/**
 * @brief A lookup table mapping all PseudoOpcode values to their formats
 */
static const PseudoInstructionFormat PSEUDO_INSTRUCTION_FORMATS[] = {
    // No operands
    {PSEUDO_NOP, 0, {0}},
    {PSEUDO_RET, 0, {0}},

    // Single register: rd, rs
    {PSEUDO_MOV, 2, {OPERAND_REGISTER, OPERAND_REGISTER}},
    {PSEUDO_NOT, 2, {OPERAND_REGISTER, OPERAND_REGISTER}},
    {PSEUDO_NEG, 2, {OPERAND_REGISTER, OPERAND_REGISTER}},

    // Comparisons against zero: rd, rs
    {PSEUDO_SEQZ, 2, {OPERAND_REGISTER, OPERAND_REGISTER}},
    {PSEUDO_SNEZ, 2, {OPERAND_REGISTER, OPERAND_REGISTER}},
    {PSEUDO_SLTZ, 2, {OPERAND_REGISTER, OPERAND_REGISTER}},
    {PSEUDO_SGTZ, 2, {OPERAND_REGISTER, OPERAND_REGISTER}},

    // Branches against zero: rs, label (target must be a code address, not a
    // value-kind symbol)
    {PSEUDO_BEQZ, 2, {OPERAND_REGISTER, OPERAND_SYMBOL}},
    {PSEUDO_BNEZ, 2, {OPERAND_REGISTER, OPERAND_SYMBOL}},
    {PSEUDO_BLEZ, 2, {OPERAND_REGISTER, OPERAND_SYMBOL}},
    {PSEUDO_BGEZ, 2, {OPERAND_REGISTER, OPERAND_SYMBOL}},
    {PSEUDO_BLTZ, 2, {OPERAND_REGISTER, OPERAND_SYMBOL}},
    {PSEUDO_BGTZ, 2, {OPERAND_REGISTER, OPERAND_SYMBOL}},

    // Branch synonyms (operand-swapped): rs, rt, label
    {PSEUDO_BGT, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_SYMBOL}},
    {PSEUDO_BLE, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_SYMBOL}},
    {PSEUDO_BGTU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_SYMBOL}},
    {PSEUDO_BLEU, 3, {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_SYMBOL}},

    // Jumps: label-only forms (always a code address)
    {PSEUDO_J, 1, {OPERAND_SYMBOL}},
    {PSEUDO_CALL, 1, {OPERAND_SYMBOL}},
    {PSEUDO_TAIL, 1, {OPERAND_SYMBOL}},
    {PSEUDO_JAL, 1, {OPERAND_SYMBOL}}, // jal label  ==  jal ra, label

    // jr/jalr: register-only forms (implied offset 0)
    {PSEUDO_JR, 1, {OPERAND_REGISTER}},
    {PSEUDO_JALR, 1, {OPERAND_REGISTER}},

    // li: rd, imm
    {PSEUDO_LI, 2, {OPERAND_REGISTER, OPERAND_IMMEDIATE}},

    // la/lla: rd, symbol
    {PSEUDO_LA, 2, {OPERAND_REGISTER, OPERAND_SYMBOL}},
    {PSEUDO_LLA, 2, {OPERAND_REGISTER, OPERAND_SYMBOL}},
};

/**
 * @brief Returns the correct format for an instruction based on a token
 *
 * @param token A pointer to the token
 * @return const InstructionFormat* The instruction format
 */
static const InstructionFormat* get_instruction_format(Token* token) {
  if (token->type != TOKEN_MNEMONIC && token->type != TOKEN_INSTRUCTION_AMBIGUOUS)
    return NULL;
  for (size_t i = 0; i < sizeof(INSTRUCTION_FORMATS) / sizeof(INSTRUCTION_FORMATS[0]); i++) {
    if (INSTRUCTION_FORMATS[i].op == token->opcode.op)
      return &INSTRUCTION_FORMATS[i];
  }

  return NULL;
}

/**
 * @brief Returns the correct format for a directive based on a token
 *
 * @param token A pointer to the token
 * @return const DirectiveFormat* The directive format
 */
static const DirectiveFormat* get_directive_format(Token* token) {
  if (token->type != TOKEN_DIRECTIVE)
    return NULL;
  for (size_t i = 0; i < sizeof(DIRECTIVE_FORMATS) / sizeof(DIRECTIVE_FORMATS[0]); i++) {
    if (DIRECTIVE_FORMATS[i].directive == token->dir)
      return &DIRECTIVE_FORMATS[i];
  }

  return NULL;
}

/**
 * @brief Returns the correct format for a pseudo instruction based on a token
 *
 * @param token A pointer to the token
 * @return const PseudoInstructionFormat* The pseudo instruction format
 */
static const PseudoInstructionFormat* get_pseudo_format(Token* token) {
  if (token->type != TOKEN_PSEUDO && token->type != TOKEN_INSTRUCTION_AMBIGUOUS)
    return NULL;
  for (size_t i = 0; i < sizeof(PSEUDO_INSTRUCTION_FORMATS) / sizeof(PSEUDO_INSTRUCTION_FORMATS[0]); i++) {
    if (PSEUDO_INSTRUCTION_FORMATS[i].pseudo == token->opcode.pseudo)
      return &PSEUDO_INSTRUCTION_FORMATS[i];
  }

  return NULL;
}

/**
 * @brief Converts a token type into a human-readable string, for error messages
 *
 * @param tok The token type
 * @return const char* A human-readable string describing the token type
 */
static const char* token_type_to_string(TokenType tok) {
  switch (tok) {
  case TOKEN_UNKNOWN:
    return "unknown";
  case TOKEN_LABEL:
    return "label";
  case TOKEN_SYMBOL:
    return "symbol";
  case TOKEN_MNEMONIC:
  case TOKEN_PSEUDO:
  case TOKEN_INSTRUCTION_AMBIGUOUS:
    return "instruction";
  case TOKEN_REGISTER:
    return "register";
  case TOKEN_IMMEDIATE:
    return "immediate";
  case TOKEN_COMMA:
    return "','";
  case TOKEN_LPARAN:
    return "'('";
  case TOKEN_RPARAN:
    return "')'";
  case TOKEN_DIRECTIVE:
    return "directive";
  case TOKEN_STRING:
    return "string";
  }

  return "";
}

/**
 * @brief Converts an operand mask into a human-readable, pipe-separated string, for error messages
 *
 * @param mask The operand mask
 * @return const char* A human-readable string describing the accepted operand types, "none" if
 * empty. Backed by a static buffer, overwritten on each call
 */
static const char* operand_mask_to_string(OperandMask mask) {
  static char buf[128];
  buf[0] = '\0';

  static const struct {
    OperandMask bit;
    const char* name;
  } bits[] = {
      {OPERAND_REGISTER, "register"}, {OPERAND_IMMEDIATE_LITERAL, "immediate"},
      {OPERAND_SYMBOL, "symbol"},     {OPERAND_MEMORY, "memory"},
      {OPERAND_STRING, "string"},
  };

  for (int i = 0; i < 5; i++) {
    if (mask & bits[i].bit) {
      if (buf[0])
        strncat(buf, "|", sizeof(buf) - strlen(buf) - 1);
      strncat(buf, bits[i].name, sizeof(buf) - strlen(buf) - 1);
    }
  }

  return buf[0] ? buf : "none";
}

/**
 * @brief Attempts to parse a line of tokens according to the expected types,
 * writing the results to dest
 *
 * @param arena The arena to allocate any owned symbol text into
 * @param curr The token to start consuming at
 * @param operand_count The number of operands expected
 * @param expected_types An array of operand masks corresponding to accepted
 * operand types for each operand
 * @param dest Where to write the result
 * @param print_error_trace Whether to log a detailed error trace on mismatch
 * @return int 1 if successful, 0 if there was a mismatch or an error occurred
 */
static int try_parse_operands(Arena* arena, Token* curr, int operand_count, const OperandMask* expected_types,
                              ParsedLine* dest, int print_error_trace) {
#define LOC(tok) ((tok) ? (tok)->line : prev ? prev->line : 0), ((tok) ? (tok)->col : prev ? prev->col : 0)
#define TRACE(fmt, ...)                                                                                                \
  do {                                                                                                                 \
    if (print_error_trace)                                                                                             \
      LOG_ERROR_LOCATION(fmt, LOC(curr), ##__VA_ARGS__);                                                               \
  } while (0)

  Token* prev = NULL;

  int i = 0;
  for (i = 0; i < operand_count; i++) {
    if (!curr) {
      TRACE("Too few operands, expected %d, got %d", operand_count, i);
      return 0;
    }

    const OperandMask* expected = &expected_types[i];

    if ((*expected & OPERAND_REGISTER) && curr->type == TOKEN_REGISTER) {
      dest->instruction.operands[i].type = OPERAND_REGISTER;
      dest->instruction.operands[i].reg.reg = curr->reg;
    } else if ((*expected & OPERAND_IMMEDIATE_LITERAL) && curr->type == TOKEN_IMMEDIATE) {
      dest->instruction.operands[i].type = OPERAND_IMMEDIATE_LITERAL;
      dest->instruction.operands[i].imm.imm = curr->imm;
    } else if ((*expected & OPERAND_SYMBOL) && curr->type == TOKEN_SYMBOL) {
      dest->instruction.operands[i].type = OPERAND_SYMBOL;
      char* owned = arena_alloc(arena, curr->start, curr->len);
      if (!owned)
        return 0;
      dest->instruction.operands[i].label.label = owned;
      dest->instruction.operands[i].label.len = curr->len;
    } else if ((*expected & OPERAND_MEMORY) && curr->type == TOKEN_IMMEDIATE) {
      dest->instruction.operands[i].type = OPERAND_MEMORY;
      dest->instruction.operands[i].mem.offset = curr->imm;
      prev = curr;
      curr = curr->next;
      if (!curr || curr->type != TOKEN_LPARAN) {
        TRACE("Expected '(' after memory offset for operand %d, got %s", i + 1,
              curr ? token_type_to_string(curr->type) : "none");
        return 0;
      }
      prev = curr;
      curr = curr->next;
      if (!curr || curr->type != TOKEN_REGISTER) {
        TRACE("Expected register inside memory operand for operand %d, "
              "got %s",
              i + 1, curr ? token_type_to_string(curr->type) : "none");
        return 0;
      }
      dest->instruction.operands[i].mem.base_reg = curr->reg;
      prev = curr;
      curr = curr->next;
      if (!curr || curr->type != TOKEN_RPARAN) {
        TRACE("Expected ')' after register for operand %d, got %s", i + 1,
              curr ? token_type_to_string(curr->type) : "none");
        return 0;
      }
    } else {
      TRACE("Expected %s operand(s) for operand %d, got %s", operand_mask_to_string(*expected), i + 1,
            token_type_to_string(curr->type));
      return 0;
    }

    prev = curr;
    curr = curr->next;

    if (i < operand_count - 1) {
      if (!curr || curr->type != TOKEN_COMMA) {
        TRACE("Expected ',' after operand %d, got %s", i + 1, curr ? token_type_to_string(curr->type) : "none");
        return 0;
      }
      prev = curr;
      curr = curr->next;
    }
  }

  if (curr) {
    TRACE("Too many operands, expected %d, got %d", operand_count, i);
    return 0;
  }

#undef LOC
#undef TRACE

  dest->instruction.operand_count = operand_count;
  return 1;
}

/**
 * @brief Parses a mnemonic token sequence
 *
 * @param arena The arena to allocate any owned symbol text into
 * @param curr_token A pointer to the current token being parsed (will be
 * mutated since this function consumes tokens)
 * @param dest Where to write the parsed data
 * @return int 1 if success, 0 if the operation failed
 */
static int handle_mnemonic_token(Arena* arena, Token* curr_token, ParsedLine* dest) {
  if (!curr_token || !dest)
    return 0;

  const InstructionFormat* ins = get_instruction_format(curr_token);
  const PseudoInstructionFormat* ins_pseudo = get_pseudo_format(curr_token);

  if (!ins && !ins_pseudo) {
    LOG_ERROR_LOCATION("Invalid instruction", curr_token->line, curr_token->col);
    return 0;
  }

  dest->type = LINE_INSTRUCTION;
  Token* operand_start = curr_token->next;

  if (ins) {
    if (try_parse_operands(arena, operand_start, ins->operand_count, ins->operands, dest, 0)) {
      dest->instruction.op = ins->op;
      dest->instruction.type = INSTRUCTION_REAL;
      return 1;
    }
  }

  if (ins_pseudo) {
    if (try_parse_operands(arena, operand_start, ins_pseudo->operand_count, ins_pseudo->operands, dest, 0)) {
      dest->instruction.pseudo = ins_pseudo->pseudo;
      dest->instruction.type = INSTRUCTION_PSEUDO;
      return 1;
    }
  }

  LOG_ERROR("Failed to parse instruction '%.*s':", (int)curr_token->len, curr_token->start);
  ParsedLine temp = {0};
  if (ins && ins_pseudo) {
    LOG_ERROR("As real:");
    try_parse_operands(arena, operand_start, ins->operand_count, ins->operands, &temp, 1);
    LOG_ERROR("As pseudo:");
    try_parse_operands(arena, operand_start, ins_pseudo->operand_count, ins_pseudo->operands, &temp, 1);
  } else if (ins) {
    try_parse_operands(arena, operand_start, ins->operand_count, ins->operands, &temp, 1);
  } else if (ins_pseudo) {
    try_parse_operands(arena, operand_start, ins_pseudo->operand_count, ins_pseudo->operands, &temp, 1);
  } else {
    LOG_ERROR("No matching instruction format found");
  }

  return 0;
}

/**
 * @brief Frees all memory associated with a directive arg linked list
 *
 * @param head The head of the arg list
 */
void free_directive_args(DirectiveArg* head) {
  DirectiveArg* curr = head;
  while (curr) {
    DirectiveArg* next = curr->next;
    free(curr);
    curr = next;
  }
}

/**
 * @brief Parses a directive token sequence
 *
 * @param arena The arena to allocate any owned symbol/string arg text into
 * @param curr_token A pointer to the current token being parsed (will be
 * mutated since this function consumes tokens)
 * @param dest Where to write the parsed data
 * @return int 1 if success, 0 if the operation failed
 */
static int handle_directive_token(Arena* arena, Token* curr_token, ParsedLine* dest) {
  if (!curr_token)
    return 0;

  const DirectiveFormat* dir = get_directive_format(curr_token);
  if (!dir) {
    LOG_ERROR_LOCATION("No matching directive format found", curr_token->line, curr_token->col);
    return 0;
  }

  dest->type = LINE_DIRECTIVE;
  dest->directive.directive = dir->directive;
  int arg_count = 0;

  DirectiveArg* arg_head = NULL;
  DirectiveArg* arg_curr = NULL;

  Token* arg_token = curr_token->next;
  int expect_comma = 0;
  int pending_arg_after_comma = 0;
  while (arg_token != NULL) {
    if (expect_comma) {
      if (arg_token->type != TOKEN_COMMA) {
        LOG_ERROR_LOCATION("Expected ',' between directive args", arg_token->line, arg_token->col);
        free_directive_args(arg_head);
        return 0;
      }
      arg_token = arg_token->next;
      expect_comma = 0;
      pending_arg_after_comma = 1;
      continue;
    }
    pending_arg_after_comma = 0;

    if (dir->max_args != VARIABLE_ARGS && arg_count >= dir->max_args) {
      LOG_ERROR_LOCATION("Too many args, expected max %d, got %d", arg_token->line, arg_token->col, dir->max_args,
                         arg_count);
      free_directive_args(arg_head);
      return 0;
    }

    OperandMask accepted =
        dir->max_args == VARIABLE_ARGS ? dir->variable_type : dir->arg_types[arg_count] | dir->variable_type;

    if (!arg_curr) {
      arg_head = calloc(1, sizeof(DirectiveArg));
      if (!arg_head)
        return 0;
      arg_curr = arg_head;
    } else {
      DirectiveArg* tmp = calloc(1, sizeof(DirectiveArg));
      if (!tmp) {
        free_directive_args(arg_head);
        return 0;
      }
      arg_curr->next = tmp;
      arg_curr = tmp;
    }

    if ((OPERAND_IMMEDIATE & accepted) != 0 && arg_token->type == TOKEN_IMMEDIATE) {
      arg_curr->type = OPERAND_IMMEDIATE;
      arg_curr->imm = arg_token->imm;
      arg_curr->next = NULL;
    } else if ((OPERAND_SYMBOL & accepted) != 0 && arg_token->type == TOKEN_SYMBOL) {
      arg_curr->type = OPERAND_SYMBOL;
      arg_curr->start = arena_alloc(arena, arg_token->start, arg_token->len);
      if (!arg_curr->start) {
        free_directive_args(arg_head);
        return 0;
      }
      arg_curr->len = arg_token->len;
      arg_curr->next = NULL;
    } else if ((OPERAND_STRING & accepted) != 0 && arg_token->type == TOKEN_STRING) {
      arg_curr->type = OPERAND_STRING;
      arg_curr->start = arena_alloc(arena, arg_token->start, arg_token->len);
      if (!arg_curr->start) {
        free_directive_args(arg_head);
        return 0;
      }
      arg_curr->len = arg_token->len;
      arg_curr->next = NULL;
    } else {
      LOG_ERROR_LOCATION("Expected %s arg(s), got %s", arg_token->line, arg_token->col,
                         operand_mask_to_string(accepted), token_type_to_string(arg_token->type));
      free_directive_args(arg_head);
      return 0;
    }

    arg_token = arg_token->next;
    arg_count++;
    expect_comma = 1;
  }

  if (pending_arg_after_comma) {
    LOG_ERROR_LOCATION("Expected arg after comma, got end of line", curr_token->line, curr_token->col);
    free_directive_args(arg_head);
    return 0;
  }

  if (arg_count < dir->min_args) {
    LOG_ERROR_LOCATION("Too few args, expected min %d, got %d", curr_token->line, curr_token->col, dir->min_args,
                       arg_count);
    free_directive_args(arg_head);
    return 0;
  }

  dest->directive.arg_count = arg_count;
  dest->directive.args = arg_head;

  return 1;
}

/**
 * @brief Parses the tokens for a line into a more structured ParsedLine struct,
 * and finds any syntactical errors
 *
 * @param arena The arena to allocate any owned text into
 * @param line_tokens The tokens for the line
 * @param dest Where to write the parsed data
 * @return int 1 if success, 0 if the operation failed
 */
static int parse_tokenized_line(Arena* arena, Token* line_tokens, ParsedLine* dest) {
  if (!dest || !line_tokens)
    return 0;

  memset(dest, 0, sizeof(ParsedLine));
  dest->line = line_tokens->line;

  Token* curr_token = line_tokens;

  if (curr_token->type == TOKEN_LABEL) {
    dest->label = arena_alloc(arena, curr_token->start, curr_token->len);
    if (!dest->label)
      return 0;
    dest->len = curr_token->len;
    if (!curr_token->next) {
      dest->type = LINE_LABEL;
      return 1;
    }
    curr_token = curr_token->next;
  }

  switch (curr_token->type) {
  case TOKEN_MNEMONIC:
  case TOKEN_PSEUDO:
  case TOKEN_INSTRUCTION_AMBIGUOUS:
    if (!handle_mnemonic_token(arena, curr_token, dest))
      return 0;
    break;
  case TOKEN_DIRECTIVE:
    if (!handle_directive_token(arena, curr_token, dest))
      return 0;
    break;
  default:
    LOG_ERROR_LOCATION("Unexpected token '%.*s'", curr_token->line, curr_token->col, (int)curr_token->len,
                       curr_token->start);
    return 0;
  }

  return 1;
}

ParsedInput* parse_tokenized_input(TokenizedInput* tokenized) {
  ParsedInput* result = malloc(sizeof(ParsedInput));
  if (!result)
    return NULL;

  if (!arena_init(&result->arena, tokenized->arena.used)) {
    free(result);
    return NULL;
  }

  DynamicArray parsed_lines = {0};
  if (!dynamic_array_init(&parsed_lines, sizeof(ParsedLine), 256)) {
    arena_free(&result->arena);
    free(result);
    return NULL;
  }

  for (size_t index = 0; index < tokenized->count; index++) {
    Token* line = tokenized->lines[index];
    ParsedLine* parsed = (ParsedLine*)dynamic_array_emplace(&parsed_lines);
    if (!parsed || parse_tokenized_line(&result->arena, line, parsed) == 0) {
      result->lines = (ParsedLine*)parsed_lines.data;
      result->count = parsed_lines.count;
      free_parsed_input(result);
      return NULL;
    }
  }

  result->lines = (ParsedLine*)parsed_lines.data;
  result->count = parsed_lines.count;
  return result;
}

int copy_operand(Arena* arena, Operand* op) {
  if (!arena || !op)
    return 0;

  if (op->type != OPERAND_SYMBOL)
    return 1;

  char* owned = arena_alloc(arena, op->label.label, op->label.len);
  if (!owned)
    return 0;

  op->label.label = owned;
  return 1;
}

DirectiveArg* copy_directive_args(Arena* arena, const DirectiveArg* head) {
  if (!arena || !head)
    return NULL;

  DirectiveArg* new_head = NULL;
  DirectiveArg* new_tail = NULL;

  for (const DirectiveArg* curr = head; curr; curr = curr->next) {
    DirectiveArg* copy = calloc(1, sizeof(DirectiveArg));
    if (!copy) {
      free(copy);
      free_directive_args(new_head);
      return NULL;
    }
    copy->type = curr->type;
    copy->imm = curr->imm;
    copy->next = NULL;

    if (curr->type == OPERAND_SYMBOL || curr->type == OPERAND_STRING) {
      copy->start = arena_alloc(arena, curr->start, curr->len);
      if (!copy->start) {
        free(copy);
        free_directive_args(new_head);
        return NULL;
      }
      copy->len = curr->len;
    }

    if (!new_head) {
      new_head = copy;
      new_tail = copy;
    } else {
      new_tail->next = copy;
      new_tail = copy;
    }
  }

  return new_head;
}

int copy_parsed_line(Arena* arena, const ParsedLine* src, ParsedLine* dest) {
  if (!arena || !src || !dest)
    return 0;

  *dest = *src;

  if (src->label) {
    dest->label = arena_alloc(arena, src->label, src->len);
    if (!dest->label)
      return 0;
  }

  if (src->type == LINE_INSTRUCTION) {
    for (int i = 0; i < dest->instruction.operand_count; i++)
      if (!copy_operand(arena, &dest->instruction.operands[i]))
        return 0;
  } else if (src->type == LINE_DIRECTIVE) {
    if (src->directive.args) {
      dest->directive.args = copy_directive_args(arena, src->directive.args);
      if (!dest->directive.args)
        return 0;
    }
  }

  return 1;
}

void free_parsed_input(ParsedInput* input) {
  if (!input)
    return;

  for (size_t i = 0; i < input->count; i++)
    if (input->lines[i].type == LINE_DIRECTIVE)
      free_directive_args(input->lines[i].directive.args);

  free(input->lines);
  arena_free(&input->arena);
  free(input);
}