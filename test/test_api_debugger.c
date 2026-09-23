#define _POSIX_C_SOURCE 200809L

#include "../src/api_debugger.c"
#include "unity/unity.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static ProgramState prog;
static DebuggerState dbg;

void setUp(void) {
  memset(&prog, 0, sizeof(ProgramState));
  memset(&dbg, 0, sizeof(DebuggerState));
  init_decode_table();
}

void tearDown(void) {
  for (int i = 0; i < PT_SIZE; i++) {
    if (prog.pt.l2[i]) {
      for (int j = 0; j < PT_SIZE; j++)
        free(prog.pt.l2[i]->pages[j]);
      free(prog.pt.l2[i]);
    }
  }
}

static void write_word_at(uint32_t addr, uint32_t word) {
  mem_write_u32(&prog.pt, addr, word, MEMORY_READ | MEMORY_WRITE | MEMORY_EXECUTE);
}

static const char* capture_stdout(void (*fn)(void)) {
  static char buffer[2048];
  char path[] = "/tmp/api_debugger_test_XXXXXX";
  int fd = mkstemp(path);
  int saved_stdout = dup(STDOUT_FILENO);

  fflush(stdout);
  dup2(fd, STDOUT_FILENO);

  fn();

  fflush(stdout);
  dup2(saved_stdout, STDOUT_FILENO);
  close(saved_stdout);

  memset(buffer, 0, sizeof(buffer));
  lseek(fd, 0, SEEK_SET);
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
  (void)bytes_read;
  close(fd);
  unlink(path);

  return buffer;
}

/* ---------------------------------------------------------------------
 * debugger_init
 * ------------------------------------------------------------------- */

void test_debugger_init_null_args_no_crash(void) {
  debugger_init(NULL, &prog, NULL, NULL);
  Elf32Image source = {0};
  debugger_init(&dbg, NULL, &source, NULL);
  TEST_PASS();
}

void test_debugger_init_sets_fields(void) {
  Elf32Image source = {0};
  SymbolTable table = {0};
  dbg.is_paused = 1; /* should be reset by memset inside debugger_init */

  debugger_init(&dbg, &prog, &source, &table);

  TEST_ASSERT_EQUAL_PTR(&prog, dbg.prog);
  TEST_ASSERT_EQUAL_PTR(&source, dbg.image);
  TEST_ASSERT_EQUAL_PTR(&table, dbg.symbol_table);
  TEST_ASSERT_EQUAL_INT(0, dbg.is_paused);
}

void test_debugger_init_requires_prog_source(void) {
  debugger_init(&dbg, &prog, NULL, NULL);
  TEST_ASSERT_NULL(dbg.prog);
}

/* ---------------------------------------------------------------------
 * breakpoints
 * ------------------------------------------------------------------- */

void test_debugger_find_breakpoint_null_dbg_should_return_not_found(void) {
  TEST_ASSERT_EQUAL_INT(-1, debugger_find_breakpoint(NULL, 0x1000));
}

void test_debugger_find_breakpoint_not_set_returns_negative_one(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  TEST_ASSERT_EQUAL_INT(-1, debugger_find_breakpoint(&dbg, 0x1000));
}

void test_debugger_add_and_find_breakpoint(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);

  TEST_ASSERT_EQUAL_INT(1, debugger_add_breakpoint(&dbg, 0x1000));
  int idx = debugger_find_breakpoint(&dbg, 0x1000);
  TEST_ASSERT_TRUE(idx >= 0);
  TEST_ASSERT_EQUAL_UINT32(0x1000, dbg.breakpoints[idx].addr);
  TEST_ASSERT_EQUAL_INT(1, dbg.breakpoints[idx].active);
}

void test_debugger_add_breakpoint_duplicate_is_idempotent(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_breakpoint(&dbg, 0x1000);
  debugger_add_breakpoint(&dbg, 0x1000);

  int count = 0;
  for (int i = 0; i < MAX_BREAKPOINTS; i++)
    if (dbg.breakpoints[i].set)
      count++;
  TEST_ASSERT_EQUAL_INT(1, count);
}

void test_debugger_add_breakpoint_table_full_fails(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  for (int i = 0; i < MAX_BREAKPOINTS; i++)
    TEST_ASSERT_EQUAL_INT(1, debugger_add_breakpoint(&dbg, 0x1000 + i * 4));

  TEST_ASSERT_EQUAL_INT(0, debugger_add_breakpoint(&dbg, 0xFFFF));
}

void test_debugger_add_breakpoint_symbol_resolves(void) {
  Symbol sym = {0};
  sym.type = SYMBOL_LABEL;
  sym.name = "loop";
  sym.len = 4;
  sym.value = 0x2000;
  SymbolTable table = {0};
  table.head = &sym;

  debugger_init(&dbg, &prog, &(Elf32Image){0}, &table);

  TEST_ASSERT_EQUAL_INT(1, debugger_add_breakpoint_symbol(&dbg, "loop", 4));
  TEST_ASSERT_TRUE(debugger_find_breakpoint(&dbg, 0x2000) >= 0);
}

void test_debugger_add_breakpoint_symbol_unresolved_fails(void) {
  SymbolTable table = {0};
  table.head = NULL;
  debugger_init(&dbg, &prog, &(Elf32Image){0}, &table);

  TEST_ASSERT_EQUAL_INT(0, debugger_add_breakpoint_symbol(&dbg, "missing", 7));
}

void test_debugger_add_breakpoint_symbol_no_table_fails(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  TEST_ASSERT_EQUAL_INT(0, debugger_add_breakpoint_symbol(&dbg, "loop", 4));
}

void test_debugger_remove_breakpoint(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_breakpoint(&dbg, 0x1000);

  debugger_remove_breakpoint(&dbg, 0x1000);

  TEST_ASSERT_EQUAL_INT(-1, debugger_find_breakpoint(&dbg, 0x1000));
}

void test_debugger_remove_breakpoint_not_found_no_crash(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_remove_breakpoint(&dbg, 0x9999);
  TEST_PASS();
}

void test_debugger_toggle_breakpoint(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_breakpoint(&dbg, 0x1000);

  TEST_ASSERT_EQUAL_INT(1, debugger_toggle_breakpoint(&dbg, 0x1000, 0));
  int idx = debugger_find_breakpoint(&dbg, 0x1000);
  TEST_ASSERT_EQUAL_INT(0, dbg.breakpoints[idx].active);
}

void test_debugger_toggle_breakpoint_not_found_fails(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  TEST_ASSERT_EQUAL_INT(0, debugger_toggle_breakpoint(&dbg, 0x1000, 0));
}

/* ---------------------------------------------------------------------
 * watchpoints
 * ------------------------------------------------------------------- */

void test_debugger_find_watchpoint_null_dbg_should_return_not_found(void) {
  TEST_ASSERT_EQUAL_INT(-1, debugger_find_watchpoint(NULL, 0x1000));
}

void test_debugger_add_watchpoint_mapped_returns_one(void) {
  mem_write_u32(&prog.pt, 0x1000, 42, MEMORY_READ | MEMORY_WRITE);
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);

  int result = debugger_add_watchpoint(&dbg, 0x1000);

  TEST_ASSERT_EQUAL_INT(1, result);
  int idx = debugger_find_watchpoint(&dbg, 0x1000);
  TEST_ASSERT_TRUE(idx >= 0);
  TEST_ASSERT_EQUAL_UINT32(42, dbg.watchpoints[idx].last_val);
}

void test_debugger_add_watchpoint_unmapped_returns_two(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);

  int result = debugger_add_watchpoint(&dbg, 0x9000); /* never mapped */

  TEST_ASSERT_EQUAL_INT(2, result);
  TEST_ASSERT_TRUE(debugger_find_watchpoint(&dbg, 0x9000) >= 0);
}

void test_debugger_add_watchpoint_duplicate_is_idempotent(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_watchpoint(&dbg, 0x1000);
  debugger_add_watchpoint(&dbg, 0x1000);

  int count = 0;
  for (int i = 0; i < MAX_WATCHPOINTS; i++)
    if (dbg.watchpoints[i].set)
      count++;
  TEST_ASSERT_EQUAL_INT(1, count);
}

void test_debugger_remove_watchpoint(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_watchpoint(&dbg, 0x1000);

  debugger_remove_watchpoint(&dbg, 0x1000);

  TEST_ASSERT_EQUAL_INT(-1, debugger_find_watchpoint(&dbg, 0x1000));
}

void test_debugger_toggle_watchpoint(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_watchpoint(&dbg, 0x1000);

  TEST_ASSERT_EQUAL_INT(1, debugger_toggle_watchpoint(&dbg, 0x1000, 0));
  int idx = debugger_find_watchpoint(&dbg, 0x1000);
  TEST_ASSERT_EQUAL_INT(0, dbg.watchpoints[idx].active);
}

/* ---------------------------------------------------------------------
 * check_watchpoints (internal, not in header)
 * ------------------------------------------------------------------- */

void test_check_watchpoints_no_change_returns_zero(void) {
  mem_write_u32(&prog.pt, 0x1000, 42, MEMORY_READ | MEMORY_WRITE);
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_watchpoint(&dbg, 0x1000);

  TEST_ASSERT_EQUAL_INT(0, check_watchpoints(&dbg));
}

void test_check_watchpoints_detects_change(void) {
  mem_write_u32(&prog.pt, 0x1000, 42, MEMORY_READ | MEMORY_WRITE);
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_watchpoint(&dbg, 0x1000);

  mem_write_u32(&prog.pt, 0x1000, 99, MEMORY_WRITE);

  TEST_ASSERT_EQUAL_INT(1, check_watchpoints(&dbg));
  int idx = debugger_find_watchpoint(&dbg, 0x1000);
  TEST_ASSERT_EQUAL_UINT32(99, dbg.watchpoints[idx].last_val);
}

void test_check_watchpoints_ignores_inactive(void) {
  mem_write_u32(&prog.pt, 0x1000, 42, MEMORY_READ | MEMORY_WRITE);
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_watchpoint(&dbg, 0x1000);
  debugger_toggle_watchpoint(&dbg, 0x1000, 0);

  mem_write_u32(&prog.pt, 0x1000, 99, MEMORY_WRITE);

  TEST_ASSERT_EQUAL_INT(0, check_watchpoints(&dbg));
}

/* ---------------------------------------------------------------------
 * classify_step (internal, not in header)
 * ------------------------------------------------------------------- */

void test_classify_step_null_dbg_returns_fault(void) {
  StepResult r = {0};
  TEST_ASSERT_EQUAL(STOP_FAULT, classify_step(NULL, r));
}

void test_classify_step_fault_status(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  StepResult r = {.status = STEP_HALTED_FETCH};
  TEST_ASSERT_EQUAL(STOP_FAULT, classify_step(&dbg, r));
}

void test_classify_step_exited_status(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  StepResult r = {.status = STEP_EXITED};
  TEST_ASSERT_EQUAL(STOP_EXITED, classify_step(&dbg, r));
}

void test_classify_step_ebreak_status(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  StepResult r = {.status = STEP_EBREAK};
  TEST_ASSERT_EQUAL(STOP_EBREAK, classify_step(&dbg, r));
}

void test_classify_step_ok_status_stores_last_step(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  StepResult r = {.status = STEP_OK, .pc_old = 4, .pc_new = 8};

  TEST_ASSERT_EQUAL(STOP_STEP, classify_step(&dbg, r));
  TEST_ASSERT_EQUAL_UINT32(4, dbg.last_step.pc_old);
  TEST_ASSERT_EQUAL_UINT32(8, dbg.last_step.pc_new);
}

/* ---------------------------------------------------------------------
 * debugger_step / debugger_continue
 * ------------------------------------------------------------------- */

void test_debugger_step_null_dbg_returns_fault(void) { TEST_ASSERT_EQUAL(STOP_FAULT, debugger_step(NULL)); }

void test_debugger_step_executes_single_instruction(void) {
  uint32_t addi = (5u << 20) | (0u << 15) | (0u << 12) | (1u << 7) | 0x13u;
  write_word_at(0x1000, addi);
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);

  DebugStopReason reason = debugger_step(&dbg);

  TEST_ASSERT_EQUAL(STOP_STEP, reason);
  TEST_ASSERT_EQUAL_UINT32(5, prog.registers[1]);
  TEST_ASSERT_EQUAL_UINT32(0x1004, prog.pc);
}

void test_debugger_step_triggers_watchpoint(void) {
  /* sw x2, 0(x1) where x1=0x2000, x2=99 -- write that changes watched mem */
  mem_write_u32(&prog.pt, 0x2000, 0, MEMORY_READ | MEMORY_WRITE);
  uint32_t sw = (0u << 25) | (2u << 20) | (1u << 15) | (2u << 12) | (0u << 7) | 0x23u;
  write_word_at(0x1000, sw);
  prog.registers[1] = 0x2000;
  prog.registers[2] = 99;
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_watchpoint(&dbg, 0x2000);

  DebugStopReason reason = debugger_step(&dbg);

  TEST_ASSERT_EQUAL(STOP_WATCHPOINT, reason);
}

void test_debugger_continue_null_dbg_returns_fault(void) { TEST_ASSERT_EQUAL(STOP_FAULT, debugger_continue(NULL)); }

void test_debugger_continue_stops_at_breakpoint(void) {
  uint32_t addi1 = (5u << 20) | (0u << 15) | (0u << 12) | (1u << 7) | 0x13u;
  uint32_t addi2 = (3u << 20) | (1u << 15) | (0u << 12) | (1u << 7) | 0x13u;
  write_word_at(0x1000, addi1);
  write_word_at(0x1004, addi2);
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_breakpoint(&dbg, 0x1004);

  DebugStopReason reason = debugger_continue(&dbg);

  TEST_ASSERT_EQUAL(STOP_BREAKPOINT, reason);
  TEST_ASSERT_EQUAL_UINT32(0x1004, prog.pc);      /* stopped before executing it */
  TEST_ASSERT_EQUAL_UINT32(5, prog.registers[1]); /* first instruction did run */
}

void test_debugger_continue_steps_over_breakpoint_at_current_pc(void) {
  uint32_t addi1 = (5u << 20) | (0u << 15) | (0u << 12) | (1u << 7) | 0x13u;
  write_word_at(0x1000, addi1);
  write_word_at(0x1004, 0x73); /* ecall */
  prog.registers[17] = 10;     /* exit */
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_breakpoint(&dbg, 0x1000); /* breakpoint at the starting pc */

  DebugStopReason reason = debugger_continue(&dbg);

  /* Should step over the breakpoint at the current pc rather than
   * immediately reporting STOP_BREAKPOINT without making progress. */
  TEST_ASSERT_EQUAL(STOP_EXITED, reason);
  TEST_ASSERT_EQUAL_UINT32(5, prog.registers[1]);
}

void test_debugger_continue_runs_to_exit(void) {
  write_word_at(0x1000, 0x73);
  prog.registers[17] = 10;
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);

  TEST_ASSERT_EQUAL(STOP_EXITED, debugger_continue(&dbg));
}

void test_debugger_continue_runs_to_fault(void) {
  prog.pc = 0x1000; /* unmapped */
  prog.status = CPU_RUNNING;
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);

  TEST_ASSERT_EQUAL(STOP_FAULT, debugger_continue(&dbg));
}

/* ---------------------------------------------------------------------
 * print_num / debugger_print_register / debugger_print_memory
 * ------------------------------------------------------------------- */

static DebuggerState* print_target_dbg;
static uint32_t print_target_num;
static NumberFormat print_target_fmt;

static void call_print_num(void) { print_num(print_target_num, print_target_fmt); }

void test_print_num_hex(void) {
  print_target_num = 0xABCD;
  print_target_fmt = FORMAT_HEX;
  const char* out = capture_stdout(call_print_num);
  TEST_ASSERT_EQUAL_STRING("0x0000ABCD", out);
}

void test_print_num_dec(void) {
  print_target_num = 12345;
  print_target_fmt = FORMAT_DEC;
  const char* out = capture_stdout(call_print_num);
  TEST_ASSERT_EQUAL_STRING("12345", out);
}

void test_print_num_ascii(void) {
  print_target_num = 0x64636261; /* little-endian bytes: 'a','b','c','d' */
  print_target_fmt = FORMAT_ASCII;
  const char* out = capture_stdout(call_print_num);
  TEST_ASSERT_EQUAL_STRING("abcd", out);
}

static void call_print_register(void) { debugger_print_register(print_target_dbg, 0x3, FORMAT_DEC); }

void test_debugger_print_register_masked_output(void) {
  prog.registers[0] = 111;
  prog.registers[1] = 222;
  prog.registers[2] = 333; /* not in mask 0x3 (bits 0,1) */
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  print_target_dbg = &dbg;

  const char* out = capture_stdout(call_print_register);

  TEST_ASSERT_NOT_NULL(strstr(out, "x0: 111"));
  TEST_ASSERT_NOT_NULL(strstr(out, "x1: 222"));
  TEST_ASSERT_NULL(strstr(out, "333"));
}

static void call_print_memory(void) { debugger_print_memory(print_target_dbg, 0x1000, 1, 4, FORMAT_DEC); }

void test_debugger_print_memory_null_dbg_no_crash(void) {
  debugger_print_memory(NULL, 0, 1, 4, FORMAT_DEC);
  TEST_PASS();
}

static void call_print_memory_invalid_count(void) { debugger_print_memory(print_target_dbg, 0x1000, 3, 4, FORMAT_DEC); }

void test_debugger_print_memory_invalid_count_no_output(void) {
  mem_write_u32(&prog.pt, 0x1000, 777, MEMORY_READ | MEMORY_WRITE);
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  print_target_dbg = &dbg;

  /* count=3 is not one of the accepted values {1, 2, 4}, so the function
   * should return immediately with no output at all. */
  const char* out = capture_stdout(call_print_memory_invalid_count);
  TEST_ASSERT_EQUAL_STRING("", out);
}

void test_debugger_print_memory_readable(void) {
  mem_write_u32(&prog.pt, 0x1000, 777, MEMORY_READ | MEMORY_WRITE);
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  print_target_dbg = &dbg;

  const char* out = capture_stdout(call_print_memory);

  TEST_ASSERT_NOT_NULL(strstr(out, "0x00001000"));
  TEST_ASSERT_NOT_NULL(strstr(out, "777"));
}

static void call_print_memory_unreadable(void) { debugger_print_memory(print_target_dbg, 0x9000, 1, 4, FORMAT_DEC); }

void test_debugger_print_memory_unreadable_marks_unreadable(void) {
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  print_target_dbg = &dbg;

  const char* out = capture_stdout(call_print_memory_unreadable);

  TEST_ASSERT_NOT_NULL(strstr(out, "<unreadable>"));
}

/* ---------------------------------------------------------------------
 * debugger_print_disas
 * ------------------------------------------------------------------- */

static void call_print_disas(void) { debugger_print_disas(print_target_dbg, 0x1000, 1, FORMAT_DEC); }

void test_debugger_print_disas_null_dbg_no_crash(void) {
  debugger_print_disas(NULL, 0, 1, FORMAT_DEC);
  TEST_PASS();
}

void test_debugger_print_disas_valid_instruction(void) {
  uint32_t addi = (5u << 20) | (0u << 15) | (0u << 12) | (1u << 7) | 0x13u; /* addi x1, x0, 5 */
  write_word_at(0x1000, addi);
  prog.pc = 0x1000;
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  print_target_dbg = &dbg;

  const char* out = capture_stdout(call_print_disas);

  TEST_ASSERT_NOT_NULL(strstr(out, "addi x1, x0, 5"));
  TEST_ASSERT_NOT_NULL(strstr(out, "=>")); /* pc marker, since prog.pc == 0x1000 */
}

void test_debugger_print_disas_marks_breakpoint(void) {
  uint32_t addi = (5u << 20) | (0u << 15) | (0u << 12) | (1u << 7) | 0x13u;
  write_word_at(0x1000, addi);
  prog.pc = 0x2000; /* not at this address, so no pc marker */
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  debugger_add_breakpoint(&dbg, 0x1000);
  print_target_dbg = &dbg;

  const char* out = capture_stdout(call_print_disas);

  TEST_ASSERT_NOT_NULL(strstr(out, "*0x00001000"));
}

void test_debugger_print_disas_unreadable_memory(void) {
  prog.pc = 0x9999;
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  print_target_dbg = &dbg;

  const char* out = capture_stdout(call_print_disas);

  TEST_ASSERT_NOT_NULL(strstr(out, "<unreadable>"));
}

void test_debugger_print_disas_invalid_instruction(void) {
  write_word_at(0x1000, 0x7F); /* reserved/undecodable opcode byte */
  debugger_init(&dbg, &prog, &(Elf32Image){0}, NULL);
  print_target_dbg = &dbg;

  const char* out = capture_stdout(call_print_disas);

  TEST_ASSERT_NOT_NULL(strstr(out, "<invalid instruction:"));
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_debugger_init_null_args_no_crash);
  RUN_TEST(test_debugger_init_sets_fields);
  RUN_TEST(test_debugger_init_requires_prog_source);

  RUN_TEST(test_debugger_find_breakpoint_null_dbg_should_return_not_found);
  RUN_TEST(test_debugger_find_breakpoint_not_set_returns_negative_one);
  RUN_TEST(test_debugger_add_and_find_breakpoint);
  RUN_TEST(test_debugger_add_breakpoint_duplicate_is_idempotent);
  RUN_TEST(test_debugger_add_breakpoint_table_full_fails);
  RUN_TEST(test_debugger_add_breakpoint_symbol_resolves);
  RUN_TEST(test_debugger_add_breakpoint_symbol_unresolved_fails);
  RUN_TEST(test_debugger_add_breakpoint_symbol_no_table_fails);
  RUN_TEST(test_debugger_remove_breakpoint);
  RUN_TEST(test_debugger_remove_breakpoint_not_found_no_crash);
  RUN_TEST(test_debugger_toggle_breakpoint);
  RUN_TEST(test_debugger_toggle_breakpoint_not_found_fails);

  RUN_TEST(test_debugger_find_watchpoint_null_dbg_should_return_not_found);
  RUN_TEST(test_debugger_add_watchpoint_mapped_returns_one);
  RUN_TEST(test_debugger_add_watchpoint_unmapped_returns_two);
  RUN_TEST(test_debugger_add_watchpoint_duplicate_is_idempotent);
  RUN_TEST(test_debugger_remove_watchpoint);
  RUN_TEST(test_debugger_toggle_watchpoint);

  RUN_TEST(test_check_watchpoints_no_change_returns_zero);
  RUN_TEST(test_check_watchpoints_detects_change);
  RUN_TEST(test_check_watchpoints_ignores_inactive);

  RUN_TEST(test_classify_step_null_dbg_returns_fault);
  RUN_TEST(test_classify_step_fault_status);
  RUN_TEST(test_classify_step_exited_status);
  RUN_TEST(test_classify_step_ebreak_status);
  RUN_TEST(test_classify_step_ok_status_stores_last_step);

  RUN_TEST(test_debugger_step_null_dbg_returns_fault);
  RUN_TEST(test_debugger_step_executes_single_instruction);
  RUN_TEST(test_debugger_step_triggers_watchpoint);

  RUN_TEST(test_debugger_continue_null_dbg_returns_fault);
  RUN_TEST(test_debugger_continue_stops_at_breakpoint);
  RUN_TEST(test_debugger_continue_steps_over_breakpoint_at_current_pc);
  RUN_TEST(test_debugger_continue_runs_to_exit);
  RUN_TEST(test_debugger_continue_runs_to_fault);

  RUN_TEST(test_print_num_hex);
  RUN_TEST(test_print_num_dec);
  RUN_TEST(test_print_num_ascii);
  RUN_TEST(test_debugger_print_register_masked_output);
  RUN_TEST(test_debugger_print_memory_null_dbg_no_crash);
  RUN_TEST(test_debugger_print_memory_invalid_count_no_output);
  RUN_TEST(test_debugger_print_memory_readable);
  RUN_TEST(test_debugger_print_memory_unreadable_marks_unreadable);

  RUN_TEST(test_debugger_print_disas_null_dbg_no_crash);
  RUN_TEST(test_debugger_print_disas_valid_instruction);
  RUN_TEST(test_debugger_print_disas_marks_breakpoint);
  RUN_TEST(test_debugger_print_disas_unreadable_memory);
  RUN_TEST(test_debugger_print_disas_invalid_instruction);

  return UNITY_END();
}