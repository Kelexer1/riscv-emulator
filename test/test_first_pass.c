#define _POSIX_C_SOURCE 200809L

#include "../src/first_pass.c"
#include "unity/unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static ExpandedInput make_expanded_input(ParsedLine* lines, size_t count) {
  ExpandedInput input;
  arena_init(&input.arena, 512);
  input.arena.used = 64;
  input.lines = lines;
  input.count = count;
  return input;
}

static ParsedLine make_instruction_line(const char* label, Opcode op) {
  ParsedLine line = {0};
  line.type = LINE_INSTRUCTION;
  if (label) {
    line.label = (char*)label;
    line.len = strlen(label);
  }
  line.instruction.type = INSTRUCTION_REAL;
  line.instruction.op = op;
  return line;
}

static ParsedLine make_section_line(Directive dir) {
  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = dir;
  return line;
}

/* ---------------------------------------------------------------------
 * append_symbol
 * ------------------------------------------------------------------- */

void test_append_symbol_null_args_no_crash(void) {
  Symbol sym = {0};
  Symbol* head = NULL;
  Symbol* tail = NULL;

  append_symbol(NULL, &tail, &sym);
  append_symbol(&head, NULL, &sym);
  append_symbol(&head, &tail, NULL);

  TEST_ASSERT_NULL(head);
}

void test_append_symbol_chains_multiple(void) {
  Symbol a = {0};
  Symbol b = {0};
  Symbol* head = NULL;
  Symbol* tail = NULL;

  append_symbol(&head, &tail, &a);
  append_symbol(&head, &tail, &b);

  TEST_ASSERT_EQUAL_PTR(&a, head);
  TEST_ASSERT_EQUAL_PTR(&b, tail);
  TEST_ASSERT_EQUAL_PTR(&b, a.next);
}

/* ---------------------------------------------------------------------
 * resolve_symbol
 * ------------------------------------------------------------------- */

void test_resolve_symbol_null_args_fails(void) {
  SymbolTable table = {0};
  TEST_ASSERT_EQUAL_INT(0, resolve_symbol(NULL, "x", 1, NULL));
  TEST_ASSERT_EQUAL_INT(0, resolve_symbol(&table, NULL, 1, NULL));
}

void test_resolve_symbol_found_copies_fields(void) {
  Symbol sym = {0};
  sym.type = SYMBOL_LABEL;
  sym.value = 100;
  sym.name = "loop";
  sym.len = 4;
  SymbolTable table = {0};
  table.head = &sym;

  Symbol out;
  int result = resolve_symbol(&table, "loop", 4, &out);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL_UINT(100, out.value);
}

void test_resolve_symbol_not_found(void) {
  Symbol sym = {0};
  sym.name = "loop";
  sym.len = 4;
  SymbolTable table = {0};
  table.head = &sym;

  TEST_ASSERT_EQUAL_INT(0, resolve_symbol(&table, "other", 5, NULL));
}

void test_resolve_symbol_null_out_no_crash(void) {
  Symbol sym = {0};
  sym.name = "loop";
  sym.len = 4;
  SymbolTable table = {0};
  table.head = &sym;

  TEST_ASSERT_EQUAL_INT(1, resolve_symbol(&table, "loop", 4, NULL));
}

/* ---------------------------------------------------------------------
 * value_to_symbol
 * ------------------------------------------------------------------- */

void test_value_to_symbol_null_table_returns_null(void) { TEST_ASSERT_NULL(value_to_symbol(NULL, 0, SYMBOL_LABEL)); }

void test_value_to_symbol_match(void) {
  Symbol sym = {0};
  sym.value = 42;
  sym.type = SYMBOL_LABEL;
  SymbolTable table = {0};
  table.head = &sym;

  const Symbol* result = value_to_symbol(&table, 42, SYMBOL_LABEL);

  TEST_ASSERT_EQUAL_PTR(&sym, result);
}

void test_value_to_symbol_wrong_type_no_match(void) {
  Symbol sym = {0};
  sym.value = 42;
  sym.type = SYMBOL_LABEL;
  SymbolTable table = {0};
  table.head = &sym;

  TEST_ASSERT_NULL(value_to_symbol(&table, 42, SYMBOL_CONSTANT));
}

void test_value_to_symbol_no_match(void) {
  Symbol sym = {0};
  sym.value = 42;
  sym.type = SYMBOL_LABEL;
  SymbolTable table = {0};
  table.head = &sym;

  TEST_ASSERT_NULL(value_to_symbol(&table, 99, SYMBOL_LABEL));
}

/* ---------------------------------------------------------------------
 * assemble_symbol_table
 * ------------------------------------------------------------------- */

void test_assemble_symbol_table_null_returns_null(void) { TEST_ASSERT_NULL(assemble_symbol_table(NULL)); }

void test_assemble_symbol_table_simple_text_label(void) {
  ParsedLine lines[] = {
      make_instruction_line("start", OP_ADD),
      make_instruction_line(NULL, OP_ADD),
  };
  ExpandedInput input = make_expanded_input(lines, 2);

  SymbolTable* table = assemble_symbol_table(&input);

  TEST_ASSERT_NOT_NULL(table);
  Symbol out = {0};
  int found = resolve_symbol(table, "start", 5, &out);
  uint32_t value = out.value;
  free_symbol_table(table);
  arena_free(&input.arena);

  TEST_ASSERT_EQUAL_INT(1, found);
  TEST_ASSERT_EQUAL_UINT32(0, value); /* base_text=0, first instruction */
}

void test_assemble_symbol_table_duplicate_label_fails(void) {
  ParsedLine lines[] = {
      make_instruction_line("dup", OP_ADD),
      make_instruction_line("dup", OP_ADD),
  };
  ExpandedInput input = make_expanded_input(lines, 2);
  SymbolTable* table = assemble_symbol_table(&input);

  TEST_ASSERT_NULL(table);
  arena_free(&input.arena);
}

void test_assemble_symbol_table_instruction_outside_text_fails(void) {
  ParsedLine lines[] = {
      make_section_line(DIRECTIVE_DATA),
      make_instruction_line(NULL, OP_ADD),
  };
  ExpandedInput input = make_expanded_input(lines, 2);

  TEST_ASSERT_NULL(assemble_symbol_table(&input));
  arena_free(&input.arena);
}

void test_assemble_symbol_table_pseudo_instruction_fails(void) {
  ParsedLine lines[1] = {0};
  lines[0].type = LINE_INSTRUCTION;
  lines[0].instruction.type = INSTRUCTION_PSEUDO;
  lines[0].instruction.pseudo = PSEUDO_NOP;
  ExpandedInput input = make_expanded_input(lines, 1);

  TEST_ASSERT_NULL(assemble_symbol_table(&input));
  arena_free(&input.arena);
}

void test_assemble_symbol_table_unknown_directive_fails(void) {
  ParsedLine lines[] = {make_section_line(DIRECTIVE_UNKNOWN)};
  ExpandedInput input = make_expanded_input(lines, 1);

  TEST_ASSERT_NULL(assemble_symbol_table(&input));
  arena_free(&input.arena);
}

void test_assemble_symbol_table_data_label_after_text(void) {
  DirectiveArg word_arg = {0};
  word_arg.type = OPERAND_IMMEDIATE;
  word_arg.imm = 5;

  ParsedLine lines[3] = {0};
  lines[0] = make_instruction_line(NULL, OP_ADD); /* lc_text: 0 -> 4 */
  lines[1] = make_section_line(DIRECTIVE_DATA);
  lines[2].type = LINE_DIRECTIVE;
  lines[2].label = "myvar";
  lines[2].len = 5;
  lines[2].directive.directive = DIRECTIVE_WORD;
  lines[2].directive.arg_count = 1;
  lines[2].directive.args = &word_arg;

  ExpandedInput input = make_expanded_input(lines, 3);
  SymbolTable* table = assemble_symbol_table(&input);

  TEST_ASSERT_NOT_NULL(table);
  Symbol out = {0};
  int found = resolve_symbol(table, "myvar", 5, &out);
  free_symbol_table(table);
  arena_free(&input.arena);

  TEST_ASSERT_EQUAL_INT(1, found);
  /* base_text=0, lc_text=4 -> base_rodata rounds up to 4096 (empty rodata).
   * Since lc_rodata=0, base_data stays at 4096 too (rounding an
   * already-aligned base by adding zero doesn't move it). myvar's local
   * offset within .data was 0 (recorded before its own directive size is
   * added), so its final absolute value is base_data + 0 = 4096. */
  TEST_ASSERT_EQUAL_UINT32(4096, out.value);
}

void test_assemble_symbol_table_equ_constant(void) {
  DirectiveArg name_arg = {0};
  name_arg.type = OPERAND_SYMBOL;
  name_arg.start = "SIZE";
  name_arg.len = 4;

  DirectiveArg value_arg = {0};
  value_arg.type = OPERAND_IMMEDIATE;
  value_arg.imm = 10;
  name_arg.next = &value_arg;

  ParsedLine lines[1] = {0};
  lines[0].type = LINE_DIRECTIVE;
  lines[0].directive.directive = DIRECTIVE_EQU;
  lines[0].directive.arg_count = 2;
  lines[0].directive.args = &name_arg;

  ExpandedInput input = make_expanded_input(lines, 1);
  SymbolTable* table = assemble_symbol_table(&input);

  TEST_ASSERT_NOT_NULL(table);
  Symbol out = {0};
  int found = resolve_symbol(table, "SIZE", 4, &out);
  free_symbol_table(table);
  arena_free(&input.arena);

  TEST_ASSERT_EQUAL_INT(1, found);
  TEST_ASSERT_EQUAL(SYMBOL_CONSTANT, out.type);
  TEST_ASSERT_EQUAL_UINT32(10, out.value); /* not relocated, unlike SYMBOL_LABEL */
}

void test_free_symbol_table_null_does_not_crash(void) {
  free_symbol_table(NULL);
  TEST_PASS();
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_append_symbol_null_args_no_crash);
  RUN_TEST(test_append_symbol_chains_multiple);

  RUN_TEST(test_resolve_symbol_null_args_fails);
  RUN_TEST(test_resolve_symbol_found_copies_fields);
  RUN_TEST(test_resolve_symbol_not_found);
  RUN_TEST(test_resolve_symbol_null_out_no_crash);

  RUN_TEST(test_value_to_symbol_null_table_returns_null);
  RUN_TEST(test_value_to_symbol_match);
  RUN_TEST(test_value_to_symbol_wrong_type_no_match);
  RUN_TEST(test_value_to_symbol_no_match);

  RUN_TEST(test_assemble_symbol_table_null_returns_null);
  RUN_TEST(test_assemble_symbol_table_simple_text_label);
  RUN_TEST(test_assemble_symbol_table_duplicate_label_fails);
  RUN_TEST(test_assemble_symbol_table_instruction_outside_text_fails);
  RUN_TEST(test_assemble_symbol_table_pseudo_instruction_fails);
  RUN_TEST(test_assemble_symbol_table_unknown_directive_fails);
  RUN_TEST(test_assemble_symbol_table_data_label_after_text);
  RUN_TEST(test_assemble_symbol_table_equ_constant);
  RUN_TEST(test_free_symbol_table_null_does_not_crash);

  return UNITY_END();
}