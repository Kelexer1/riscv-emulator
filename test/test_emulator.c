#define _POSIX_C_SOURCE 200809L

#include "../src/emulator.c"
#include "unity/unity.h"
#include <stdlib.h>
#include <string.h>

static ProgramState prog;

void setUp(void) {
  memset(&prog, 0, sizeof(ProgramState));
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

void test_execute_program_ebreak_halts_and_returns_zero(void) {
  write_word_at(0x1000, 0x00100073); /* ebreak */
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  TEST_ASSERT_EQUAL_INT(0, execute_program(&prog));
  TEST_ASSERT_EQUAL(CPU_HALTED, prog.status);
}

/* ---------------------------------------------------------------------
 * execute_program
 * ------------------------------------------------------------------- */

void test_execute_program_null_prog_returns_zero(void) { TEST_ASSERT_EQUAL_INT(0, execute_program(NULL)); }

void test_execute_program_immediate_exit_returns_one(void) {
  /* ecall (a7=10, exit) as the very first instruction */
  write_word_at(0x1000, 0x73);
  prog.registers[17] = 10;
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  TEST_ASSERT_EQUAL_INT(1, execute_program(&prog));
  TEST_ASSERT_EQUAL(CPU_EXITED, prog.status);
}

void test_execute_program_exit_with_code_returns_one(void) {
  write_word_at(0x1000, 0x73);
  prog.registers[17] = 17; /* exit with code */
  prog.registers[10] = 5;
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  TEST_ASSERT_EQUAL_INT(1, execute_program(&prog));
  TEST_ASSERT_EQUAL_INT(5, prog.exit_code);
}

void test_execute_program_runs_multiple_instructions_then_exits(void) {
  /* addi x1, x0, 5 ; addi x1, x1, 3 ; ecall (exit) */
  uint32_t addi1 = (5u << 20) | (0u << 15) | (0u << 12) | (1u << 7) | 0x13u;
  uint32_t addi2 = (3u << 20) | (1u << 15) | (0u << 12) | (1u << 7) | 0x13u;
  write_word_at(0x1000, addi1);
  write_word_at(0x1004, addi2);
  write_word_at(0x1008, 0x73);
  prog.registers[17] = 10;
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  TEST_ASSERT_EQUAL_INT(1, execute_program(&prog));
  TEST_ASSERT_EQUAL_UINT32(8, prog.registers[1]);
}

void test_execute_program_fetch_fault_returns_zero(void) {
  prog.pc = 0x1000; /* unmapped, never written */
  prog.status = CPU_RUNNING;

  TEST_ASSERT_EQUAL_INT(0, execute_program(&prog));
  TEST_ASSERT_EQUAL(CPU_HALTED, prog.status);
}

void test_execute_program_decode_fault_returns_zero(void) {
  write_word_at(0x1000, 0x7F); /* reserved opcode byte, undecodable */
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  TEST_ASSERT_EQUAL_INT(0, execute_program(&prog));
}

void test_execute_program_invalid_op_returns_zero(void) {
  /* funct7=0x10 doesn't match ADD's 0x00 or SUB's 0x20 for opcode 0x33 --
   * an undecodable R-type encoding. */
  uint32_t bin = (0x10u << 25) | (0u << 20) | (0u << 15) | (0u << 12) | (0u << 7) | 0x33u;
  write_word_at(0x1000, bin);
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  TEST_ASSERT_EQUAL_INT(0, execute_program(&prog));
}

void test_execute_program_mem_fault_returns_zero(void) {
  /* lw x1, 0(x2) where x2 points to unmapped memory */
  uint32_t lw = (0u << 20) | (2u << 15) | (2u << 12) | (1u << 7) | 0x03u;
  write_word_at(0x1000, lw);
  prog.registers[2] = 0x9000; /* unmapped */
  prog.pc = 0x1000;
  prog.status = CPU_RUNNING;

  TEST_ASSERT_EQUAL_INT(0, execute_program(&prog));
  TEST_ASSERT_EQUAL(CPU_HALTED, prog.status);
}

void test_execute_program_already_exited_returns_one_without_stepping(void) {
  /* Loop condition is `status == CPU_RUNNING`; if the program is already
   * marked exited, execute_program should report success without
   * attempting to fetch/decode anything (pc is left at 0, unmapped). */
  prog.status = CPU_EXITED;
  prog.pc = 0;

  TEST_ASSERT_EQUAL_INT(1, execute_program(&prog));
}

void test_execute_program_already_halted_returns_zero_without_stepping(void) {
  prog.status = CPU_HALTED;
  prog.pc = 0;

  TEST_ASSERT_EQUAL_INT(0, execute_program(&prog));
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_execute_program_null_prog_returns_zero);
  RUN_TEST(test_execute_program_immediate_exit_returns_one);
  RUN_TEST(test_execute_program_exit_with_code_returns_one);
  RUN_TEST(test_execute_program_runs_multiple_instructions_then_exits);
  RUN_TEST(test_execute_program_fetch_fault_returns_zero);
  RUN_TEST(test_execute_program_decode_fault_returns_zero);
  RUN_TEST(test_execute_program_invalid_op_returns_zero);
  RUN_TEST(test_execute_program_mem_fault_returns_zero);
  RUN_TEST(test_execute_program_ebreak_halts_and_returns_zero);
  RUN_TEST(test_execute_program_already_exited_returns_one_without_stepping);
  RUN_TEST(test_execute_program_already_halted_returns_zero_without_stepping);

  return UNITY_END();
}