#define _POSIX_C_SOURCE 200809L

#include "../src/second_pass.c"
#include "unity/unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static DirectiveArg make_arg(int imm) {
  DirectiveArg arg = {0};
  arg.type = OPERAND_IMMEDIATE;
  arg.imm = imm;
  return arg;
}

static ParsedLine make_directive_line(Directive dir, DirectiveArg* args, int arg_count) {
  ParsedLine line = {0};
  line.type = LINE_DIRECTIVE;
  line.directive.directive = dir;
  line.directive.args = args;
  line.directive.arg_count = arg_count;
  return line;
}

static Operand make_reg(int reg) {
  Operand op = {0};
  op.type = OPERAND_REGISTER;
  op.reg.reg = reg;
  return op;
}

static ParsedLine make_r_instruction(const char* label, Opcode op, int rd, int rs1, int rs2) {
  ParsedLine line = {0};
  if (label) {
    line.label = (char*)label;
    line.len = strlen(label);
  }
  line.type = LINE_INSTRUCTION;
  line.instruction.type = INSTRUCTION_REAL;
  line.instruction.op = op;
  line.instruction.operand_count = 3;
  line.instruction.operands[0] = make_reg(rd);
  line.instruction.operands[1] = make_reg(rs1);
  line.instruction.operands[2] = make_reg(rs2);
  return line;
}

static ExpandedInput make_expanded_input(ParsedLine* lines, size_t count) {
  ExpandedInput input;
  arena_init(&input.arena, 256);
  input.arena.used = 32;
  input.lines = lines;
  input.count = count;
  return input;
}

static SymbolTable* make_heap_symbol_table(void) {
  SymbolTable* table = malloc(sizeof(SymbolTable));
  memset(table, 0, sizeof(SymbolTable));
  arena_init(&table->arena, 64);
  return table;
}

static Symbol* make_heap_symbol(const char* name, size_t len, uint32_t value, SymbolType type) {
  Symbol* sym = malloc(sizeof(Symbol));
  memset(sym, 0, sizeof(Symbol));
  sym->type = type;
  sym->name = (char*)name;
  sym->len = len;
  sym->value = value;
  return sym;
}

/* ---------------------------------------------------------------------
 * push_word / push_half / push_string / push_byte
 * ------------------------------------------------------------------- */

void test_push_word_null_arr_fails(void) { TEST_ASSERT_EQUAL_INT(0, push_word(NULL, 0x1234)); }

void test_push_word_little_endian(void) {
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, push_word(&arr, 0x12345678));
  uint8_t* bytes = (uint8_t*)arr.data;
  TEST_ASSERT_EQUAL_UINT8(0x78, bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0x56, bytes[1]);
  TEST_ASSERT_EQUAL_UINT8(0x34, bytes[2]);
  TEST_ASSERT_EQUAL_UINT8(0x12, bytes[3]);
  TEST_ASSERT_EQUAL_UINT(4, arr.count);

  dynamic_array_free(&arr);
}

void test_push_half_null_arr_fails(void) { TEST_ASSERT_EQUAL_INT(0, push_half(NULL, 0x1234)); }

void test_push_half_little_endian(void) {
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, push_half(&arr, 0x1234));
  uint8_t* bytes = (uint8_t*)arr.data;
  TEST_ASSERT_EQUAL_UINT8(0x34, bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0x12, bytes[1]);
  TEST_ASSERT_EQUAL_UINT(2, arr.count);

  dynamic_array_free(&arr);
}

void test_push_string_null_args_fail(void) {
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);
  TEST_ASSERT_EQUAL_INT(0, push_string(NULL, "hi", 2, 1));
  TEST_ASSERT_EQUAL_INT(0, push_string(&arr, NULL, 2, 1));
  dynamic_array_free(&arr);
}

void test_push_string_null_terminated(void) {
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, push_string(&arr, "hi", 2, 1));
  TEST_ASSERT_EQUAL_UINT(3, arr.count);
  uint8_t* bytes = (uint8_t*)arr.data;
  TEST_ASSERT_EQUAL_UINT8('h', bytes[0]);
  TEST_ASSERT_EQUAL_UINT8('i', bytes[1]);
  TEST_ASSERT_EQUAL_UINT8(0, bytes[2]);

  dynamic_array_free(&arr);
}

void test_push_string_no_null_terminator(void) {
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, push_string(&arr, "hi", 2, 0));
  TEST_ASSERT_EQUAL_UINT(2, arr.count);

  dynamic_array_free(&arr);
}

void test_push_byte_null_arr_fails(void) { TEST_ASSERT_EQUAL_INT(0, push_byte(NULL, 5)); }

void test_push_byte_basic(void) {
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, push_byte(&arr, 0xAB));
  TEST_ASSERT_EQUAL_UINT(1, arr.count);
  TEST_ASSERT_EQUAL_UINT8(0xAB, ((uint8_t*)arr.data)[0]);

  dynamic_array_free(&arr);
}

/* ---------------------------------------------------------------------
 * handle_directive_data
 * ------------------------------------------------------------------- */

void test_handle_directive_data_wrong_line_type_fails(void) {
  ParsedLine line = {0};
  line.type = LINE_INSTRUCTION;
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(0, handle_directive_data(&line, &arr));
  dynamic_array_free(&arr);
}

void test_handle_directive_data_byte(void) {
  DirectiveArg args[2] = {make_arg(1), make_arg(2)};
  args[0].next = &args[1];
  ParsedLine line = make_directive_line(DIRECTIVE_BYTE, args, 2);
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, handle_directive_data(&line, &arr));
  TEST_ASSERT_EQUAL_UINT(2, arr.count);
  TEST_ASSERT_EQUAL_UINT8(1, ((uint8_t*)arr.data)[0]);
  TEST_ASSERT_EQUAL_UINT8(2, ((uint8_t*)arr.data)[1]);

  dynamic_array_free(&arr);
}

void test_handle_directive_data_word(void) {
  DirectiveArg args[1] = {make_arg(0x1234)};
  ParsedLine line = make_directive_line(DIRECTIVE_WORD, args, 1);
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, handle_directive_data(&line, &arr));
  TEST_ASSERT_EQUAL_UINT(4, arr.count);

  dynamic_array_free(&arr);
}

void test_handle_directive_data_string(void) {
  DirectiveArg arg = {0};
  arg.type = OPERAND_STRING;
  arg.start = "hi";
  arg.len = 2;
  ParsedLine line = make_directive_line(DIRECTIVE_STRING, &arg, 1);
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, handle_directive_data(&line, &arr));
  TEST_ASSERT_EQUAL_UINT(3, arr.count); /* "hi" + null */

  dynamic_array_free(&arr);
}

void test_handle_directive_data_zero(void) {
  DirectiveArg arg = make_arg(5);
  ParsedLine line = make_directive_line(DIRECTIVE_ZERO, &arg, 1);
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, handle_directive_data(&line, &arr));
  TEST_ASSERT_EQUAL_UINT(5, arr.count);

  dynamic_array_free(&arr);
}

void test_handle_directive_data_zero_negative_silently_noops(void) {
  DirectiveArg arg = make_arg(-5);
  ParsedLine line = make_directive_line(DIRECTIVE_ZERO, &arg, 1);
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  TEST_ASSERT_EQUAL_INT(1, handle_directive_data(&line, &arr));
  TEST_ASSERT_EQUAL_UINT(0, arr.count);

  dynamic_array_free(&arr);
}

void test_handle_directive_data_equ_should_be_a_noop(void) {
  DirectiveArg name_arg = {0};
  name_arg.type = OPERAND_SYMBOL;
  name_arg.start = "SIZE";
  name_arg.len = 4;
  DirectiveArg value_arg = make_arg(10);
  name_arg.next = &value_arg;

  ParsedLine line = make_directive_line(DIRECTIVE_EQU, &name_arg, 2);
  DynamicArray arr;
  dynamic_array_init(&arr, sizeof(uint8_t), 16);

  int result = handle_directive_data(&line, &arr);
  size_t count = arr.count;
  dynamic_array_free(&arr);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL_UINT(0, count);
}

/* ---------------------------------------------------------------------
 * get_entry_point
 * ------------------------------------------------------------------- */

void test_get_entry_point_prefers_start(void) {
  Symbol start = {0};
  start.type = SYMBOL_LABEL;
  start.name = "_start";
  start.len = 6;
  start.value = 100;
  Symbol main_sym = {0};
  main_sym.type = SYMBOL_LABEL;
  main_sym.name = "main";
  main_sym.len = 4;
  main_sym.value = 200;
  start.next = &main_sym;

  SymbolTable table = {0};
  table.head = &start;

  TEST_ASSERT_EQUAL_UINT32(100, get_entry_point(&table));
}

void test_get_entry_point_falls_back_to_main(void) {
  Symbol main_sym = {0};
  main_sym.type = SYMBOL_LABEL;
  main_sym.name = "main";
  main_sym.len = 4;
  main_sym.value = 200;

  SymbolTable table = {0};
  table.head = &main_sym;

  TEST_ASSERT_EQUAL_UINT32(200, get_entry_point(&table));
}

void test_get_entry_point_defaults_to_zero(void) {
  SymbolTable table = {0};
  table.head = NULL;

  TEST_ASSERT_EQUAL_UINT32(0, get_entry_point(&table));
}

void test_get_entry_point_ignores_wrong_type(void) {
  Symbol start = {0};
  start.type = SYMBOL_CONSTANT; /* not a SYMBOL_LABEL */
  start.name = "_start";
  start.len = 6;
  start.value = 100;

  SymbolTable table = {0};
  table.head = &start;

  TEST_ASSERT_EQUAL_UINT32(0, get_entry_point(&table));
}

/* ---------------------------------------------------------------------
 * finalize_assembly / free_assembled_program
 * ------------------------------------------------------------------- */

void test_finalize_assembly_null_args_fail(void) {
  SymbolTable table = {0};
  ExpandedInput input = make_expanded_input(NULL, 1);
  TEST_ASSERT_NULL(finalize_assembly(NULL, &table));
  TEST_ASSERT_NULL(finalize_assembly(&input, NULL));
  arena_free(&input.arena);
}

void test_finalize_assembly_empty_input_should_succeed(void) {
  SymbolTable* table = make_heap_symbol_table();
  ExpandedInput input = make_expanded_input(NULL, 0);

  AssembledProgram* result = finalize_assembly(&input, table);

  int succeeded = (result != NULL);
  size_t text_size = result ? result->text.size : 0;
  if (result)
    free_assembled_program(result);
  else
    free_symbol_table(table);
  arena_free(&input.arena);

  TEST_ASSERT_TRUE(succeeded);
  TEST_ASSERT_EQUAL_UINT(0, text_size);
}

void test_finalize_assembly_simple_text_program(void) {
  ParsedLine lines[1] = {make_r_instruction(NULL, OP_ADD, 1, 2, 3)};
  ExpandedInput input = make_expanded_input(lines, 1);
  SymbolTable* table = make_heap_symbol_table();

  AssembledProgram* result = finalize_assembly(&input, table);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(4, result->text.size);
  TEST_ASSERT_EQUAL_UINT8(0x33, result->text.data[0] & 0x7F);

  free_assembled_program(result); /* also frees table */
  arena_free(&input.arena);
}

void test_finalize_assembly_pseudo_instruction_fails(void) {
  ParsedLine lines[1] = {0};
  lines[0].type = LINE_INSTRUCTION;
  lines[0].instruction.type = INSTRUCTION_PSEUDO;
  lines[0].instruction.pseudo = PSEUDO_NOP;
  ExpandedInput input = make_expanded_input(lines, 1);
  SymbolTable table = {0};

  TEST_ASSERT_NULL(finalize_assembly(&input, &table));
  arena_free(&input.arena);
}

void test_finalize_assembly_instruction_outside_text_fails(void) {
  ParsedLine lines[2] = {0};
  lines[0] = make_directive_line(DIRECTIVE_DATA, NULL, 0);
  lines[1] = make_r_instruction(NULL, OP_ADD, 1, 2, 3);
  ExpandedInput input = make_expanded_input(lines, 2);
  SymbolTable table = {0};

  TEST_ASSERT_NULL(finalize_assembly(&input, &table));
  arena_free(&input.arena);
}

void test_finalize_assembly_unknown_directive_fails(void) {
  ParsedLine lines[1] = {make_directive_line(DIRECTIVE_UNKNOWN, NULL, 0)};
  ExpandedInput input = make_expanded_input(lines, 1);
  SymbolTable table = {0};

  TEST_ASSERT_NULL(finalize_assembly(&input, &table));
  arena_free(&input.arena);
}

void test_finalize_assembly_directive_in_text_segment_fails(void) {
  DirectiveArg arg = make_arg(5);
  ParsedLine lines[1] = {make_directive_line(DIRECTIVE_ZERO, &arg, 1)};
  ExpandedInput input = make_expanded_input(lines, 1);
  SymbolTable table = {0};

  TEST_ASSERT_NULL(finalize_assembly(&input, &table));
  arena_free(&input.arena);
}

void test_finalize_assembly_data_segment_directive(void) {
  DirectiveArg arg = make_arg(0xAB);
  ParsedLine lines[2] = {0};
  lines[0] = make_directive_line(DIRECTIVE_DATA, NULL, 0);
  lines[1] = make_directive_line(DIRECTIVE_WORD, &arg, 1);
  ExpandedInput input = make_expanded_input(lines, 2);
  SymbolTable* table = make_heap_symbol_table();

  AssembledProgram* result = finalize_assembly(&input, table);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(0, result->text.size);
  TEST_ASSERT_EQUAL_UINT(4, result->data.size);

  free_assembled_program(result); /* also frees table */
  arena_free(&input.arena);
}

void test_finalize_assembly_entry_offset_uses_start_symbol(void) {
  SymbolTable* table = make_heap_symbol_table();
  table->head = make_heap_symbol("_start", 6, 0, SYMBOL_LABEL);

  ParsedLine lines[1] = {make_r_instruction("_start", OP_ADD, 1, 2, 3)};
  ExpandedInput input = make_expanded_input(lines, 1);

  AssembledProgram* result = finalize_assembly(&input, table);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT32(0, result->entry_offset);

  free_assembled_program(result); /* also frees table and its Symbol node */
  arena_free(&input.arena);
}

void test_free_assembled_program_null_does_not_crash(void) {
  free_assembled_program(NULL);
  TEST_PASS();
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_push_word_null_arr_fails);
  RUN_TEST(test_push_word_little_endian);
  RUN_TEST(test_push_half_null_arr_fails);
  RUN_TEST(test_push_half_little_endian);
  RUN_TEST(test_push_string_null_args_fail);
  RUN_TEST(test_push_string_null_terminated);
  RUN_TEST(test_push_string_no_null_terminator);
  RUN_TEST(test_push_byte_null_arr_fails);
  RUN_TEST(test_push_byte_basic);

  RUN_TEST(test_handle_directive_data_wrong_line_type_fails);
  RUN_TEST(test_handle_directive_data_byte);
  RUN_TEST(test_handle_directive_data_word);
  RUN_TEST(test_handle_directive_data_string);
  RUN_TEST(test_handle_directive_data_zero);
  RUN_TEST(test_handle_directive_data_zero_negative_silently_noops);
  RUN_TEST(test_handle_directive_data_equ_should_be_a_noop);

  RUN_TEST(test_get_entry_point_prefers_start);
  RUN_TEST(test_get_entry_point_falls_back_to_main);
  RUN_TEST(test_get_entry_point_defaults_to_zero);
  RUN_TEST(test_get_entry_point_ignores_wrong_type);

  RUN_TEST(test_finalize_assembly_null_args_fail);
  RUN_TEST(test_finalize_assembly_empty_input_should_succeed);
  RUN_TEST(test_finalize_assembly_simple_text_program);
  RUN_TEST(test_finalize_assembly_pseudo_instruction_fails);
  RUN_TEST(test_finalize_assembly_instruction_outside_text_fails);
  RUN_TEST(test_finalize_assembly_unknown_directive_fails);
  RUN_TEST(test_finalize_assembly_directive_in_text_segment_fails);
  RUN_TEST(test_finalize_assembly_data_segment_directive);
  RUN_TEST(test_finalize_assembly_entry_offset_uses_start_symbol);
  RUN_TEST(test_free_assembled_program_null_does_not_crash);

  return UNITY_END();
}