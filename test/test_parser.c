#define _POSIX_C_SOURCE 200809L

#include "../src/parser.c"
#include "unity/unity.h"
#include <string.h>

static Arena arena;

void setUp(void) { arena_init(&arena, 4096); }

void tearDown(void) { arena_free(&arena); }

static void link_tokens(Token* tokens, int count) {
  for (int i = 0; i < count - 1; i++)
    tokens[i].next = &tokens[i + 1];
  if (count > 0)
    tokens[count - 1].next = NULL;
}

static Token make_reg(int reg) {
  Token t = {0};
  t.type = TOKEN_REGISTER;
  t.reg = reg;
  return t;
}

static Token make_imm(int imm) {
  Token t = {0};
  t.type = TOKEN_IMMEDIATE;
  t.imm = imm;
  return t;
}

static Token make_punct(TokenType type) {
  Token t = {0};
  t.type = type;
  return t;
}

static Token make_symbol(const char* s) {
  Token t = {0};
  t.type = TOKEN_SYMBOL;
  t.start = (char*)s;
  t.len = strlen(s);
  return t;
}

static Token make_label(const char* s) {
  Token t = {0};
  t.type = TOKEN_LABEL;
  t.start = (char*)s;
  t.len = strlen(s);
  return t;
}

static Token make_string(const char* s) {
  Token t = {0};
  t.type = TOKEN_STRING;
  t.start = (char*)s;
  t.len = strlen(s);
  return t;
}

static Token make_directive(Directive dir) {
  Token t = {0};
  t.type = TOKEN_DIRECTIVE;
  t.dir = dir;
  return t;
}

static Token make_mnemonic(Opcode op) {
  Token t = {0};
  t.type = TOKEN_MNEMONIC;
  t.opcode.op = op;
  t.opcode.pseudo = PSEUDO_UNKNOWN;
  return t;
}

static Token make_pseudo(PseudoOpcode pseudo) {
  Token t = {0};
  t.type = TOKEN_PSEUDO;
  t.opcode.op = OP_UNKNOWN;
  t.opcode.pseudo = pseudo;
  return t;
}

static Token make_ambiguous(Opcode op, PseudoOpcode pseudo) {
  Token t = {0};
  t.type = TOKEN_INSTRUCTION_AMBIGUOUS;
  t.opcode.op = op;
  t.opcode.pseudo = pseudo;
  return t;
}

static Operand make_reg_operand(int reg) {
  Operand op = {0};
  op.type = OPERAND_REGISTER;
  op.reg.reg = reg;
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
 * get_instruction_format / get_directive_format / get_pseudo_format
 * ------------------------------------------------------------------- */

void test_get_instruction_format_valid(void) {
  Token tok = make_mnemonic(OP_ADD);
  const InstructionFormat* fmt = get_instruction_format(&tok);

  TEST_ASSERT_NOT_NULL(fmt);
  TEST_ASSERT_EQUAL(OP_ADD, fmt->op);
  TEST_ASSERT_EQUAL_INT(3, fmt->operand_count);
}

void test_get_instruction_format_ambiguous_type_accepted(void) {
  Token tok = make_ambiguous(OP_JAL, PSEUDO_JAL);
  const InstructionFormat* fmt = get_instruction_format(&tok);

  TEST_ASSERT_NOT_NULL(fmt);
  TEST_ASSERT_EQUAL(OP_JAL, fmt->op);
}

void test_get_instruction_format_wrong_token_type(void) {
  Token tok = make_reg(1);
  TEST_ASSERT_NULL(get_instruction_format(&tok));
}

void test_get_instruction_format_unknown_op(void) {
  Token tok = make_mnemonic(OP_UNKNOWN);
  TEST_ASSERT_NULL(get_instruction_format(&tok));
}

void test_get_directive_format_valid(void) {
  Token tok = make_directive(DIRECTIVE_WORD);
  const DirectiveFormat* fmt = get_directive_format(&tok);

  TEST_ASSERT_NOT_NULL(fmt);
  TEST_ASSERT_EQUAL(DIRECTIVE_WORD, fmt->directive);
}

void test_get_directive_format_wrong_token_type(void) {
  Token tok = make_mnemonic(OP_ADD);
  TEST_ASSERT_NULL(get_directive_format(&tok));
}

void test_get_directive_format_unknown_directive(void) {
  Token tok = make_directive(DIRECTIVE_UNKNOWN);
  TEST_ASSERT_NULL(get_directive_format(&tok));
}

void test_get_pseudo_format_valid(void) {
  Token tok = make_pseudo(PSEUDO_NOP);
  const PseudoInstructionFormat* fmt = get_pseudo_format(&tok);

  TEST_ASSERT_NOT_NULL(fmt);
  TEST_ASSERT_EQUAL(PSEUDO_NOP, fmt->pseudo);
  TEST_ASSERT_EQUAL_INT(0, fmt->operand_count);
}

void test_get_pseudo_format_ambiguous_type_accepted(void) {
  Token tok = make_ambiguous(OP_JAL, PSEUDO_JAL);
  const PseudoInstructionFormat* fmt = get_pseudo_format(&tok);

  TEST_ASSERT_NOT_NULL(fmt);
  TEST_ASSERT_EQUAL(PSEUDO_JAL, fmt->pseudo);
}

void test_get_pseudo_format_wrong_token_type(void) {
  Token tok = make_mnemonic(OP_ADD);
  TEST_ASSERT_NULL(get_pseudo_format(&tok));
}

/* ---------------------------------------------------------------------
 * token_type_to_string / operand_mask_to_string
 * ------------------------------------------------------------------- */

void test_token_type_to_string_samples(void) {
  TEST_ASSERT_EQUAL_STRING("unknown", token_type_to_string(TOKEN_UNKNOWN));
  TEST_ASSERT_EQUAL_STRING("instruction", token_type_to_string(TOKEN_MNEMONIC));
  TEST_ASSERT_EQUAL_STRING("instruction", token_type_to_string(TOKEN_PSEUDO));
  TEST_ASSERT_EQUAL_STRING("','", token_type_to_string(TOKEN_COMMA));
  TEST_ASSERT_EQUAL_STRING("register", token_type_to_string(TOKEN_REGISTER));
}

void test_operand_mask_to_string_single_bit(void) {
  TEST_ASSERT_EQUAL_STRING("register", operand_mask_to_string(OPERAND_REGISTER));
}

void test_operand_mask_to_string_combined_bits(void) {
  TEST_ASSERT_EQUAL_STRING("register|symbol", operand_mask_to_string(OPERAND_REGISTER | OPERAND_SYMBOL));
}

void test_operand_mask_to_string_zero_is_none(void) { TEST_ASSERT_EQUAL_STRING("none", operand_mask_to_string(0)); }

/* ---------------------------------------------------------------------
 * try_parse_operands
 * ------------------------------------------------------------------- */

void test_try_parse_operands_three_registers_succeeds(void) {
  Token tokens[] = {make_reg(10), make_punct(TOKEN_COMMA), make_reg(11), make_punct(TOKEN_COMMA), make_reg(12)};
  link_tokens(tokens, 5);
  OperandMask expected[] = {OPERAND_REGISTER, OPERAND_REGISTER, OPERAND_REGISTER};
  ParsedLine dest = {0};

  int result = try_parse_operands(&arena, &tokens[0], 3, expected, &dest, 0);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL_INT(3, dest.instruction.operand_count);
  TEST_ASSERT_EQUAL_INT(10, dest.instruction.operands[0].reg.reg);
  TEST_ASSERT_EQUAL_INT(11, dest.instruction.operands[1].reg.reg);
  TEST_ASSERT_EQUAL_INT(12, dest.instruction.operands[2].reg.reg);
}

void test_try_parse_operands_null_curr_too_few(void) {
  OperandMask expected[] = {OPERAND_REGISTER};
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, try_parse_operands(&arena, NULL, 1, expected, &dest, 0));
}

void test_try_parse_operands_missing_comma_fails(void) {
  Token tokens[] = {make_reg(1), make_reg(2)}; /* no comma between */
  link_tokens(tokens, 2);
  OperandMask expected[] = {OPERAND_REGISTER, OPERAND_REGISTER};
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, try_parse_operands(&arena, &tokens[0], 2, expected, &dest, 0));
}

void test_try_parse_operands_too_many_fails(void) {
  Token tokens[] = {make_reg(1), make_punct(TOKEN_COMMA), make_reg(2), make_punct(TOKEN_COMMA), make_reg(3)};
  link_tokens(tokens, 5);
  OperandMask expected[] = {OPERAND_REGISTER, OPERAND_REGISTER};
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, try_parse_operands(&arena, &tokens[0], 2, expected, &dest, 0));
}

void test_try_parse_operands_symbol_operand(void) {
  Token tokens[] = {make_symbol("myfunc")};
  link_tokens(tokens, 1);
  OperandMask expected[] = {OPERAND_SYMBOL};
  ParsedLine dest = {0};

  int result = try_parse_operands(&arena, &tokens[0], 1, expected, &dest, 0);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(OPERAND_SYMBOL, dest.instruction.operands[0].type);
  TEST_ASSERT_EQUAL_UINT(6, dest.instruction.operands[0].label.len);
  TEST_ASSERT_EQUAL_MEMORY("myfunc", dest.instruction.operands[0].label.label, 6);
}

void test_try_parse_operands_memory_operand_full_form(void) {
  Token tokens[] = {make_imm(4), make_punct(TOKEN_LPARAN), make_reg(2), make_punct(TOKEN_RPARAN)};
  link_tokens(tokens, 4);
  OperandMask expected[] = {OPERAND_MEMORY};
  ParsedLine dest = {0};

  int result = try_parse_operands(&arena, &tokens[0], 1, expected, &dest, 0);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(OPERAND_MEMORY, dest.instruction.operands[0].type);
  TEST_ASSERT_EQUAL_INT(4, dest.instruction.operands[0].mem.offset);
  TEST_ASSERT_EQUAL_INT(2, dest.instruction.operands[0].mem.base_reg);
}

void test_try_parse_operands_memory_missing_lparen_fails(void) {
  Token tokens[] = {make_imm(4)};
  link_tokens(tokens, 1);
  OperandMask expected[] = {OPERAND_MEMORY};
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, try_parse_operands(&arena, &tokens[0], 1, expected, &dest, 0));
}

void test_try_parse_operands_memory_missing_register_fails(void) {
  Token tokens[] = {make_imm(4), make_punct(TOKEN_LPARAN), make_punct(TOKEN_RPARAN)};
  link_tokens(tokens, 3);
  OperandMask expected[] = {OPERAND_MEMORY};
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, try_parse_operands(&arena, &tokens[0], 1, expected, &dest, 0));
}

void test_try_parse_operands_memory_missing_rparen_fails(void) {
  Token tokens[] = {make_imm(4), make_punct(TOKEN_LPARAN), make_reg(2)};
  link_tokens(tokens, 3);
  OperandMask expected[] = {OPERAND_MEMORY};
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, try_parse_operands(&arena, &tokens[0], 1, expected, &dest, 0));
}

void test_try_parse_operands_type_mismatch_fails(void) {
  Token tokens[] = {make_imm(5)};
  link_tokens(tokens, 1);
  OperandMask expected[] = {OPERAND_REGISTER};
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, try_parse_operands(&arena, &tokens[0], 1, expected, &dest, 0));
}

/* ---------------------------------------------------------------------
 * handle_mnemonic_token
 * ------------------------------------------------------------------- */

void test_handle_mnemonic_token_null_curr_fails(void) {
  ParsedLine dest = {0};
  TEST_ASSERT_EQUAL_INT(0, handle_mnemonic_token(&arena, NULL, &dest));
}

void test_handle_mnemonic_token_real_only(void) {
  Token tokens[] = {make_mnemonic(OP_ADD),   make_reg(10), make_punct(TOKEN_COMMA), make_reg(11),
                    make_punct(TOKEN_COMMA), make_reg(12)};
  link_tokens(tokens, 6);
  ParsedLine dest = {0};

  int result = handle_mnemonic_token(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(LINE_INSTRUCTION, dest.type);
  TEST_ASSERT_EQUAL(INSTRUCTION_REAL, dest.instruction.type);
  TEST_ASSERT_EQUAL(OP_ADD, dest.instruction.op);
}

void test_handle_mnemonic_token_pseudo_only_no_operands(void) {
  Token tokens[] = {make_pseudo(PSEUDO_NOP)};
  link_tokens(tokens, 1);
  ParsedLine dest = {0};

  int result = handle_mnemonic_token(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(INSTRUCTION_PSEUDO, dest.instruction.type);
  TEST_ASSERT_EQUAL(PSEUDO_NOP, dest.instruction.pseudo);
}

void test_handle_mnemonic_token_ambiguous_resolves_real(void) {
  Token tokens[] = {make_ambiguous(OP_JAL, PSEUDO_JAL), make_reg(1), make_punct(TOKEN_COMMA), make_imm(100)};
  link_tokens(tokens, 4);
  ParsedLine dest = {0};

  int result = handle_mnemonic_token(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(INSTRUCTION_REAL, dest.instruction.type);
  TEST_ASSERT_EQUAL(OP_JAL, dest.instruction.op);
}

void test_handle_mnemonic_token_ambiguous_falls_back_to_pseudo(void) {
  Token tokens[] = {make_ambiguous(OP_JAL, PSEUDO_JAL), make_symbol("myfunc")};
  link_tokens(tokens, 2);
  ParsedLine dest = {0};

  int result = handle_mnemonic_token(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(INSTRUCTION_PSEUDO, dest.instruction.type);
  TEST_ASSERT_EQUAL(PSEUDO_JAL, dest.instruction.pseudo);
}

void test_handle_mnemonic_token_unknown_instruction_fails(void) {
  Token tokens[] = {make_mnemonic(OP_UNKNOWN)};
  link_tokens(tokens, 1);
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, handle_mnemonic_token(&arena, &tokens[0], &dest));
}

void test_handle_mnemonic_token_wrong_operands_for_both_forms_fails(void) {
  Token tokens[] = {make_ambiguous(OP_JAL, PSEUDO_JAL), make_string("oops")};
  link_tokens(tokens, 2);
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, handle_mnemonic_token(&arena, &tokens[0], &dest));
}

/* ---------------------------------------------------------------------
 * handle_directive_token
 * ------------------------------------------------------------------- */

void test_handle_directive_token_null_curr_fails(void) {
  ParsedLine dest = {0};
  TEST_ASSERT_EQUAL_INT(0, handle_directive_token(&arena, NULL, &dest));
}

void test_handle_directive_token_unknown_directive_fails(void) {
  Token tokens[] = {make_directive(DIRECTIVE_UNKNOWN)};
  link_tokens(tokens, 1);
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, handle_directive_token(&arena, &tokens[0], &dest));
}

void test_handle_directive_token_single_arg_succeeds(void) {
  Token tokens[] = {make_directive(DIRECTIVE_ZERO), make_imm(10)};
  link_tokens(tokens, 2);
  ParsedLine dest = {0};

  int result = handle_directive_token(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL_INT(1, dest.directive.arg_count);
  TEST_ASSERT_EQUAL_INT(10, dest.directive.args->imm);

  free_directive_args(dest.directive.args);
}

void test_handle_directive_token_below_min_args_fails(void) {
  Token tokens[] = {make_directive(DIRECTIVE_ZERO)}; /* min_args=1, none given */
  link_tokens(tokens, 1);
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, handle_directive_token(&arena, &tokens[0], &dest));
}

void test_handle_directive_token_above_max_args_fails(void) {
  Token tokens[] = {make_directive(DIRECTIVE_ZERO), make_imm(1), make_imm(2)};
  link_tokens(tokens, 3);
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, handle_directive_token(&arena, &tokens[0], &dest));
}

void test_handle_directive_token_wrong_arg_type_fails(void) {
  Token tokens[] = {make_directive(DIRECTIVE_ZERO), make_string("oops")};
  link_tokens(tokens, 2);
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, handle_directive_token(&arena, &tokens[0], &dest));
}

void test_handle_directive_token_string_arg_succeeds(void) {
  Token tokens[] = {make_directive(DIRECTIVE_STRING), make_string("hi")};
  link_tokens(tokens, 2);
  ParsedLine dest = {0};

  int result = handle_directive_token(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(OPERAND_STRING, dest.directive.args->type);
  TEST_ASSERT_EQUAL_MEMORY("hi", dest.directive.args->start, 2);

  free_directive_args(dest.directive.args);
}

void test_handle_directive_token_multi_arg_with_commas_succeeds(void) {
  Token tokens[] = {make_directive(DIRECTIVE_WORD), make_imm(1), make_punct(TOKEN_COMMA), make_imm(2),
                    make_punct(TOKEN_COMMA),        make_imm(3)};
  link_tokens(tokens, 6);
  ParsedLine dest = {0};

  int result = handle_directive_token(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL_INT(3, dest.directive.arg_count);
  TEST_ASSERT_EQUAL_INT(1, dest.directive.args->imm);
  TEST_ASSERT_EQUAL_INT(2, dest.directive.args->next->imm);
  TEST_ASSERT_EQUAL_INT(3, dest.directive.args->next->next->imm);
  free_directive_args(dest.directive.args);
}

void test_handle_directive_token_missing_comma_between_args_fails(void) {
  Token tokens[] = {make_directive(DIRECTIVE_WORD), make_imm(1), make_imm(2)};
  link_tokens(tokens, 3);
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, handle_directive_token(&arena, &tokens[0], &dest));
}

void test_handle_directive_token_trailing_comma_fails(void) {
  Token tokens[] = {make_directive(DIRECTIVE_WORD), make_imm(1), make_punct(TOKEN_COMMA)};
  link_tokens(tokens, 3);
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, handle_directive_token(&arena, &tokens[0], &dest));
}

void test_handle_directive_token_equ_end_to_end(void) {
  Token tokens[] = {make_directive(DIRECTIVE_EQU), make_symbol("SIZE"), make_punct(TOKEN_COMMA), make_imm(4)};
  link_tokens(tokens, 4);
  ParsedLine dest = {0};

  int result = handle_directive_token(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL_INT(2, dest.directive.arg_count);
  TEST_ASSERT_EQUAL(OPERAND_SYMBOL, dest.directive.args->type);
  TEST_ASSERT_EQUAL_UINT(4, dest.directive.args->len);
  TEST_ASSERT_EQUAL_MEMORY("SIZE", dest.directive.args->start, 4);
  TEST_ASSERT_EQUAL(OPERAND_IMMEDIATE, dest.directive.args->next->type);
  TEST_ASSERT_EQUAL_INT(4, dest.directive.args->next->imm);
  TEST_ASSERT_NULL(dest.directive.args->next->next);

  free_directive_args(dest.directive.args);
}

void test_handle_directive_token_equ_value_must_be_literal_not_symbol(void) {
  Token tokens[] = {make_directive(DIRECTIVE_EQU), make_symbol("ALIAS"), make_punct(TOKEN_COMMA), make_symbol("OTHER")};
  link_tokens(tokens, 4);
  ParsedLine dest = {0};

  TEST_ASSERT_EQUAL_INT(0, handle_directive_token(&arena, &tokens[0], &dest));
}

void test_handle_directive_token_word_accepts_symbol_arg(void) {
  Token tokens[] = {make_directive(DIRECTIVE_WORD), make_symbol("myvar")};
  link_tokens(tokens, 2);
  ParsedLine dest = {0};

  int result = handle_directive_token(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(OPERAND_SYMBOL, dest.directive.args->type);
  free_directive_args(dest.directive.args);
}

/* ---------------------------------------------------------------------
 * parse_tokenized_line
 * ------------------------------------------------------------------- */

void test_parse_tokenized_line_null_args_fail(void) {
  ParsedLine dest = {0};
  Token tok = make_mnemonic(OP_ADD);
  TEST_ASSERT_EQUAL_INT(0, parse_tokenized_line(&arena, NULL, &dest));
  TEST_ASSERT_EQUAL_INT(0, parse_tokenized_line(&arena, &tok, NULL));
}

void test_parse_tokenized_line_label_only(void) {
  Token tokens[] = {make_label("loop")};
  tokens[0].line = 5;
  link_tokens(tokens, 1);
  ParsedLine dest;

  int result = parse_tokenized_line(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(LINE_LABEL, dest.type);
  TEST_ASSERT_EQUAL_UINT(4, dest.len);
  TEST_ASSERT_EQUAL_MEMORY("loop", dest.label, 4);
  TEST_ASSERT_EQUAL_INT(5, dest.line);
}

void test_parse_tokenized_line_label_plus_instruction(void) {
  Token tokens[] = {make_label("loop"), make_pseudo(PSEUDO_NOP)};
  link_tokens(tokens, 2);
  ParsedLine dest;

  int result = parse_tokenized_line(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(LINE_INSTRUCTION, dest.type);
  TEST_ASSERT_EQUAL_MEMORY("loop", dest.label, 4);
}

void test_parse_tokenized_line_directive_only(void) {
  Token tokens[] = {make_directive(DIRECTIVE_TEXT)};
  link_tokens(tokens, 1);
  ParsedLine dest;

  int result = parse_tokenized_line(&arena, &tokens[0], &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL(LINE_DIRECTIVE, dest.type);
  TEST_ASSERT_EQUAL_INT(0, dest.directive.arg_count);
}

void test_parse_tokenized_line_unexpected_token_fails(void) {
  Token tokens[] = {make_punct(TOKEN_COMMA)};
  link_tokens(tokens, 1);
  ParsedLine dest;

  TEST_ASSERT_EQUAL_INT(0, parse_tokenized_line(&arena, &tokens[0], &dest));
}

/* ---------------------------------------------------------------------
 * parse_tokenized_input / free_parsed_input
 * ------------------------------------------------------------------- */

void test_parse_tokenized_input_multiple_valid_lines(void) {
  Token line1[] = {make_pseudo(PSEUDO_NOP)};
  link_tokens(line1, 1);
  Token line2[] = {make_directive(DIRECTIVE_TEXT)};
  link_tokens(line2, 1);
  Token* lines[] = {line1, line2};

  TokenizedInput input;
  arena_init(&input.arena, 256);
  input.arena.used = 64; /* pretend some space was used, so the result arena is non-trivial */
  input.lines = lines;
  input.count = 2;

  ParsedInput* result = parse_tokenized_input(&input);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(2, result->count);
  TEST_ASSERT_EQUAL(LINE_INSTRUCTION, result->lines[0].type);
  TEST_ASSERT_EQUAL(LINE_DIRECTIVE, result->lines[1].type);

  free_parsed_input(result);
  arena_free(&input.arena);
}

void test_parse_tokenized_input_empty_input_succeeds(void) {
  TokenizedInput input;
  arena_init(&input.arena, 64);
  input.arena.used = 16;
  input.lines = NULL;
  input.count = 0;

  ParsedInput* result = parse_tokenized_input(&input);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(0, result->count);

  free_parsed_input(result);
  arena_free(&input.arena);
}

void test_parse_tokenized_input_bad_line_fails(void) {
  Token bad_line[] = {make_punct(TOKEN_COMMA)}; /* unexpected top-level token */
  link_tokens(bad_line, 1);
  Token* lines[] = {bad_line};

  TokenizedInput input;
  arena_init(&input.arena, 64);
  input.arena.used = 16;
  input.lines = lines;
  input.count = 1;

  ParsedInput* result = parse_tokenized_input(&input);

  TEST_ASSERT_NULL(result);
  arena_free(&input.arena);
}

void test_free_parsed_input_null_does_not_crash(void) {
  free_parsed_input(NULL);
  TEST_PASS();
}

void test_free_directive_args_null_does_not_crash(void) {
  free_directive_args(NULL);
  TEST_PASS();
}

/* ---------------------------------------------------------------------
 * copy_operand
 * ------------------------------------------------------------------- */

void test_copy_operand_null_args_fail(void) {
  Operand op = make_reg_operand(1);
  TEST_ASSERT_EQUAL_INT(0, copy_operand(NULL, &op));
  TEST_ASSERT_EQUAL_INT(0, copy_operand(&arena, NULL));
}

void test_copy_operand_non_symbol_is_noop(void) {
  Operand op = make_reg_operand(3);
  TEST_ASSERT_EQUAL_INT(1, copy_operand(&arena, &op));
  TEST_ASSERT_EQUAL_INT(3, op.reg.reg);
}

void test_copy_operand_symbol_copies_into_new_arena(void) {
  const char* original_str = "myfunc";
  Operand op = make_symbol_operand(original_str);

  int result = copy_operand(&arena, &op);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_NOT_EQUAL(original_str, op.label.label);
  TEST_ASSERT_EQUAL_MEMORY("myfunc", op.label.label, 6);
}

void test_copy_operand_symbol_arena_exhausted_fails(void) {
  Arena tiny;
  arena_init(&tiny, 2);
  Operand op = make_symbol_operand("toolong");

  TEST_ASSERT_EQUAL_INT(0, copy_operand(&tiny, &op));
  arena_free(&tiny);
}

/* ---------------------------------------------------------------------
 * copy_directive_args
 * ------------------------------------------------------------------- */

void test_copy_directive_args_null_args_returns_null(void) {
  DirectiveArg dummy = {0};
  TEST_ASSERT_NULL(copy_directive_args(NULL, &dummy));
  TEST_ASSERT_NULL(copy_directive_args(&arena, NULL));
}

void test_copy_directive_args_immediate_arg(void) {
  DirectiveArg src = {0};
  src.type = OPERAND_IMMEDIATE;
  src.imm = 99;

  DirectiveArg* copy = copy_directive_args(&arena, &src);

  TEST_ASSERT_NOT_NULL(copy);
  TEST_ASSERT_EQUAL_INT(99, copy->imm);
  TEST_ASSERT_NULL(copy->next);
  free_directive_args(copy);
}

void test_copy_directive_args_string_arg_copies_into_new_arena(void) {
  DirectiveArg src = {0};
  src.type = OPERAND_STRING;
  src.start = "hello";
  src.len = 5;

  DirectiveArg* copy = copy_directive_args(&arena, &src);

  TEST_ASSERT_NOT_NULL(copy);
  TEST_ASSERT_NOT_EQUAL(src.start, copy->start);
  TEST_ASSERT_EQUAL_MEMORY("hello", copy->start, 5);
  free_directive_args(copy);
}

void test_copy_directive_args_preserves_chain_order(void) {
  DirectiveArg a = {0};
  a.type = OPERAND_IMMEDIATE;
  a.imm = 1;
  DirectiveArg b = {0};
  b.type = OPERAND_IMMEDIATE;
  b.imm = 2;
  a.next = &b;

  DirectiveArg* copy = copy_directive_args(&arena, &a);

  TEST_ASSERT_NOT_NULL(copy);
  TEST_ASSERT_EQUAL_INT(1, copy->imm);
  TEST_ASSERT_NOT_NULL(copy->next);
  TEST_ASSERT_EQUAL_INT(2, copy->next->imm);
  free_directive_args(copy);
}

void test_copy_directive_args_arena_exhausted_returns_null(void) {
  Arena tiny;
  arena_init(&tiny, 2);
  DirectiveArg src = {0};
  src.type = OPERAND_STRING;
  src.start = "toolong";
  src.len = 7;

  TEST_ASSERT_NULL(copy_directive_args(&tiny, &src));
  arena_free(&tiny);
}

/* ---------------------------------------------------------------------
 * copy_parsed_line
 * ------------------------------------------------------------------- */

void test_copy_parsed_line_null_args_fail(void) {
  ParsedLine src = {0};
  ParsedLine dest;
  TEST_ASSERT_EQUAL_INT(0, copy_parsed_line(NULL, &src, &dest));
  TEST_ASSERT_EQUAL_INT(0, copy_parsed_line(&arena, NULL, &dest));
  TEST_ASSERT_EQUAL_INT(0, copy_parsed_line(&arena, &src, NULL));
}

void test_copy_parsed_line_label_copied_into_new_arena(void) {
  ParsedLine src = {0};
  src.type = LINE_LABEL;
  src.label = "loop";
  src.len = 4;
  ParsedLine dest;

  int result = copy_parsed_line(&arena, &src, &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_NOT_EQUAL(src.label, dest.label);
  TEST_ASSERT_EQUAL_MEMORY("loop", dest.label, 4);
}

void test_copy_parsed_line_no_label_stays_null(void) {
  ParsedLine src = {0};
  src.type = LINE_INSTRUCTION;
  src.instruction.type = INSTRUCTION_REAL;
  src.instruction.op = OP_ADD;
  src.instruction.operand_count = 3;
  src.instruction.operands[0] = make_reg_operand(1);
  src.instruction.operands[1] = make_reg_operand(2);
  src.instruction.operands[2] = make_reg_operand(3);
  ParsedLine dest;

  int result = copy_parsed_line(&arena, &src, &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_NULL(dest.label);
  TEST_ASSERT_EQUAL_INT(1, dest.instruction.operands[0].reg.reg);
}

void test_copy_parsed_line_instruction_symbol_operand_copied(void) {
  ParsedLine src = {0};
  src.type = LINE_INSTRUCTION;
  src.instruction.type = INSTRUCTION_PSEUDO;
  src.instruction.pseudo = PSEUDO_J;
  src.instruction.operand_count = 1;
  const char* label_str = "target";
  src.instruction.operands[0] = make_symbol_operand(label_str);
  ParsedLine dest;

  int result = copy_parsed_line(&arena, &src, &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_NOT_EQUAL(label_str, dest.instruction.operands[0].label.label);
  TEST_ASSERT_EQUAL_MEMORY("target", dest.instruction.operands[0].label.label, 6);
}

void test_copy_parsed_line_directive_args_copied(void) {
  DirectiveArg arg = {0};
  arg.type = OPERAND_IMMEDIATE;
  arg.imm = 55;

  ParsedLine src = {0};
  src.type = LINE_DIRECTIVE;
  src.directive.directive = DIRECTIVE_ZERO;
  src.directive.arg_count = 1;
  src.directive.args = &arg;
  ParsedLine dest;

  int result = copy_parsed_line(&arena, &src, &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_NOT_EQUAL(&arg, dest.directive.args);
  TEST_ASSERT_EQUAL_INT(55, dest.directive.args->imm);
  free_directive_args(dest.directive.args);
}

void test_copy_parsed_line_directive_no_args_stays_null(void) {
  ParsedLine src = {0};
  src.type = LINE_DIRECTIVE;
  src.directive.directive = DIRECTIVE_TEXT;
  src.directive.arg_count = 0;
  src.directive.args = NULL;
  ParsedLine dest;

  int result = copy_parsed_line(&arena, &src, &dest);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_NULL(dest.directive.args);
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_get_instruction_format_valid);
  RUN_TEST(test_get_instruction_format_ambiguous_type_accepted);
  RUN_TEST(test_get_instruction_format_wrong_token_type);
  RUN_TEST(test_get_instruction_format_unknown_op);
  RUN_TEST(test_get_directive_format_valid);
  RUN_TEST(test_get_directive_format_wrong_token_type);
  RUN_TEST(test_get_directive_format_unknown_directive);
  RUN_TEST(test_get_pseudo_format_valid);
  RUN_TEST(test_get_pseudo_format_ambiguous_type_accepted);
  RUN_TEST(test_get_pseudo_format_wrong_token_type);

  RUN_TEST(test_token_type_to_string_samples);
  RUN_TEST(test_operand_mask_to_string_single_bit);
  RUN_TEST(test_operand_mask_to_string_combined_bits);
  RUN_TEST(test_operand_mask_to_string_zero_is_none);

  RUN_TEST(test_try_parse_operands_three_registers_succeeds);
  RUN_TEST(test_try_parse_operands_null_curr_too_few);
  RUN_TEST(test_try_parse_operands_missing_comma_fails);
  RUN_TEST(test_try_parse_operands_too_many_fails);
  RUN_TEST(test_try_parse_operands_symbol_operand);
  RUN_TEST(test_try_parse_operands_memory_operand_full_form);
  RUN_TEST(test_try_parse_operands_memory_missing_lparen_fails);
  RUN_TEST(test_try_parse_operands_memory_missing_register_fails);
  RUN_TEST(test_try_parse_operands_memory_missing_rparen_fails);
  RUN_TEST(test_try_parse_operands_type_mismatch_fails);

  RUN_TEST(test_handle_mnemonic_token_null_curr_fails);
  RUN_TEST(test_handle_mnemonic_token_real_only);
  RUN_TEST(test_handle_mnemonic_token_pseudo_only_no_operands);
  RUN_TEST(test_handle_mnemonic_token_ambiguous_resolves_real);
  RUN_TEST(test_handle_mnemonic_token_ambiguous_falls_back_to_pseudo);
  RUN_TEST(test_handle_mnemonic_token_unknown_instruction_fails);
  RUN_TEST(test_handle_mnemonic_token_wrong_operands_for_both_forms_fails);

  RUN_TEST(test_handle_directive_token_null_curr_fails);
  RUN_TEST(test_handle_directive_token_unknown_directive_fails);
  RUN_TEST(test_handle_directive_token_single_arg_succeeds);
  RUN_TEST(test_handle_directive_token_below_min_args_fails);
  RUN_TEST(test_handle_directive_token_above_max_args_fails);
  RUN_TEST(test_handle_directive_token_wrong_arg_type_fails);
  RUN_TEST(test_handle_directive_token_string_arg_succeeds);
  RUN_TEST(test_handle_directive_token_multi_arg_with_commas_succeeds);
  RUN_TEST(test_handle_directive_token_missing_comma_between_args_fails);
  RUN_TEST(test_handle_directive_token_trailing_comma_fails);
  RUN_TEST(test_handle_directive_token_equ_end_to_end);
  RUN_TEST(test_handle_directive_token_equ_value_must_be_literal_not_symbol);
  RUN_TEST(test_handle_directive_token_word_accepts_symbol_arg);

  RUN_TEST(test_parse_tokenized_line_null_args_fail);
  RUN_TEST(test_parse_tokenized_line_label_only);
  RUN_TEST(test_parse_tokenized_line_label_plus_instruction);
  RUN_TEST(test_parse_tokenized_line_directive_only);
  RUN_TEST(test_parse_tokenized_line_unexpected_token_fails);

  RUN_TEST(test_parse_tokenized_input_multiple_valid_lines);
  RUN_TEST(test_parse_tokenized_input_empty_input_succeeds);
  RUN_TEST(test_parse_tokenized_input_bad_line_fails);

  RUN_TEST(test_copy_operand_null_args_fail);
  RUN_TEST(test_copy_operand_non_symbol_is_noop);
  RUN_TEST(test_copy_operand_symbol_copies_into_new_arena);
  RUN_TEST(test_copy_operand_symbol_arena_exhausted_fails);

  RUN_TEST(test_copy_directive_args_null_args_returns_null);
  RUN_TEST(test_copy_directive_args_immediate_arg);
  RUN_TEST(test_copy_directive_args_string_arg_copies_into_new_arena);
  RUN_TEST(test_copy_directive_args_preserves_chain_order);
  RUN_TEST(test_copy_directive_args_arena_exhausted_returns_null);

  RUN_TEST(test_copy_parsed_line_null_args_fail);
  RUN_TEST(test_copy_parsed_line_label_copied_into_new_arena);
  RUN_TEST(test_copy_parsed_line_no_label_stays_null);
  RUN_TEST(test_copy_parsed_line_instruction_symbol_operand_copied);
  RUN_TEST(test_copy_parsed_line_directive_args_copied);
  RUN_TEST(test_copy_parsed_line_directive_no_args_stays_null);

  RUN_TEST(test_free_parsed_input_null_does_not_crash);
  RUN_TEST(test_free_directive_args_null_does_not_crash);

  return UNITY_END();
}