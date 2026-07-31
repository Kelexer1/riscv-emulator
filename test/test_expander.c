#define _POSIX_C_SOURCE 200809L

#include "../src/expander.c"
#include "unity/unity.h"
#include <string.h>

static Arena arena;

void setUp(void) { arena_init(&arena, 4096); }

void tearDown(void) { arena_free(&arena); }

static Operand make_reg_operand(int reg) {
  Operand op = {0};
  op.type = OPERAND_REGISTER;
  op.reg.reg = reg;
  return op;
}

static Operand make_imm_operand(int imm) {
  Operand op = {0};
  op.type = OPERAND_IMMEDIATE_LITERAL;
  op.imm.imm = imm;
  return op;
}

static Operand make_symbol_operand(const char* s) {
  Operand op = {0};
  op.type = OPERAND_SYMBOL;
  op.label.label = (char*)s;
  op.label.len = strlen(s);
  return op;
}

/* ---------------------------------------------------------------------
 * resolve_source
 * ------------------------------------------------------------------- */

void test_resolve_source_fixed_registers(void) {
  ParsedInstruction original = {0};

  Operand zero = resolve_source(EXPAND_REG_ZERO, &original);
  Operand ra = resolve_source(EXPAND_REG_RA, &original);
  Operand t1 = resolve_source(EXPAND_REG_T1, &original);

  TEST_ASSERT_EQUAL(OPERAND_REGISTER, zero.type);
  TEST_ASSERT_EQUAL_INT(0, zero.reg.reg);
  TEST_ASSERT_EQUAL_INT(1, ra.reg.reg);
  TEST_ASSERT_EQUAL_INT(6, t1.reg.reg);
}

void test_resolve_source_operand_passthrough(void) {
  ParsedInstruction original = {0};
  original.operands[0] = make_reg_operand(5);
  original.operands[1] = make_reg_operand(7);
  original.operands[2] = make_imm_operand(42);

  TEST_ASSERT_EQUAL_INT(5, resolve_source(EXPAND_OP0, &original).reg.reg);
  TEST_ASSERT_EQUAL_INT(7, resolve_source(EXPAND_OP1, &original).reg.reg);
  TEST_ASSERT_EQUAL_INT(42, resolve_source(EXPAND_OP2, &original).imm.imm);
}

void test_resolve_source_fixed_immediates(void) {
  ParsedInstruction original = {0};

  TEST_ASSERT_EQUAL_INT(0, resolve_source(EXPAND_IMM_0, &original).imm.imm);
  TEST_ASSERT_EQUAL_INT(1, resolve_source(EXPAND_IMM_1, &original).imm.imm);
  TEST_ASSERT_EQUAL_INT(-1, resolve_source(EXPAND_IMM_NEG1, &original).imm.imm);
}

void test_resolve_source_hi_lo_reconstructs_original(void) {
  ParsedInstruction original = {0};
  int values[] = {4096, 0, 1, -1, 100000, -100000, 0x7FFFFFFF};

  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    original.operands[1] = make_imm_operand(values[i]);
    int hi = resolve_source(EXPAND_LI_HI, &original).imm.imm;
    int lo = resolve_source(EXPAND_LI_LO, &original).imm.imm;
    int reconstructed = (hi << 12) + lo;
    TEST_ASSERT_EQUAL_INT(values[i], reconstructed);
  }
}

void test_resolve_source_invalid_source_returns_zeroed(void) {
  ParsedInstruction original = {0};
  Operand result = resolve_source((ExpandSource)999, &original);
  TEST_ASSERT_EQUAL_INT(0, result.type);
}

/* ---------------------------------------------------------------------
 * resolve_source pc_relative tagging (symbols/labels/immediates
 * interchangeability methodology)
 * ------------------------------------------------------------------- */

void test_resolve_source_op0_symbol_marks_pc_relative(void) {
  /* EXPAND_OP0/OP1/OP2 mark any symbol operand pc_relative=1, this is
   * how la/call/tail/j/branch-pseudos all end up PC-relative, since they
   * all route their symbol operand through these generic sources. */
  ParsedInstruction original = {0};
  original.operands[0] = make_symbol_operand("mylabel");

  Operand result = resolve_source(EXPAND_OP0, &original);

  TEST_ASSERT_EQUAL(OPERAND_SYMBOL, result.type);
  TEST_ASSERT_EQUAL_INT(1, result.label.pc_relative);
  TEST_ASSERT_EQUAL_MEMORY("mylabel", result.label.label, 7);
}

void test_resolve_source_op0_non_symbol_unaffected(void) {
  ParsedInstruction original = {0};
  original.operands[0] = make_reg_operand(5);

  Operand result = resolve_source(EXPAND_OP0, &original);

  TEST_ASSERT_EQUAL(OPERAND_REGISTER, result.type);
  TEST_ASSERT_EQUAL_INT(5, result.reg.reg);
}

void test_resolve_source_op1_symbol_marks_pc_relative(void) {
  ParsedInstruction original = {0};
  original.operands[1] = make_symbol_operand("target");

  Operand result = resolve_source(EXPAND_OP1, &original);

  TEST_ASSERT_EQUAL(OPERAND_SYMBOL, result.type);
  TEST_ASSERT_EQUAL_INT(1, result.label.pc_relative);
}

void test_resolve_source_op2_symbol_marks_pc_relative(void) {
  ParsedInstruction original = {0};
  original.operands[2] = make_symbol_operand("target");

  Operand result = resolve_source(EXPAND_OP2, &original);

  TEST_ASSERT_EQUAL(OPERAND_SYMBOL, result.type);
  TEST_ASSERT_EQUAL_INT(1, result.label.pc_relative);
}

void test_resolve_source_li_hi_lo_symbol_forces_absolute(void) {
  ParsedInstruction original = {0};
  original.operands[1] = make_symbol_operand("t");
  original.operands[1].label.pc_relative = 1; /* pre-set on purpose, to prove li overrides it */

  Operand hi = resolve_source(EXPAND_LI_HI, &original);
  Operand lo = resolve_source(EXPAND_LI_LO, &original);

  TEST_ASSERT_EQUAL(OPERAND_SYMBOL, hi.type);
  TEST_ASSERT_EQUAL(OPERAND_SYMBOL, lo.type);
  TEST_ASSERT_EQUAL_INT(0, hi.label.pc_relative);
  TEST_ASSERT_EQUAL_INT(0, lo.label.pc_relative);
  TEST_ASSERT_EQUAL_MEMORY("t", hi.label.label, 1);
  TEST_ASSERT_EQUAL_MEMORY("t", lo.label.label, 1);
}

void test_copy_operand_preserves_pc_relative(void) {
  Operand op = make_symbol_operand("myfunc");
  op.label.pc_relative = 1;

  copy_operand(&arena, &op);

  TEST_ASSERT_EQUAL_INT(1, op.label.pc_relative);
}

/* ---------------------------------------------------------------------
 * find_expansion
 * ------------------------------------------------------------------- */

void test_find_expansion_unknown_returns_null(void) { TEST_ASSERT_NULL(find_expansion(PSEUDO_UNKNOWN)); }

void test_find_expansion_valid(void) {
  const PseudoExpansion* exp = find_expansion(PSEUDO_NOP);
  TEST_ASSERT_NOT_NULL(exp);
  TEST_ASSERT_EQUAL_INT(1, exp->instruction_count);
}

void test_find_expansion_two_instruction_pseudo(void) {
  const PseudoExpansion* exp = find_expansion(PSEUDO_LI);
  TEST_ASSERT_NOT_NULL(exp);
  TEST_ASSERT_EQUAL_INT(2, exp->instruction_count);
}

void test_find_expansion_out_of_range_returns_null(void) { TEST_ASSERT_NULL(find_expansion((PseudoOpcode)250)); }

/* ---------------------------------------------------------------------
 * expand_pseudo
 * ------------------------------------------------------------------- */

void test_expand_pseudo_null_args_fail(void) {
  ParsedInstruction original = {0};
  ParsedInstruction out[MAX_EXPANSION_LENGTH];
  int out_count = 0;

  TEST_ASSERT_EQUAL_INT(0, expand_pseudo(NULL, &original, out, &out_count));
  TEST_ASSERT_EQUAL_INT(0, expand_pseudo(find_expansion(PSEUDO_NOP), NULL, out, &out_count));
}

void test_expand_pseudo_nop(void) {
  ParsedInstruction original = {0};
  ParsedInstruction out[MAX_EXPANSION_LENGTH];
  int out_count = 0;

  int result = expand_pseudo(find_expansion(PSEUDO_NOP), &original, out, &out_count);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL_INT(1, out_count);
  TEST_ASSERT_EQUAL(OP_ADDI, out[0].op);
  TEST_ASSERT_EQUAL_INT(3, out[0].operand_count);
  TEST_ASSERT_EQUAL_INT(0, out[0].operands[0].reg.reg);
  TEST_ASSERT_EQUAL_INT(0, out[0].operands[1].reg.reg);
  TEST_ASSERT_EQUAL_INT(0, out[0].operands[2].imm.imm);
}

void test_expand_pseudo_mov(void) {
  ParsedInstruction original = {0};
  original.operands[0] = make_reg_operand(5);
  original.operands[1] = make_reg_operand(7);
  ParsedInstruction out[MAX_EXPANSION_LENGTH];
  int out_count = 0;

  expand_pseudo(find_expansion(PSEUDO_MOV), &original, out, &out_count);

  TEST_ASSERT_EQUAL(OP_ADDI, out[0].op);
  TEST_ASSERT_EQUAL_INT(5, out[0].operands[0].reg.reg);
  TEST_ASSERT_EQUAL_INT(7, out[0].operands[1].reg.reg);
  TEST_ASSERT_EQUAL_INT(0, out[0].operands[2].imm.imm);
}

void test_expand_pseudo_li_two_instructions(void) {
  ParsedInstruction original = {0};
  original.operands[0] = make_reg_operand(10);
  original.operands[1] = make_imm_operand(4096);
  ParsedInstruction out[MAX_EXPANSION_LENGTH];
  int out_count = 0;

  int result = expand_pseudo(find_expansion(PSEUDO_LI), &original, out, &out_count);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL_INT(2, out_count);
  TEST_ASSERT_EQUAL(OP_LUI, out[0].op);
  TEST_ASSERT_EQUAL_INT(10, out[0].operands[0].reg.reg);
  TEST_ASSERT_EQUAL(OP_ADDI, out[1].op);
  TEST_ASSERT_EQUAL_INT(10, out[1].operands[0].reg.reg);
  TEST_ASSERT_EQUAL_INT(10, out[1].operands[1].reg.reg);
  int reconstructed = (out[0].operands[1].imm.imm << 12) + out[1].operands[2].imm.imm;
  TEST_ASSERT_EQUAL_INT(4096, reconstructed);
}

/* ---------------------------------------------------------------------
 * expand_pseudoinstructions / free_expanded_input
 * ------------------------------------------------------------------- */

static ParsedInput make_parsed_input(ParsedLine* lines, size_t count) {
  ParsedInput input;
  arena_init(&input.arena, 512);
  input.arena.used = 128;
  input.lines = lines;
  input.count = count;
  return input;
}

void test_expand_pseudoinstructions_null_returns_null(void) { TEST_ASSERT_NULL(expand_pseudoinstructions(NULL)); }

void test_expand_pseudoinstructions_empty_input(void) {
  ParsedInput input = make_parsed_input(NULL, 0);

  ExpandedInput* result = expand_pseudoinstructions(&input);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(0, result->count);

  free_expanded_input(result);
  arena_free(&input.arena);
}

void test_expand_pseudoinstructions_real_instruction_passthrough(void) {
  ParsedLine lines[1] = {0};
  lines[0].type = LINE_INSTRUCTION;
  lines[0].instruction.type = INSTRUCTION_REAL;
  lines[0].instruction.op = OP_ADD;
  lines[0].instruction.operand_count = 3;
  lines[0].instruction.operands[0] = make_reg_operand(1);
  lines[0].instruction.operands[1] = make_reg_operand(2);
  lines[0].instruction.operands[2] = make_reg_operand(3);

  ParsedInput input = make_parsed_input(lines, 1);
  ExpandedInput* result = expand_pseudoinstructions(&input);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(1, result->count);
  TEST_ASSERT_EQUAL(OP_ADD, result->lines[0].instruction.op);

  free_expanded_input(result);
  arena_free(&input.arena);
}

void test_expand_pseudoinstructions_pseudo_expands_and_preserves_label_on_first_only(void) {
  ParsedLine lines[1] = {0};
  lines[0].type = LINE_INSTRUCTION;
  lines[0].label = "start";
  lines[0].len = 5;
  lines[0].instruction.type = INSTRUCTION_PSEUDO;
  lines[0].instruction.pseudo = PSEUDO_LI;
  lines[0].instruction.operand_count = 2;
  lines[0].instruction.operands[0] = make_reg_operand(10);
  lines[0].instruction.operands[1] = make_imm_operand(4096);

  ParsedInput input = make_parsed_input(lines, 1);
  ExpandedInput* result = expand_pseudoinstructions(&input);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(2, result->count);
  TEST_ASSERT_EQUAL(OP_LUI, result->lines[0].instruction.op);
  TEST_ASSERT_NOT_NULL(result->lines[0].label);
  TEST_ASSERT_EQUAL_MEMORY("start", result->lines[0].label, 5);
  TEST_ASSERT_EQUAL(OP_ADDI, result->lines[1].instruction.op);
  TEST_ASSERT_NULL(result->lines[1].label);

  free_expanded_input(result);
  arena_free(&input.arena);
}

void test_expand_pseudoinstructions_directive_passthrough(void) {
  DirectiveArg arg = {0};
  arg.type = OPERAND_IMMEDIATE;
  arg.imm = 8;

  ParsedLine lines[1] = {0};
  lines[0].type = LINE_DIRECTIVE;
  lines[0].directive.directive = DIRECTIVE_ZERO;
  lines[0].directive.arg_count = 1;
  lines[0].directive.args = &arg;

  ParsedInput input = make_parsed_input(lines, 1);
  ExpandedInput* result = expand_pseudoinstructions(&input);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(1, result->count);
  TEST_ASSERT_EQUAL(LINE_DIRECTIVE, result->lines[0].type);
  TEST_ASSERT_EQUAL_INT(8, result->lines[0].directive.args->imm);

  free_expanded_input(result); /* also frees the copied directive arg chain */
  arena_free(&input.arena);
}

void test_expand_pseudoinstructions_invalid_pseudo_fails(void) {
  ParsedLine lines[1] = {0};
  lines[0].type = LINE_INSTRUCTION;
  lines[0].instruction.type = INSTRUCTION_PSEUDO;
  lines[0].instruction.pseudo = PSEUDO_UNKNOWN;

  ParsedInput input = make_parsed_input(lines, 1);
  ExpandedInput* result = expand_pseudoinstructions(&input);

  TEST_ASSERT_NULL(result);
  arena_free(&input.arena);
}

void test_free_expanded_input_null_does_not_crash(void) {
  free_expanded_input(NULL);
  TEST_PASS();
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_resolve_source_fixed_registers);
  RUN_TEST(test_resolve_source_operand_passthrough);
  RUN_TEST(test_resolve_source_fixed_immediates);
  RUN_TEST(test_resolve_source_hi_lo_reconstructs_original);
  RUN_TEST(test_resolve_source_invalid_source_returns_zeroed);

  RUN_TEST(test_find_expansion_unknown_returns_null);
  RUN_TEST(test_find_expansion_valid);
  RUN_TEST(test_find_expansion_two_instruction_pseudo);
  RUN_TEST(test_find_expansion_out_of_range_returns_null);

  RUN_TEST(test_expand_pseudo_null_args_fail);
  RUN_TEST(test_expand_pseudo_nop);
  RUN_TEST(test_expand_pseudo_mov);
  RUN_TEST(test_expand_pseudo_li_two_instructions);

  RUN_TEST(test_expand_pseudoinstructions_null_returns_null);
  RUN_TEST(test_expand_pseudoinstructions_empty_input);
  RUN_TEST(test_expand_pseudoinstructions_real_instruction_passthrough);
  RUN_TEST(test_expand_pseudoinstructions_pseudo_expands_and_preserves_label_on_first_only);
  RUN_TEST(test_expand_pseudoinstructions_directive_passthrough);
  RUN_TEST(test_expand_pseudoinstructions_invalid_pseudo_fails);
  RUN_TEST(test_free_expanded_input_null_does_not_crash);

  return UNITY_END();
}