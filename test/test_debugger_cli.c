#define _POSIX_C_SOURCE 200809L

#include "../src/debugger_cli.c"
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

static char capture_buffer[4096];

static const char* capture_stdout(void (*fn)(void)) {
  char path[] = "/tmp/debugger_cli_test_XXXXXX";
  int fd = mkstemp(path);
  int saved_stdout = dup(STDOUT_FILENO);

  fflush(stdout);
  dup2(fd, STDOUT_FILENO);

  fn();

  fflush(stdout);
  dup2(saved_stdout, STDOUT_FILENO);
  close(saved_stdout);

  memset(capture_buffer, 0, sizeof(capture_buffer));
  lseek(fd, 0, SEEK_SET);
  ssize_t bytes_read = read(fd, capture_buffer, sizeof(capture_buffer) - 1);
  (void)bytes_read;
  close(fd);
  unlink(path);

  return capture_buffer;
}

static DebuggerState* cli_target_dbg;
static const char* cli_target_script;

static void run_debugger_cli_call(void) {
  char stdin_path[] = "/tmp/debugger_cli_stdin_XXXXXX";
  int stdin_fd = mkstemp(stdin_path);
  write(stdin_fd, cli_target_script, strlen(cli_target_script));
  lseek(stdin_fd, 0, SEEK_SET);

  int saved_stdin = dup(STDIN_FILENO);
  dup2(stdin_fd, STDIN_FILENO);
  clearerr(stdin);
  fflush(stdin);

  debugger_cli(cli_target_dbg);

  dup2(saved_stdin, STDIN_FILENO);
  close(saved_stdin);
  close(stdin_fd);
  unlink(stdin_path);
}

static const char* run_cli_with_input(DebuggerState* d, const char* script) {
  cli_target_dbg = d;
  cli_target_script = script;
  return capture_stdout(run_debugger_cli_call);
}

/* ---------------------------------------------------------------------
 * tokenize_command
 * ------------------------------------------------------------------- */

void test_tokenize_command_null_args_return_zero(void) {
  char* tokens[MAX_TOKENS];
  TEST_ASSERT_EQUAL_INT(0, tokenize_command(NULL, tokens, MAX_TOKENS));
  char line[] = "step";
  TEST_ASSERT_EQUAL_INT(0, tokenize_command(line, NULL, MAX_TOKENS));
}

void test_tokenize_command_basic_split(void) {
  char line[] = "break 0x1000";
  char* tokens[MAX_TOKENS];

  int count = tokenize_command(line, tokens, MAX_TOKENS);

  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_EQUAL_STRING("break", tokens[0]);
  TEST_ASSERT_EQUAL_STRING("0x1000", tokens[1]);
}

void test_tokenize_command_collapses_whitespace(void) {
  char line[] = "  break    0x1000  \t extra";
  char* tokens[MAX_TOKENS];

  int count = tokenize_command(line, tokens, MAX_TOKENS);

  TEST_ASSERT_EQUAL_INT(3, count);
  TEST_ASSERT_EQUAL_STRING("break", tokens[0]);
  TEST_ASSERT_EQUAL_STRING("0x1000", tokens[1]);
  TEST_ASSERT_EQUAL_STRING("extra", tokens[2]);
}

void test_tokenize_command_empty_line(void) {
  char line[] = "";
  char* tokens[MAX_TOKENS];
  TEST_ASSERT_EQUAL_INT(0, tokenize_command(line, tokens, MAX_TOKENS));
}

void test_tokenize_command_whitespace_only(void) {
  char line[] = "   \t  ";
  char* tokens[MAX_TOKENS];
  TEST_ASSERT_EQUAL_INT(0, tokenize_command(line, tokens, MAX_TOKENS));
}

void test_tokenize_command_respects_max_tokens(void) {
  char line[] = "a b c d e";
  char* tokens[3];
  int count = tokenize_command(line, tokens, 3);
  TEST_ASSERT_EQUAL_INT(3, count);
}

/* ---------------------------------------------------------------------
 * dispatch
 * ------------------------------------------------------------------- */

void test_dispatch_null_args_return_zero(void) {
  char line[] = "step";
  TEST_ASSERT_EQUAL_INT(0, dispatch(NULL, line));
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  TEST_ASSERT_EQUAL_INT(0, dispatch(&dbg, NULL));
}

void test_dispatch_empty_line_returns_zero(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char line[] = "   ";
  TEST_ASSERT_EQUAL_INT(0, dispatch(&dbg, line));
}

void test_dispatch_unknown_command(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char line[] = "frobnicate";

  int result = dispatch(&dbg, line);
  TEST_ASSERT_EQUAL_INT(0, result);
}

void test_dispatch_recognizes_aliases(void) {
  /* "s" is an alias for "step"; both should resolve to the same handler
   * and not report "unknown command". */
  write_word_at(0x1000, 0x13); /* addi x0,x0,0 -- valid nop */
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);

  char line[] = "s";
  int result = dispatch(&dbg, line);

  TEST_ASSERT_EQUAL_INT(CMD_OK, result);
}

/* ---------------------------------------------------------------------
 * parse_address_or_symbol
 * ------------------------------------------------------------------- */

void test_parse_address_or_symbol_null_args_fail(void) {
  uint32_t out;
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  TEST_ASSERT_EQUAL_INT(0, parse_address_or_symbol(NULL, "0x1000", &out));
  TEST_ASSERT_EQUAL_INT(0, parse_address_or_symbol(&dbg, NULL, &out));
  TEST_ASSERT_EQUAL_INT(0, parse_address_or_symbol(&dbg, "0x1000", NULL));
}

void test_parse_address_or_symbol_hex(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  uint32_t out;
  TEST_ASSERT_EQUAL_INT(1, parse_address_or_symbol(&dbg, "0x1000", &out));
  TEST_ASSERT_EQUAL_UINT32(0x1000, out);
}

void test_parse_address_or_symbol_decimal(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  uint32_t out;
  TEST_ASSERT_EQUAL_INT(1, parse_address_or_symbol(&dbg, "100", &out));
  TEST_ASSERT_EQUAL_UINT32(100, out);
}

void test_parse_address_or_symbol_resolves_symbol(void) {
  Symbol sym = {0};
  sym.name = "loop";
  sym.len = 4;
  sym.value = 0x2000;
  SymbolTable table = {0};
  table.head = &sym;
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, &table);

  uint32_t out;
  TEST_ASSERT_EQUAL_INT(1, parse_address_or_symbol(&dbg, "loop", &out));
  TEST_ASSERT_EQUAL_UINT32(0x2000, out);
}

void test_parse_address_or_symbol_unresolvable_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  uint32_t out;
  TEST_ASSERT_EQUAL_INT(0, parse_address_or_symbol(&dbg, "not_a_symbol", &out));
}

/* ---------------------------------------------------------------------
 * cmd_break
 * ------------------------------------------------------------------- */

void test_cmd_break_no_args_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_break(&dbg, 0, NULL));
}

void test_cmd_break_valid_address(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char addr[] = "0x1000";
  char* argv[] = {addr};

  TEST_ASSERT_EQUAL_INT(CMD_OK, cmd_break(&dbg, 1, argv));
  TEST_ASSERT_TRUE(debugger_find_breakpoint(&dbg, 0x1000) >= 0);
}

void test_cmd_break_invalid_symbol_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char addr[] = "not_a_symbol";
  char* argv[] = {addr};

  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_break(&dbg, 1, argv));
}

void test_cmd_break_table_full_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  for (int i = 0; i < MAX_BREAKPOINTS; i++)
    debugger_add_breakpoint(&dbg, 0x1000 + i * 4);

  char addr[] = "0xFFFF";
  char* argv[] = {addr};
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_break(&dbg, 1, argv));
}

/* ---------------------------------------------------------------------
 * cmd_delete
 * ------------------------------------------------------------------- */

void test_cmd_delete_no_args_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_delete(&dbg, 0, NULL));
}

void test_cmd_delete_nonexistent_should_report_not_found(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char addr[] = "0x1000";
  char* argv[] = {addr};

  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_delete(&dbg, 1, argv));
}

void test_cmd_delete_breakpoint_at_index_zero_should_be_removed(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  debugger_add_breakpoint(&dbg, 0x1000); /* lands at index 0 in an empty table */

  char addr[] = "0x1000";
  char* argv[] = {addr};
  cmd_delete(&dbg, 1, argv);

  TEST_ASSERT_EQUAL_INT(0, dbg.breakpoints[0].set);
}

void test_cmd_delete_breakpoint_at_nonzero_index_works(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  debugger_add_breakpoint(&dbg, 0x1000); /* index 0 */
  debugger_add_breakpoint(&dbg, 0x2000); /* index 1 */

  char addr[] = "0x2000";
  char* argv[] = {addr};
  int result = cmd_delete(&dbg, 1, argv);

  TEST_ASSERT_EQUAL_INT(CMD_OK, result);
  TEST_ASSERT_EQUAL_INT(0, dbg.breakpoints[1].set);
}

/* ---------------------------------------------------------------------
 * cmd_watch
 * ------------------------------------------------------------------- */

void test_cmd_watch_no_args_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_watch(&dbg, 0, NULL));
}

void test_cmd_watch_mapped_address_succeeds(void) {
  mem_write_u32(&prog.pt, 0x1000, 5, MEMORY_READ | MEMORY_WRITE);
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char addr[] = "0x1000";
  char* argv[] = {addr};

  TEST_ASSERT_EQUAL_INT(CMD_OK, cmd_watch(&dbg, 1, argv));
  TEST_ASSERT_TRUE(debugger_find_watchpoint(&dbg, 0x1000) >= 0);
}

void test_cmd_watch_table_full_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  for (int i = 0; i < MAX_WATCHPOINTS; i++)
    debugger_add_watchpoint(&dbg, 0x1000 + i * 4);

  char addr[] = "0xFFFF";
  char* argv[] = {addr};
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_watch(&dbg, 1, argv));
}

/* ---------------------------------------------------------------------
 * cmd_info
 * ------------------------------------------------------------------- */

void test_cmd_info_no_args_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_info(&dbg, 0, NULL));
}

void test_cmd_info_unknown_target_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char target[] = "bogus";
  char* argv[] = {target};
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_info(&dbg, 1, argv));
}

static DebuggerState* info_target_dbg;
static char* info_target_argv[1];

static void call_cmd_info(void) { cmd_info(info_target_dbg, 1, info_target_argv); }

void test_cmd_info_breakpoints_lists_set_entries(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  debugger_add_breakpoint(&dbg, 0x1000);
  info_target_dbg = &dbg;
  static char target[] = "breakpoints";
  info_target_argv[0] = target;

  const char* out = capture_stdout(call_cmd_info);

  TEST_ASSERT_NOT_NULL(strstr(out, "0x00001000"));
}

void test_cmd_info_registers_prints_all(void) {
  prog.registers[5] = 42;
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  info_target_dbg = &dbg;
  static char target[] = "registers";
  info_target_argv[0] = target;

  const char* out = capture_stdout(call_cmd_info);

  TEST_ASSERT_NOT_NULL(strstr(out, "x5:"));
}

/* ---------------------------------------------------------------------
 * cmd_examine / cmd_disas
 * ------------------------------------------------------------------- */

void test_cmd_examine_too_few_args_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char addr[] = "0x1000";
  char* argv[] = {addr};
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_examine(&dbg, 1, argv));
}

void test_cmd_examine_invalid_symbol_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char addr[] = "not_a_symbol";
  char count[] = "1";
  char* argv[] = {addr, count};
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_examine(&dbg, 2, argv));
}

static DebuggerState* examine_target_dbg;
static char* examine_target_argv[2];

static void call_cmd_examine(void) { cmd_examine(examine_target_dbg, 2, examine_target_argv); }

void test_cmd_examine_reads_memory(void) {
  mem_write_u32(&prog.pt, 0x1000, 0xCAFEBABE, MEMORY_READ | MEMORY_WRITE);
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  examine_target_dbg = &dbg;
  static char addr[] = "0x1000";
  static char count[] = "1";
  examine_target_argv[0] = addr;
  examine_target_argv[1] = count;

  const char* out = capture_stdout(call_cmd_examine);

  TEST_ASSERT_NOT_NULL(strstr(out, "0x00001000"));
}

static DebuggerState* disas_target_dbg;
static char* disas_target_argv[2];
static int disas_target_argc;

static void call_cmd_disas(void) { cmd_disas(disas_target_dbg, disas_target_argc, disas_target_argv); }

void test_cmd_disas_defaults_to_pc_and_count_10(void) {
  write_word_at(0x1000, 0x13); /* nop */
  prog.pc = 0x1000;
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  disas_target_dbg = &dbg;
  disas_target_argc = 0;

  const char* out = capture_stdout(call_cmd_disas);

  TEST_ASSERT_NOT_NULL(strstr(out, "0x00001000"));
}

void test_cmd_disas_explicit_address_and_count(void) {
  write_word_at(0x2000, 0x13);
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  disas_target_dbg = &dbg;
  static char addr[] = "0x2000";
  static char count[] = "1";
  disas_target_argv[0] = addr;
  disas_target_argv[1] = count;
  disas_target_argc = 2;

  const char* out = capture_stdout(call_cmd_disas);

  TEST_ASSERT_NOT_NULL(strstr(out, "0x00002000"));
}

void test_cmd_disas_invalid_symbol_fails(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  char addr[] = "not_a_symbol";
  char* argv[] = {addr};
  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_disas(&dbg, 1, argv));
}

/* ---------------------------------------------------------------------
 * cmd_step / cmd_continue / cmd_run / cmd_quit / cmd_help
 * ------------------------------------------------------------------- */

void test_cmd_step_executes_and_returns_ok(void) {
  write_word_at(0x1000, 0x13); /* nop */
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);

  TEST_ASSERT_EQUAL_INT(CMD_OK, cmd_step(&dbg, 0, NULL));
}

void test_cmd_step_fault_returns_error(void) {
  prog.pc = 0x1000; /* unmapped */
  prog.status = CPU_RUNNING;
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);

  TEST_ASSERT_EQUAL_INT(CMD_ERROR, cmd_step(&dbg, 0, NULL));
}

void test_cmd_continue_runs_to_exit(void) {
  write_word_at(0x1000, 0x73); /* ecall */
  prog.registers[17] = 10;     /* exit */
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);

  TEST_ASSERT_EQUAL_INT(CMD_OK, cmd_continue(&dbg, 0, NULL));
  TEST_ASSERT_EQUAL(CPU_EXITED, prog.status);
}

void test_cmd_run_resets_and_reports_ok_even_on_fault(void) {
  AssembledProgram source = {0};
  ProgramState* initial = calloc(1, sizeof(ProgramState));
  debugger_init(&dbg, initial, &source, NULL);

  int result = cmd_run(&dbg, 0, NULL);

  TEST_ASSERT_EQUAL_INT(CMD_OK, result);
  TEST_ASSERT_EQUAL(CPU_HALTED, dbg.prog->status);

  free_loaded_binary(dbg.prog);
}

void test_cmd_quit_returns_cmd_quit(void) { TEST_ASSERT_EQUAL_INT(CMD_QUIT, cmd_quit(&dbg, 0, NULL)); }

void test_cmd_quit_null_dbg_no_crash(void) { TEST_ASSERT_EQUAL_INT(CMD_QUIT, cmd_quit(NULL, 0, NULL)); }

static void call_cmd_help(void) { cmd_help(NULL, 0, NULL); }

void test_cmd_help_lists_all_commands(void) {
  const char* out = capture_stdout(call_cmd_help);

  TEST_ASSERT_NOT_NULL(strstr(out, "step"));
  TEST_ASSERT_NOT_NULL(strstr(out, "break"));
  TEST_ASSERT_NOT_NULL(strstr(out, "quit"));
}

/* ---------------------------------------------------------------------
 * debugger_cli (integration)
 * ------------------------------------------------------------------- */

void test_debugger_cli_quit_exits_cleanly(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  const char* out = run_cli_with_input(&dbg, "quit\n");
  TEST_ASSERT_NOT_NULL(strstr(out, "(dbg) "));
}

void test_debugger_cli_eof_breaks_loop(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  const char* out = run_cli_with_input(&dbg, "");
  TEST_ASSERT_NOT_NULL(strstr(out, "(dbg) "));
}

void test_debugger_cli_blank_line_repeats_last_command(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  const char* out = run_cli_with_input(&dbg, "help\n\nquit\n");

  /* "help" prints "Execute one instruction" (from the step command's
   * help text) once per invocation; a blank line should re-run "help",
   * so it should appear twice total. */
  int count = 0;
  const char* p = out;
  while ((p = strstr(p, "Execute one instruction")) != NULL) {
    count++;
    p += 1;
  }
  TEST_ASSERT_EQUAL_INT(2, count);
}

void test_debugger_cli_unknown_command_reports_error(void) {
  debugger_init(&dbg, &prog, &(AssembledProgram){0}, NULL);
  const char* out = run_cli_with_input(&dbg, "bogus\nquit\n");
  TEST_ASSERT_NOT_NULL(strstr(out, "Unknown command"));
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_tokenize_command_null_args_return_zero);
  RUN_TEST(test_tokenize_command_basic_split);
  RUN_TEST(test_tokenize_command_collapses_whitespace);
  RUN_TEST(test_tokenize_command_empty_line);
  RUN_TEST(test_tokenize_command_whitespace_only);
  RUN_TEST(test_tokenize_command_respects_max_tokens);

  RUN_TEST(test_dispatch_null_args_return_zero);
  RUN_TEST(test_dispatch_empty_line_returns_zero);
  RUN_TEST(test_dispatch_unknown_command);
  RUN_TEST(test_dispatch_recognizes_aliases);

  RUN_TEST(test_parse_address_or_symbol_null_args_fail);
  RUN_TEST(test_parse_address_or_symbol_hex);
  RUN_TEST(test_parse_address_or_symbol_decimal);
  RUN_TEST(test_parse_address_or_symbol_resolves_symbol);
  RUN_TEST(test_parse_address_or_symbol_unresolvable_fails);

  RUN_TEST(test_cmd_break_no_args_fails);
  RUN_TEST(test_cmd_break_valid_address);
  RUN_TEST(test_cmd_break_invalid_symbol_fails);
  RUN_TEST(test_cmd_break_table_full_fails);

  RUN_TEST(test_cmd_delete_no_args_fails);
  RUN_TEST(test_cmd_delete_nonexistent_should_report_not_found);
  RUN_TEST(test_cmd_delete_breakpoint_at_index_zero_should_be_removed);
  RUN_TEST(test_cmd_delete_breakpoint_at_nonzero_index_works);

  RUN_TEST(test_cmd_watch_no_args_fails);
  RUN_TEST(test_cmd_watch_mapped_address_succeeds);
  RUN_TEST(test_cmd_watch_table_full_fails);

  RUN_TEST(test_cmd_info_no_args_fails);
  RUN_TEST(test_cmd_info_unknown_target_fails);
  RUN_TEST(test_cmd_info_breakpoints_lists_set_entries);
  RUN_TEST(test_cmd_info_registers_prints_all);

  RUN_TEST(test_cmd_examine_too_few_args_fails);
  RUN_TEST(test_cmd_examine_invalid_symbol_fails);
  RUN_TEST(test_cmd_examine_reads_memory);

  RUN_TEST(test_cmd_disas_defaults_to_pc_and_count_10);
  RUN_TEST(test_cmd_disas_explicit_address_and_count);
  RUN_TEST(test_cmd_disas_invalid_symbol_fails);

  RUN_TEST(test_cmd_step_executes_and_returns_ok);
  RUN_TEST(test_cmd_step_fault_returns_error);
  RUN_TEST(test_cmd_continue_runs_to_exit);
  RUN_TEST(test_cmd_run_resets_and_reports_ok_even_on_fault);
  RUN_TEST(test_cmd_quit_returns_cmd_quit);
  RUN_TEST(test_cmd_quit_null_dbg_no_crash);
  RUN_TEST(test_cmd_help_lists_all_commands);

  RUN_TEST(test_debugger_cli_quit_exits_cleanly);
  RUN_TEST(test_debugger_cli_eof_breaks_loop);
  RUN_TEST(test_debugger_cli_blank_line_repeats_last_command);
  RUN_TEST(test_debugger_cli_unknown_command_reports_error);

  return UNITY_END();
}