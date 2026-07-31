#define _POSIX_C_SOURCE 200809L

#include "../src/lexer.c"
#include "unity/unity.h"
#include <string.h>

static Arena arena;

void setUp(void) { arena_init(&arena, 4096); }

void tearDown(void) { arena_free(&arena); }

/* ---------------------------------------------------------------------
 * create_token / create_token_instruction
 * ------------------------------------------------------------------- */

void test_create_token_null_arena_returns_null(void) {
  TEST_ASSERT_NULL(create_token(NULL, TOKEN_SYMBOL, "abc", 3, 0, 1, 1));
}

void test_create_token_null_start_returns_null(void) {
  TEST_ASSERT_NULL(create_token(&arena, TOKEN_SYMBOL, NULL, 3, 0, 1, 1));
}

void test_create_token_basic_fields(void) {
  Token* tok = create_token(&arena, TOKEN_SYMBOL, "hello", 5, 0, 3, 7);

  TEST_ASSERT_NOT_NULL(tok);
  TEST_ASSERT_EQUAL(TOKEN_SYMBOL, tok->type);
  TEST_ASSERT_EQUAL_STRING("hello", tok->start);
  TEST_ASSERT_EQUAL_UINT(5, tok->len);
  TEST_ASSERT_EQUAL_UINT32(3, tok->line);
  TEST_ASSERT_EQUAL_UINT32(7, tok->col);
  TEST_ASSERT_NULL(tok->next);
  free(tok);
}

void test_create_token_register_sets_reg_field(void) {
  Token* tok = create_token(&arena, TOKEN_REGISTER, "sp", 2, 2, 1, 1);

  TEST_ASSERT_EQUAL_INT(2, tok->reg);
  free(tok);
}

void test_create_token_immediate_sets_imm_field(void) {
  Token* tok = create_token(&arena, TOKEN_IMMEDIATE, "42", 2, 42, 1, 1);

  TEST_ASSERT_EQUAL_INT(42, tok->imm);
  free(tok);
}

void test_create_token_directive_sets_dir_field(void) {
  Token* tok = create_token(&arena, TOKEN_DIRECTIVE, ".word", 5, (int)DIRECTIVE_WORD, 1, 1);

  TEST_ASSERT_EQUAL(DIRECTIVE_WORD, tok->dir);
  free(tok);
}

void test_create_token_zero_len_is_empty_string(void) {
  Token* tok = create_token(&arena, TOKEN_COMMA, ",", 0, 0, 1, 1);

  TEST_ASSERT_NOT_NULL(tok);
  TEST_ASSERT_EQUAL_STRING("", tok->start);
  free(tok);
}

void test_create_token_arena_exhausted_returns_null(void) {
  Arena tiny;
  arena_init(&tiny, 2);

  Token* tok = create_token(&tiny, TOKEN_SYMBOL, "toolong", 7, 0, 1, 1);

  TEST_ASSERT_NULL(tok);
  arena_free(&tiny);
}

void test_create_token_instruction_rejects_wrong_type(void) {
  TEST_ASSERT_NULL(create_token_instruction(&arena, TOKEN_REGISTER, "add", 3, OP_ADD, PSEUDO_UNKNOWN, 1, 1));
}

void test_create_token_instruction_sets_opcode_fields(void) {
  Token* tok = create_token_instruction(&arena, TOKEN_MNEMONIC, "add", 3, OP_ADD, PSEUDO_UNKNOWN, 1, 1);

  TEST_ASSERT_NOT_NULL(tok);
  TEST_ASSERT_EQUAL(OP_ADD, tok->opcode.op);
  TEST_ASSERT_EQUAL(PSEUDO_UNKNOWN, tok->opcode.pseudo);
  free(tok);
}

/* ---------------------------------------------------------------------
 * append_token
 * ------------------------------------------------------------------- */

void test_append_token_null_args_no_crash(void) {
  Token* tok = create_token(&arena, TOKEN_SYMBOL, "x", 1, 0, 1, 1);
  Token* head = NULL;
  Token* tail = NULL;

  append_token(NULL, &tail, tok);
  append_token(&head, NULL, tok);
  append_token(&head, &tail, NULL);

  TEST_ASSERT_NULL(head);
  free(tok);
}

void test_append_token_first_sets_head_and_tail(void) {
  Token* head = NULL;
  Token* tail = NULL;
  Token* tok = create_token(&arena, TOKEN_SYMBOL, "x", 1, 0, 1, 1);

  append_token(&head, &tail, tok);

  TEST_ASSERT_EQUAL_PTR(tok, head);
  TEST_ASSERT_EQUAL_PTR(tok, tail);
  free(tok);
}

void test_append_token_chains_multiple(void) {
  Token* head = NULL;
  Token* tail = NULL;
  Token* a = create_token(&arena, TOKEN_SYMBOL, "a", 1, 0, 1, 1);
  Token* b = create_token(&arena, TOKEN_SYMBOL, "b", 1, 0, 1, 2);

  append_token(&head, &tail, a);
  append_token(&head, &tail, b);

  TEST_ASSERT_EQUAL_PTR(a, head);
  TEST_ASSERT_EQUAL_PTR(b, tail);
  TEST_ASSERT_EQUAL_PTR(b, a->next);
  free(a);
  free(b);
}

/* ---------------------------------------------------------------------
 * is_valid_symbol
 * ------------------------------------------------------------------- */

void test_is_valid_symbol_leading_digit_invalid(void) { TEST_ASSERT_EQUAL_INT(0, is_valid_symbol("9abc", 4)); }

void test_is_valid_symbol_valid_chars(void) { TEST_ASSERT_EQUAL_INT(1, is_valid_symbol("loop_1.x", 8)); }

void test_is_valid_symbol_uppercase_invalid(void) { TEST_ASSERT_EQUAL_INT(0, is_valid_symbol("Loop", 4)); }

void test_is_valid_symbol_disallowed_char_invalid(void) { TEST_ASSERT_EQUAL_INT(0, is_valid_symbol("a-b", 3)); }

void test_is_valid_symbol_zero_length_invalid(void) { TEST_ASSERT_EQUAL_INT(0, is_valid_symbol("abc", 0)); }

/* ---------------------------------------------------------------------
 * is_valid_immediate
 * ------------------------------------------------------------------- */

void test_is_valid_immediate_decimal_positive(void) {
  int out = 0;
  TEST_ASSERT_EQUAL_INT(1, is_valid_immediate("123", 3, &out));
  TEST_ASSERT_EQUAL_INT(123, out);
}

void test_is_valid_immediate_decimal_negative(void) {
  int out = 0;
  TEST_ASSERT_EQUAL_INT(1, is_valid_immediate("-5", 2, &out));
  TEST_ASSERT_EQUAL_INT(-5, out);
}

void test_is_valid_immediate_hex(void) {
  int out = 0;
  TEST_ASSERT_EQUAL_INT(1, is_valid_immediate("0xFF", 4, &out));
  TEST_ASSERT_EQUAL_INT(255, out);
}

void test_is_valid_immediate_hex_too_short_invalid(void) {
  int out = 0;
  TEST_ASSERT_EQUAL_INT(0, is_valid_immediate("0x", 2, &out));
}

void test_is_valid_immediate_hex_invalid_digits(void) {
  int out = 0;
  TEST_ASSERT_EQUAL_INT(0, is_valid_immediate("0xzz", 4, &out));
}

void test_is_valid_immediate_trailing_garbage_invalid(void) {
  int out = 0;
  TEST_ASSERT_EQUAL_INT(0, is_valid_immediate("123abc", 6, &out));
}

void test_is_valid_immediate_char_literal(void) {
  int out = 0;
  TEST_ASSERT_EQUAL_INT(1, is_valid_immediate("'a'", 3, &out));
  TEST_ASSERT_EQUAL_INT('a', out);
}

void test_is_valid_immediate_out_of_int32_range_invalid(void) {
  int out = 0;
  TEST_ASSERT_EQUAL_INT(0, is_valid_immediate("99999999999", 11, &out));
}

void test_is_valid_immediate_zero_length_incorrectly_valid(void) {
  int out = -999;
  TEST_ASSERT_EQUAL_INT(0, is_valid_immediate("xyz", 0, &out));
}

/* ---------------------------------------------------------------------
 * is_valid_string
 * ------------------------------------------------------------------- */

void test_is_valid_string_normal(void) { TEST_ASSERT_EQUAL_INT(1, is_valid_string("\"hi\"", 4)); }

void test_is_valid_string_empty_contents(void) { TEST_ASSERT_EQUAL_INT(1, is_valid_string("\"\"", 2)); }

void test_is_valid_string_too_short_invalid(void) { TEST_ASSERT_EQUAL_INT(0, is_valid_string("\"", 1)); }

void test_is_valid_string_missing_closing_quote_invalid(void) { TEST_ASSERT_EQUAL_INT(0, is_valid_string("\"hi", 3)); }

/* ---------------------------------------------------------------------
 * unescape_string
 * ------------------------------------------------------------------- */

void test_unescape_string_no_escapes(void) {
  size_t out_len = 0;
  char* result = unescape_string("abc", 3, &out_len);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(3, out_len);
  TEST_ASSERT_EQUAL_MEMORY("abc", result, 3);
  free(result);
}

void test_unescape_string_all_supported_escapes(void) {
  size_t out_len = 0;
  char* result = unescape_string("\\n\\t\\r\\0\\\\\\\"\\'", 14, &out_len);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(7, out_len);
  TEST_ASSERT_EQUAL_CHAR('\n', result[0]);
  TEST_ASSERT_EQUAL_CHAR('\t', result[1]);
  TEST_ASSERT_EQUAL_CHAR('\r', result[2]);
  TEST_ASSERT_EQUAL_CHAR('\0', result[3]);
  TEST_ASSERT_EQUAL_CHAR('\\', result[4]);
  TEST_ASSERT_EQUAL_CHAR('"', result[5]);
  TEST_ASSERT_EQUAL_CHAR('\'', result[6]);
  free(result);
}

void test_unescape_string_trailing_backslash_fails(void) {
  size_t out_len = 999;
  char* result = unescape_string("ab\\", 3, &out_len);

  TEST_ASSERT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(2, out_len);
}

void test_unescape_string_invalid_escape_char_fails(void) {
  size_t out_len = 999;
  char* result = unescape_string("a\\q", 3, &out_len);

  TEST_ASSERT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(2, out_len);
}

void test_unescape_string_null_out_len_returns_null(void) { TEST_ASSERT_NULL(unescape_string("abc", 3, NULL)); }

/* ---------------------------------------------------------------------
 * classify_token
 * ------------------------------------------------------------------- */

void test_classify_token_null_or_zero_len(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_UNKNOWN, classify_token(NULL, 3, &out, &out2));
  TEST_ASSERT_EQUAL(TOKEN_UNKNOWN, classify_token("x", 0, &out, &out2));
}

void test_classify_token_punctuation(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_COMMA, classify_token(",", 1, &out, &out2));
  TEST_ASSERT_EQUAL(TOKEN_LPARAN, classify_token("(", 1, &out, &out2));
  TEST_ASSERT_EQUAL(TOKEN_RPARAN, classify_token(")", 1, &out, &out2));
}

void test_classify_token_string(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_STRING, classify_token("\"hi\"", 4, &out, &out2));
}

void test_classify_token_immediate(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_IMMEDIATE, classify_token("123", 3, &out, &out2));
  TEST_ASSERT_EQUAL_INT(123, out);
}

void test_classify_token_label(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_LABEL, classify_token("loop:", 5, &out, &out2));
}

void test_classify_token_directive_case_insensitive(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_DIRECTIVE, classify_token(".WORD", 5, &out, &out2));
  TEST_ASSERT_EQUAL(DIRECTIVE_WORD, (Directive)out);
}

void test_classify_token_register(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_REGISTER, classify_token("sp", 2, &out, &out2));
  TEST_ASSERT_EQUAL_INT(2, out);
}

void test_classify_token_mnemonic_only(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_MNEMONIC, classify_token("add", 3, &out, &out2));
  TEST_ASSERT_EQUAL(OP_ADD, (Opcode)out);
  TEST_ASSERT_EQUAL(PSEUDO_UNKNOWN, (PseudoOpcode)out2);
}

void test_classify_token_pseudo_only(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_PSEUDO, classify_token("nop", 3, &out, &out2));
  TEST_ASSERT_EQUAL(OP_UNKNOWN, (Opcode)out);
  TEST_ASSERT_EQUAL(PSEUDO_NOP, (PseudoOpcode)out2);
}

void test_classify_token_ambiguous(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_INSTRUCTION_AMBIGUOUS, classify_token("jal", 3, &out, &out2));
  TEST_ASSERT_EQUAL(OP_JAL, (Opcode)out);
  TEST_ASSERT_EQUAL(PSEUDO_JAL, (PseudoOpcode)out2);
}

void test_classify_token_symbol(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_SYMBOL, classify_token("mylabel_1", 9, &out, &out2));
}

void test_classify_token_unknown(void) {
  int out = 0, out2 = 0;
  TEST_ASSERT_EQUAL(TOKEN_UNKNOWN, classify_token("1abc", 4, &out, &out2));
}

/* ---------------------------------------------------------------------
 * tokenize_line
 * ------------------------------------------------------------------- */

void test_tokenize_line_blank_returns_null(void) {
  int had_error = 0;
  TEST_ASSERT_NULL(tokenize_line(&arena, "   ", 3, 1, &had_error));
  TEST_ASSERT_EQUAL_INT(0, had_error);
}

void test_tokenize_line_comment_only_returns_null(void) {
  int had_error = 0;
  TEST_ASSERT_NULL(tokenize_line(&arena, "# comment", 9, 1, &had_error));
  TEST_ASSERT_EQUAL_INT(0, had_error);
}

void test_tokenize_line_simple_instruction(void) {
  const char* line = "add a0, a1, a2";
  Token* head = tokenize_line(&arena, line, strlen(line), 1, NULL);

  TEST_ASSERT_NOT_NULL(head);
  TEST_ASSERT_EQUAL(TOKEN_MNEMONIC, head->type);
  TEST_ASSERT_EQUAL(OP_ADD, head->opcode.op);

  Token* t1 = head->next;
  TEST_ASSERT_EQUAL(TOKEN_REGISTER, t1->type);
  TEST_ASSERT_EQUAL_INT(10, t1->reg);

  Token* t2 = t1->next;
  TEST_ASSERT_EQUAL(TOKEN_COMMA, t2->type);

  Token* t3 = t2->next;
  TEST_ASSERT_EQUAL(TOKEN_REGISTER, t3->type);
  TEST_ASSERT_EQUAL_INT(11, t3->reg);

  free_token_list(head);
}

void test_tokenize_line_label_strips_colon(void) {
  const char* line = "loop:";
  Token* head = tokenize_line(&arena, line, strlen(line), 1, NULL);

  TEST_ASSERT_NOT_NULL(head);
  TEST_ASSERT_EQUAL(TOKEN_LABEL, head->type);
  TEST_ASSERT_EQUAL_UINT(4, head->len);
  TEST_ASSERT_EQUAL_STRING("loop", head->start);

  free_token_list(head);
}

void test_tokenize_line_memory_operand_columns(void) {
  const char* line = "lw a0, 4(sp)";
  Token* head = tokenize_line(&arena, line, strlen(line), 1, NULL);

  TEST_ASSERT_NOT_NULL(head);
  /* lw a0 , 4 ( sp ) */
  Token* t = head;
  TEST_ASSERT_EQUAL(TOKEN_MNEMONIC, t->type);
  TEST_ASSERT_EQUAL_UINT32(1, t->col);
  t = t->next;
  TEST_ASSERT_EQUAL(TOKEN_REGISTER, t->type);
  TEST_ASSERT_EQUAL_UINT32(4, t->col);
  t = t->next;
  TEST_ASSERT_EQUAL(TOKEN_COMMA, t->type);
  t = t->next;
  TEST_ASSERT_EQUAL(TOKEN_IMMEDIATE, t->type);
  TEST_ASSERT_EQUAL_INT(4, t->imm);
  t = t->next;
  TEST_ASSERT_EQUAL(TOKEN_LPARAN, t->type);
  t = t->next;
  TEST_ASSERT_EQUAL(TOKEN_REGISTER, t->type);
  TEST_ASSERT_EQUAL_INT(2, t->reg);
  t = t->next;
  TEST_ASSERT_EQUAL(TOKEN_RPARAN, t->type);
  TEST_ASSERT_NULL(t->next);

  free_token_list(head);
}

void test_tokenize_line_string_directive(void) {
  const char* line = ".string \"hi\\n\"";
  Token* head = tokenize_line(&arena, line, strlen(line), 1, NULL);

  TEST_ASSERT_NOT_NULL(head);
  TEST_ASSERT_EQUAL(TOKEN_DIRECTIVE, head->type);

  Token* str = head->next;
  TEST_ASSERT_EQUAL(TOKEN_STRING, str->type);
  TEST_ASSERT_EQUAL_UINT(3, str->len);
  TEST_ASSERT_EQUAL_CHAR('h', str->start[0]);
  TEST_ASSERT_EQUAL_CHAR('i', str->start[1]);
  TEST_ASSERT_EQUAL_CHAR('\n', str->start[2]);

  free_token_list(head);
}

void test_tokenize_line_unterminated_string_fails(void) {
  const char* line = ".string \"unterminated";
  int had_error = 0;
  TEST_ASSERT_NULL(tokenize_line(&arena, line, strlen(line), 1, &had_error));
  TEST_ASSERT_EQUAL_INT(1, had_error);
}

void test_tokenize_line_invalid_escape_in_string_fails(void) {
  const char* line = ".string \"bad\\qescape\"";
  int had_error = 0;
  TEST_ASSERT_NULL(tokenize_line(&arena, line, strlen(line), 1, &had_error));
  TEST_ASSERT_EQUAL_INT(1, had_error);
}

void test_tokenize_line_unknown_token_should_not_be_silently_accepted(void) {
  const char* line = "$";
  int had_error = 0;
  Token* result = tokenize_line(&arena, line, strlen(line), 1, &had_error);
  if (result)
    free_token_list(result);
  TEST_ASSERT_NULL(result);
  TEST_ASSERT_EQUAL_INT(1, had_error);
}

/* ---------------------------------------------------------------------
 * tokenize_input / free_tokenized_input
 * ------------------------------------------------------------------- */

void test_tokenize_input_multiple_lines(void) {
  const char* src = "add a0, a1, a2\nsub a0, a1, a2\n";
  TokenizedInput* result = tokenize_input(src);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(2, result->count);
  TEST_ASSERT_EQUAL(OP_ADD, result->lines[0]->opcode.op);
  TEST_ASSERT_EQUAL(OP_SUB, result->lines[1]->opcode.op);

  free_tokenized_input(result);
}

void test_tokenize_input_no_trailing_newline(void) {
  const char* src = "add a0, a1, a2\nsub a0, a1, a2";
  TokenizedInput* result = tokenize_input(src);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(2, result->count);

  free_tokenized_input(result);
}

void test_tokenize_input_blank_lines_are_skipped(void) {
  const char* src = "add a0, a1, a2\n\n\nsub a0, a1, a2\n";
  TokenizedInput* result = tokenize_input(src);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(2, result->count);

  free_tokenized_input(result);
}

void test_tokenize_input_trailing_comment_only_line_should_not_fail(void) {
  const char* src = "add a0, a1, a2\n# trailing comment";
  TokenizedInput* result = tokenize_input(src);

  int was_null = (result == NULL);
  size_t count = result ? result->count : 0;
  if (result)
    free_tokenized_input(result);
  TEST_ASSERT_FALSE(was_null);
  TEST_ASSERT_EQUAL_UINT(1, count);
}

void test_tokenize_input_mid_stream_lexer_error_should_propagate(void) {
  const char* src = "add a0, a1, a2\n.string \"unterminated\nsub a0, a1, a2\n";
  TokenizedInput* result = tokenize_input(src);

  int was_null = (result == NULL);
  if (result)
    free_tokenized_input(result);
  TEST_ASSERT_TRUE(was_null);
}

void test_free_tokenized_input_null_does_not_crash(void) {
  free_tokenized_input(NULL);
  TEST_PASS();
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_create_token_null_arena_returns_null);
  RUN_TEST(test_create_token_null_start_returns_null);
  RUN_TEST(test_create_token_basic_fields);
  RUN_TEST(test_create_token_register_sets_reg_field);
  RUN_TEST(test_create_token_immediate_sets_imm_field);
  RUN_TEST(test_create_token_directive_sets_dir_field);
  RUN_TEST(test_create_token_zero_len_is_empty_string);
  RUN_TEST(test_create_token_arena_exhausted_returns_null);
  RUN_TEST(test_create_token_instruction_rejects_wrong_type);
  RUN_TEST(test_create_token_instruction_sets_opcode_fields);

  RUN_TEST(test_append_token_null_args_no_crash);
  RUN_TEST(test_append_token_first_sets_head_and_tail);
  RUN_TEST(test_append_token_chains_multiple);

  RUN_TEST(test_is_valid_symbol_leading_digit_invalid);
  RUN_TEST(test_is_valid_symbol_valid_chars);
  RUN_TEST(test_is_valid_symbol_uppercase_invalid);
  RUN_TEST(test_is_valid_symbol_disallowed_char_invalid);
  RUN_TEST(test_is_valid_symbol_zero_length_invalid);

  RUN_TEST(test_is_valid_immediate_decimal_positive);
  RUN_TEST(test_is_valid_immediate_decimal_negative);
  RUN_TEST(test_is_valid_immediate_hex);
  RUN_TEST(test_is_valid_immediate_hex_too_short_invalid);
  RUN_TEST(test_is_valid_immediate_hex_invalid_digits);
  RUN_TEST(test_is_valid_immediate_trailing_garbage_invalid);
  RUN_TEST(test_is_valid_immediate_char_literal);
  RUN_TEST(test_is_valid_immediate_out_of_int32_range_invalid);
  RUN_TEST(test_is_valid_immediate_zero_length_incorrectly_valid);

  RUN_TEST(test_is_valid_string_normal);
  RUN_TEST(test_is_valid_string_empty_contents);
  RUN_TEST(test_is_valid_string_too_short_invalid);
  RUN_TEST(test_is_valid_string_missing_closing_quote_invalid);

  RUN_TEST(test_unescape_string_no_escapes);
  RUN_TEST(test_unescape_string_all_supported_escapes);
  RUN_TEST(test_unescape_string_trailing_backslash_fails);
  RUN_TEST(test_unescape_string_invalid_escape_char_fails);
  RUN_TEST(test_unescape_string_null_out_len_returns_null);

  RUN_TEST(test_classify_token_null_or_zero_len);
  RUN_TEST(test_classify_token_punctuation);
  RUN_TEST(test_classify_token_string);
  RUN_TEST(test_classify_token_immediate);
  RUN_TEST(test_classify_token_label);
  RUN_TEST(test_classify_token_directive_case_insensitive);
  RUN_TEST(test_classify_token_register);
  RUN_TEST(test_classify_token_mnemonic_only);
  RUN_TEST(test_classify_token_pseudo_only);
  RUN_TEST(test_classify_token_ambiguous);
  RUN_TEST(test_classify_token_symbol);
  RUN_TEST(test_classify_token_unknown);

  RUN_TEST(test_tokenize_line_blank_returns_null);
  RUN_TEST(test_tokenize_line_comment_only_returns_null);
  RUN_TEST(test_tokenize_line_simple_instruction);
  RUN_TEST(test_tokenize_line_label_strips_colon);
  RUN_TEST(test_tokenize_line_memory_operand_columns);
  RUN_TEST(test_tokenize_line_string_directive);
  RUN_TEST(test_tokenize_line_unterminated_string_fails);
  RUN_TEST(test_tokenize_line_invalid_escape_in_string_fails);
  RUN_TEST(test_tokenize_line_unknown_token_should_not_be_silently_accepted);

  RUN_TEST(test_tokenize_input_multiple_lines);
  RUN_TEST(test_tokenize_input_no_trailing_newline);
  RUN_TEST(test_tokenize_input_blank_lines_are_skipped);
  RUN_TEST(test_tokenize_input_trailing_comment_only_line_should_not_fail);
  RUN_TEST(test_tokenize_input_mid_stream_lexer_error_should_propagate);
  RUN_TEST(test_free_tokenized_input_null_does_not_crash);

  return UNITY_END();
}