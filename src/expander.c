#include "../include/expander.h"
#include "../include/dynamic_array.h"
#include <stddef.h>
#include <stdlib.h>

#define MAX_EXPANSION_LENGTH 2

/**
 * @brief Identifies where a single expanded instruction's operand value comes from
 */
typedef enum : uint8_t {
  // Registers
  EXPAND_REG_ZERO, // Register x0
  EXPAND_REG_RA,   // Register x1
  EXPAND_REG_T1,   // Register x6

  // Position operands
  EXPAND_OP0, // operands[0]
  EXPAND_OP1, // operands[1]
  EXPAND_OP2, // operands[2]

  // Immediate constants
  EXPAND_IMM_0,    // 0
  EXPAND_IMM_1,    // 1
  EXPAND_IMM_NEG1, // -1

  EXPAND_LI_HI,
  EXPAND_LI_LO,
} ExpandSource;

/**
 * @brief Describes one real instruction produced by expanding a pseudoinstruction, as an opcode
 * plus the source of each operand
 */
typedef struct {
  Opcode op;
  int operand_count;
  ExpandSource sources[MAX_OPERANDS];
} ExpansionInstruction;

/**
 * @brief Describes the full expansion of a single pseudoinstruction into one or more real
 * instructions
 */
typedef struct {
  int instruction_count;
  ExpansionInstruction instructions[MAX_EXPANSION_LENGTH];
} PseudoExpansion;

/**
 * @brief A lookup table mapping each PseudoOpcode to its expansion into real instructions
 */
static const PseudoExpansion PSEUDO_EXPANSIONS[] = {
    [PSEUDO_UNKNOWN] = {0},

    // nop -> addi x0, x0, 0
    [PSEUDO_NOP] = {1,
                    {
                        {OP_ADDI, 3, {EXPAND_REG_ZERO, EXPAND_REG_ZERO, EXPAND_IMM_0}},
                    }},

    // mov rd, rs -> addi rd, rs, 0
    [PSEUDO_MOV] = {1,
                    {
                        {OP_ADDI, 3, {EXPAND_OP0, EXPAND_OP1, EXPAND_IMM_0}},
                    }},

    // not rd, rs -> xori rd, rs, -1
    [PSEUDO_NOT] = {1,
                    {
                        {OP_XORI, 3, {EXPAND_OP0, EXPAND_OP1, EXPAND_IMM_NEG1}},
                    }},

    // neg rd, rs -> sub rd, x0, rs
    [PSEUDO_NEG] = {1,
                    {
                        {OP_SUB, 3, {EXPAND_OP0, EXPAND_REG_ZERO, EXPAND_OP1}},
                    }},

    // seqz rd, rs -> sltiu rd, rs, 1
    [PSEUDO_SEQZ] = {1,
                     {
                         {OP_SLTIU, 3, {EXPAND_OP0, EXPAND_OP1, EXPAND_IMM_1}},
                     }},

    // snez rd, rs -> sltu rd, x0, rs
    [PSEUDO_SNEZ] = {1,
                     {
                         {OP_SLTU, 3, {EXPAND_OP0, EXPAND_REG_ZERO, EXPAND_OP1}},
                     }},

    // sltz rd, rs -> slt rd, rs, x0
    [PSEUDO_SLTZ] = {1,
                     {
                         {OP_SLT, 3, {EXPAND_OP0, EXPAND_OP1, EXPAND_REG_ZERO}},
                     }},

    // sgtz rd, rs -> slt rd, x0, rs
    [PSEUDO_SGTZ] = {1,
                     {
                         {OP_SLT, 3, {EXPAND_OP0, EXPAND_REG_ZERO, EXPAND_OP1}},
                     }},

    // beqz rs, label -> beq rs, x0, label
    [PSEUDO_BEQZ] = {1,
                     {
                         {OP_BEQ, 3, {EXPAND_OP0, EXPAND_REG_ZERO, EXPAND_OP1}},
                     }},

    // bnez rs, label -> bne rs, x0, label
    [PSEUDO_BNEZ] = {1,
                     {
                         {OP_BNE, 3, {EXPAND_OP0, EXPAND_REG_ZERO, EXPAND_OP1}},
                     }},

    // blez rs, label -> bge x0, rs, label
    [PSEUDO_BLEZ] = {1,
                     {
                         {OP_BGE, 3, {EXPAND_REG_ZERO, EXPAND_OP0, EXPAND_OP1}},
                     }},

    // bgez rs, label -> bge rs, x0, label
    [PSEUDO_BGEZ] = {1,
                     {
                         {OP_BGE, 3, {EXPAND_OP0, EXPAND_REG_ZERO, EXPAND_OP1}},
                     }},

    // bltz rs, label -> blt rs, x0, label
    [PSEUDO_BLTZ] = {1,
                     {
                         {OP_BLT, 3, {EXPAND_OP0, EXPAND_REG_ZERO, EXPAND_OP1}},
                     }},

    // bgtz rs, label -> blt x0, rs, label
    [PSEUDO_BGTZ] = {1,
                     {
                         {OP_BLT, 3, {EXPAND_REG_ZERO, EXPAND_OP0, EXPAND_OP1}},
                     }},

    // bgt rs, rt, label -> blt rt, rs, label
    [PSEUDO_BGT] = {1,
                    {
                        {OP_BLT, 3, {EXPAND_OP1, EXPAND_OP0, EXPAND_OP2}},
                    }},

    // ble rs, rt, label -> bge rt, rs, label
    [PSEUDO_BLE] = {1,
                    {
                        {OP_BGE, 3, {EXPAND_OP1, EXPAND_OP0, EXPAND_OP2}},
                    }},

    // bgtu rs, rt, label -> bltu rt, rs, label
    [PSEUDO_BGTU] = {1,
                     {
                         {OP_BLTU, 3, {EXPAND_OP1, EXPAND_OP0, EXPAND_OP2}},
                     }},

    // bleu rs, rt, label -> bgeu rt, rs, label
    [PSEUDO_BLEU] = {1,
                     {
                         {OP_BGEU, 3, {EXPAND_OP1, EXPAND_OP0, EXPAND_OP2}},
                     }},

    // j label -> jal x0, label
    [PSEUDO_J] = {1,
                  {
                      {OP_JAL, 2, {EXPAND_REG_ZERO, EXPAND_OP0}},
                  }},

    // jal label -> jal ra, label
    [PSEUDO_JAL] = {1,
                    {
                        {OP_JAL, 2, {EXPAND_REG_RA, EXPAND_OP0}},
                    }},

    // jr rs -> jalr x0, rs, 0
    [PSEUDO_JR] = {1,
                   {
                       {OP_JALR, 3, {EXPAND_REG_ZERO, EXPAND_OP0, EXPAND_IMM_0}},
                   }},

    // jalr rs -> jalr ra, rs, 0
    [PSEUDO_JALR] = {1,
                     {
                         {OP_JALR, 3, {EXPAND_REG_RA, EXPAND_OP0, EXPAND_IMM_0}},
                     }},

    // ret -> jalr x0, ra, 0
    [PSEUDO_RET] = {1,
                    {
                        {OP_JALR, 3, {EXPAND_REG_ZERO, EXPAND_REG_RA, EXPAND_IMM_0}},
                    }},

    // call label -> auipc ra, symbol_hi / jalr ra, ra, symbol_lo
    [PSEUDO_CALL] = {2,
                     {
                         {OP_AUIPC, 2, {EXPAND_REG_RA, EXPAND_OP0}},
                         {OP_JALR, 3, {EXPAND_REG_RA, EXPAND_REG_RA, EXPAND_OP0}},
                     }},

    // tail label -> auipc t1, symbol_hi / jalr x0, t1, symbol_lo
    [PSEUDO_TAIL] = {2,
                     {
                         {OP_AUIPC, 2, {EXPAND_REG_T1, EXPAND_OP0}},
                         {OP_JALR, 3, {EXPAND_REG_ZERO, EXPAND_REG_T1, EXPAND_OP0}},
                     }},

    // li rd, imm -> lui rd, imm_hi / addi rd, rd, imm_lo
    [PSEUDO_LI] = {2,
                   {
                       {OP_LUI, 2, {EXPAND_OP0, EXPAND_LI_HI}},
                       {OP_ADDI, 3, {EXPAND_OP0, EXPAND_OP0, EXPAND_LI_LO}},
                   }},

    // la rd, symbol -> auipc rd, symbol_hi / addi rd, rd, symbol_lo
    [PSEUDO_LA] = {2,
                   {
                       {OP_AUIPC, 2, {EXPAND_OP0, EXPAND_OP1}},
                       {OP_ADDI, 3, {EXPAND_OP0, EXPAND_OP0, EXPAND_OP1}},
                   }},

    // lla rd, symbol -> auipc rd, symbol_hi / addi rd, rd, symbol_lo
    [PSEUDO_LLA] = {2,
                    {
                        {OP_AUIPC, 2, {EXPAND_OP0, EXPAND_OP1}},
                        {OP_ADDI, 3, {EXPAND_OP0, EXPAND_OP0, EXPAND_OP1}},
                    }},
};

/**
 * @brief Resolves a single expansion source into a concrete operand, using the original
 * pseudoinstruction's operands where referenced
 *
 * @param src The expansion source to resolve
 * @param original The original pseudoinstruction being expanded
 * @return Operand The resolved operand
 */
static Operand resolve_source(ExpandSource src, const ParsedInstruction* original) {
  switch (src) {
  case EXPAND_REG_ZERO:
    return (Operand){.type = OPERAND_REGISTER, .reg.reg = 0};
  case EXPAND_REG_RA:
    return (Operand){.type = OPERAND_REGISTER, .reg.reg = 1};
  case EXPAND_REG_T1:
    return (Operand){.type = OPERAND_REGISTER, .reg.reg = 6};
  case EXPAND_OP0: {
    Operand op = original->operands[0];
    if (op.type == OPERAND_SYMBOL)
      op.label.pc_relative = 1;
    return op;
  }
  case EXPAND_OP1: {
    Operand op = original->operands[1];
    if (op.type == OPERAND_SYMBOL)
      op.label.pc_relative = 1;
    return op;
  }
  case EXPAND_OP2:
    Operand op = original->operands[2];
    if (op.type == OPERAND_SYMBOL)
      op.label.pc_relative = 1;
    return op;
  case EXPAND_IMM_0:
    return (Operand){.type = OPERAND_IMMEDIATE_LITERAL, .imm.imm = 0};
  case EXPAND_IMM_1:
    return (Operand){.type = OPERAND_IMMEDIATE_LITERAL, .imm.imm = 1};
  case EXPAND_IMM_NEG1:
    return (Operand){.type = OPERAND_IMMEDIATE_LITERAL, .imm.imm = -1};
  case EXPAND_LI_HI: {
    Operand op = original->operands[1];
    if (op.type == OPERAND_SYMBOL) {
      op.label.pc_relative = 0;
      return op;
    }
    int imm1 = op.imm.imm;
    return (Operand){.type = OPERAND_IMMEDIATE_LITERAL, .imm.imm = (imm1 + 0x800) >> 12};
  }
  case EXPAND_LI_LO: {
    Operand op = original->operands[1];
    if (op.type == OPERAND_SYMBOL) {
      op.label.pc_relative = 0;
      return op;
    }
    int imm2 = op.imm.imm;
    int imm2_hi = (imm2 + 0x800) >> 12;
    return (Operand){.type = OPERAND_IMMEDIATE_LITERAL, .imm.imm = imm2 - (imm2_hi << 12)};
  }
  default:
    return (Operand){0};
  }
}

/**
 * @brief Looks up the expansion definition for a pseudoinstruction opcode
 *
 * @param pseudo The pseudoinstruction opcode to look up
 * @return const PseudoExpansion* The matching expansion, NULL if pseudo is out of range or invalid
 */
static const PseudoExpansion* find_expansion(PseudoOpcode pseudo) {
  if (pseudo <= PSEUDO_UNKNOWN || pseudo >= (PseudoOpcode)(sizeof(PSEUDO_EXPANSIONS) / sizeof(PSEUDO_EXPANSIONS[0])))
    return NULL;
  return &PSEUDO_EXPANSIONS[pseudo];
}

/**
 * @brief Expands a pseudoinstruction into its real-instruction equivalents using a matched
 * expansion template
 *
 * @param expansion The expansion template to apply
 * @param original The original pseudoinstruction being expanded
 * @param out Where to write the resulting real instructions
 * @param out_count Where to write the number of instructions written to out
 * @return int 1 if successful, 0 if any input pointer was NULL
 */
static int expand_pseudo(const PseudoExpansion* expansion, const ParsedInstruction* original, ParsedInstruction* out,
                         int* out_count) {
  if (!expansion || !original || !out || !out_count)
    return 0;
  for (int i = 0; i < expansion->instruction_count; i++) {
    const ExpansionInstruction* template = &expansion->instructions[i];
    out[i].type = INSTRUCTION_REAL;
    out[i].op = template->op;
    out[i].operand_count = template->operand_count;

    for (int j = 0; j < template->operand_count; j++) {
      out[i].operands[j] = resolve_source(template->sources[j], original);
    }
  }

  *out_count = expansion->instruction_count;
  return 1;
}

ExpandedInput* expand_pseudoinstructions(ParsedInput* parsed) {
  if (!parsed)
    return NULL;

  ExpandedInput* result = malloc(sizeof(ExpandedInput));
  if (!result)
    return NULL;

  if (!arena_init(&result->arena, parsed->arena.used * MAX_EXPANSION_LENGTH)) {
    free(result);
    return NULL;
  }

  if (!parsed->count) {
    result->lines = NULL;
    result->count = 0;
    return result;
  }

  DynamicArray expanded = {0};
  if (!dynamic_array_init(&expanded, sizeof(ParsedLine), parsed->count * 2))
    goto fail_arena;

  for (size_t i = 0; i < parsed->count; i++) {
    ParsedLine* line = &parsed->lines[i];

    if (line->type != LINE_INSTRUCTION || line->instruction.type != INSTRUCTION_PSEUDO) {
      ParsedLine copy;
      if (!copy_parsed_line(&result->arena, line, &copy))
        goto fail;
      if (!dynamic_array_push(&expanded, &copy))
        goto fail;
      continue;
    }

    const PseudoExpansion* expansion = find_expansion(line->instruction.pseudo);
    if (!expansion)
      goto fail;

    ParsedInstruction out_instructions[MAX_EXPANSION_LENGTH];
    int out_count = 0;
    if (!expand_pseudo(expansion, &line->instruction, out_instructions, &out_count))
      goto fail;

    for (int j = 0; j < out_count; j++) {
      ParsedLine out_line = {
          .type = LINE_INSTRUCTION,
          .line = line->line,
          .label = (j == 0) ? line->label : NULL,
          .len = (j == 0) ? line->len : 0,
          .instruction = out_instructions[j],
      };
      ParsedLine copy;
      if (!copy_parsed_line(&result->arena, &out_line, &copy))
        goto fail;
      if (!dynamic_array_push(&expanded, &copy))
        goto fail;
    }
  }

  result->lines = (ParsedLine*)expanded.data;
  result->count = expanded.count;
  return result;

fail:
  for (size_t i = 0; i < expanded.count; i++) {
    ParsedLine* pl = &((ParsedLine*)expanded.data)[i];
    if (pl->type == LINE_DIRECTIVE)
      free_directive_args(pl->directive.args);
  }
  dynamic_array_free(&expanded);
fail_arena:
  arena_free(&result->arena);
  free(result);
  return NULL;
}

void free_expanded_input(ExpandedInput* expanded) {
  if (!expanded)
    return;

  for (size_t i = 0; i < expanded->count; i++)
    if (expanded->lines[i].type == LINE_DIRECTIVE)
      free_directive_args(expanded->lines[i].directive.args);

  free(expanded->lines);
  arena_free(&expanded->arena);
  free(expanded);
}