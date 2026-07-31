#define _POSIX_C_SOURCE 200809L

#include "../src/api_emulator.c"
#include "unity/unity.h"
#include <string.h>

MemoryPage* page_alloc(PageTable* pt, uint32_t vaddr, PermissionMask perms);

static ProgramState prog;

void setUp(void) { memset(&prog, 0, sizeof(ProgramState)); }

void tearDown(void) {
  for (int i = 0; i < PT_SIZE; i++) {
    if (prog.pt.l2[i]) {
      for (int j = 0; j < PT_SIZE; j++)
        free(prog.pt.l2[i]->pages[j]);
      free(prog.pt.l2[i]);
    }
  }
}

static DecodedInstruction make_r(Opcode op, uint8_t rd, uint8_t rs1, uint8_t rs2) {
  DecodedInstruction ins = {0};
  ins.op = op;
  ins.rd = rd;
  ins.rs1 = rs1;
  ins.rs2 = rs2;
  return ins;
}

static DecodedInstruction make_i(Opcode op, uint8_t rd, uint8_t rs1, int32_t imm) {
  DecodedInstruction ins = {0};
  ins.op = op;
  ins.rd = rd;
  ins.rs1 = rs1;
  ins.imm = (uint32_t)imm;
  return ins;
}

/* ---------------------------------------------------------------------
 * handle_instruction -- arithmetic / logic
 * ------------------------------------------------------------------- */

void test_handle_instruction_null_args_fail(void) {
  DecodedInstruction ins = {0};
  TEST_ASSERT_EQUAL(EXEC_INVALID_OP, handle_instruction(NULL, &ins));
  TEST_ASSERT_EQUAL(EXEC_INVALID_OP, handle_instruction(&prog, NULL));
}

void test_handle_instruction_add(void) {
  prog.registers[1] = 5;
  prog.registers[2] = 7;
  DecodedInstruction ins = make_r(OP_ADD, 3, 1, 2);

  TEST_ASSERT_EQUAL(EXEC_OK, handle_instruction(&prog, &ins));
  TEST_ASSERT_EQUAL_UINT32(12, prog.registers[3]);
}

void test_handle_instruction_never_writes_x0(void) {
  prog.registers[1] = 5;
  prog.registers[2] = 7;
  DecodedInstruction ins = make_r(OP_ADD, 0, 1, 2); /* rd=x0 */

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32(0, prog.registers[0]);
}

void test_handle_instruction_sub(void) {
  prog.registers[1] = 10;
  prog.registers[2] = 3;
  DecodedInstruction ins = make_r(OP_SUB, 3, 1, 2);

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32(7, prog.registers[3]);
}

void test_handle_instruction_slt_signed(void) {
  prog.registers[1] = (uint32_t)-5;
  prog.registers[2] = 3;
  DecodedInstruction ins = make_r(OP_SLT, 3, 1, 2);

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32(1, prog.registers[3]);
}

void test_handle_instruction_sltu_unsigned(void) {
  prog.registers[1] = (uint32_t)-5; /* huge unsigned value */
  prog.registers[2] = 3;
  DecodedInstruction ins = make_r(OP_SLTU, 3, 1, 2);

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32(0, prog.registers[3]);
}

void test_handle_instruction_sra_arithmetic_shift(void) {
  prog.registers[1] = (uint32_t)-16; /* 0xFFFFFFF0 */
  prog.registers[2] = 2;
  DecodedInstruction ins = make_r(OP_SRA, 3, 1, 2);

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)-4, prog.registers[3]);
}

void test_handle_instruction_srl_logical_shift(void) {
  prog.registers[1] = 0xFFFFFFF0u;
  prog.registers[2] = 4;
  DecodedInstruction ins = make_r(OP_SRL, 3, 1, 2);

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32(0x0FFFFFFFu, prog.registers[3]);
}

void test_handle_instruction_div_by_zero(void) {
  prog.registers[1] = 10;
  prog.registers[2] = 0;
  DecodedInstruction ins = make_r(OP_DIV, 3, 1, 2);

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, prog.registers[3]);
}

void test_handle_instruction_div_overflow_int_min_by_neg1(void) {
  prog.registers[1] = (uint32_t)INT32_MIN;
  prog.registers[2] = (uint32_t)-1;
  DecodedInstruction ins = make_r(OP_DIV, 3, 1, 2);

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)INT32_MIN, prog.registers[3]);
}

void test_handle_instruction_rem_by_zero_returns_dividend(void) {
  prog.registers[1] = 42;
  prog.registers[2] = 0;
  DecodedInstruction ins = make_r(OP_REM, 3, 1, 2);

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32(42, prog.registers[3]);
}

/* ---------------------------------------------------------------------
 * handle_instruction -- immediates / loads / stores / branches / jumps
 * ------------------------------------------------------------------- */

void test_handle_instruction_addi(void) {
  prog.registers[1] = 10;
  DecodedInstruction ins = make_i(OP_ADDI, 2, 1, -3);

  handle_instruction(&prog, &ins);
  TEST_ASSERT_EQUAL_UINT32(7, prog.registers[2]);
}

void test_handle_instruction_lw_and_sw_roundtrip(void) {
  mem_write_u8(&prog.pt, 0x1000, 0, MEMORY_READ | MEMORY_WRITE); /* pre-allocate with both perms */
  prog.registers[1] = 0x1000;
  prog.registers[2] = 0xDEADBEEF;

  DecodedInstruction sw = make_i(OP_SW, 0, 1, 0);
  sw.rs2 = 2;
  TEST_ASSERT_EQUAL(EXEC_OK, handle_instruction(&prog, &sw));

  DecodedInstruction lw = make_i(OP_LW, 3, 1, 0);
  TEST_ASSERT_EQUAL(EXEC_OK, handle_instruction(&prog, &lw));
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, prog.registers[3]);
}

void test_handle_instruction_lb_sign_extends(void) {
  mem_write_u8(&prog.pt, 0x1000, 0xFF, MEMORY_READ | MEMORY_WRITE); /* -1 as a signed byte */
  prog.registers[1] = 0x1000;

  DecodedInstruction lb = make_i(OP_LB, 2, 1, 0);
  handle_instruction(&prog, &lb);
  TEST_ASSERT_EQUAL_INT32(-1, (int32_t)prog.registers[2]);
}

void test_handle_instruction_lbu_zero_extends(void) {
  mem_write_u8(&prog.pt, 0x1000, 0xFF, MEMORY_READ | MEMORY_WRITE);
  prog.registers[1] = 0x1000;

  DecodedInstruction lbu = make_i(OP_LBU, 2, 1, 0);
  handle_instruction(&prog, &lbu);
  TEST_ASSERT_EQUAL_UINT32(0xFF, prog.registers[2]);
}

void test_handle_instruction_load_fault_on_unmapped(void) {
  prog.registers[1] = 0x1000; /* never mapped */
  DecodedInstruction lw = make_i(OP_LW, 2, 1, 0);

  TEST_ASSERT_EQUAL(EXEC_MEM_READ_FAULT, handle_instruction(&prog, &lw));
}

void test_handle_instruction_store_fault_on_readonly(void) {
  page_alloc(&prog.pt, 0x1000, MEMORY_READ); /* no WRITE */
  prog.registers[1] = 0x1000;
  prog.registers[2] = 5;
  DecodedInstruction sw = make_i(OP_SW, 0, 1, 0);
  sw.rs2 = 2;

  TEST_ASSERT_EQUAL(EXEC_MEM_WRITE_FAULT, handle_instruction(&prog, &sw));
}

void test_handle_instruction_beq_taken(void) {
  prog.registers[1] = 5;
  prog.registers[2] = 5;
  prog.pc = 100;
  DecodedInstruction beq = make_r(OP_BEQ, 0, 1, 2);
  beq.imm = 16;

  TEST_ASSERT_EQUAL(EXEC_BRANCH_TAKEN, handle_instruction(&prog, &beq));
  TEST_ASSERT_EQUAL_UINT32(116, prog.pc);
}

void test_handle_instruction_beq_not_taken(void) {
  prog.registers[1] = 5;
  prog.registers[2] = 6;
  prog.pc = 100;
  DecodedInstruction beq = make_r(OP_BEQ, 0, 1, 2);
  beq.imm = 16;

  TEST_ASSERT_EQUAL(EXEC_OK, handle_instruction(&prog, &beq));
  TEST_ASSERT_EQUAL_UINT32(100, prog.pc); /* unchanged; cpu_step_single advances it */
}

void test_handle_instruction_jal_sets_link_and_pc(void) {
  prog.pc = 200;
  DecodedInstruction jal = {0};
  jal.op = OP_JAL;
  jal.rd = 1;
  jal.imm = 40;

  TEST_ASSERT_EQUAL(EXEC_BRANCH_TAKEN, handle_instruction(&prog, &jal));
  TEST_ASSERT_EQUAL_UINT32(204, prog.registers[1]);
  TEST_ASSERT_EQUAL_UINT32(240, prog.pc);
}

void test_handle_instruction_jalr_clears_low_bit(void) {
  prog.pc = 200;
  prog.registers[5] = 101; /* odd address */
  DecodedInstruction jalr = make_i(OP_JALR, 1, 5, 0);

  handle_instruction(&prog, &jalr);
  TEST_ASSERT_EQUAL_UINT32(100, prog.pc); /* low bit cleared */
}

void test_handle_instruction_ecall_and_ebreak(void) {
  DecodedInstruction ecall = {.op = OP_ECALL};
  DecodedInstruction ebreak = {.op = OP_EBREAK};

  TEST_ASSERT_EQUAL(EXEC_ECALL, handle_instruction(&prog, &ecall));
  TEST_ASSERT_EQUAL(EXEC_EBREAK, handle_instruction(&prog, &ebreak));
}

void test_handle_instruction_invalid_op(void) {
  DecodedInstruction ins = {.op = OP_UNKNOWN};
  TEST_ASSERT_EQUAL(EXEC_INVALID_OP, handle_instruction(&prog, &ins));
}

/* ---------------------------------------------------------------------
 * handle_ecall
 * ------------------------------------------------------------------- */

void test_handle_ecall_null_prog_fails(void) { TEST_ASSERT_EQUAL_INT(0, handle_ecall(NULL)); }

void test_handle_ecall_print_int_succeeds(void) {
  prog.registers[17] = 1;
  prog.registers[10] = 42;
  TEST_ASSERT_EQUAL_INT(1, handle_ecall(&prog));
}

void test_handle_ecall_exit_sets_status(void) {
  prog.registers[17] = 10;
  TEST_ASSERT_EQUAL_INT(1, handle_ecall(&prog));
  TEST_ASSERT_EQUAL(CPU_EXITED, prog.status);
}

void test_handle_ecall_exit_with_code_sets_status_and_code(void) {
  prog.registers[17] = 17;
  prog.registers[10] = 7;
  TEST_ASSERT_EQUAL_INT(1, handle_ecall(&prog));
  TEST_ASSERT_EQUAL(CPU_EXITED, prog.status);
  TEST_ASSERT_EQUAL_INT(7, prog.exit_code);
}

void test_handle_ecall_unknown_syscall_fails(void) {
  prog.registers[17] = 9999;
  TEST_ASSERT_EQUAL_INT(0, handle_ecall(&prog));
}

void test_handle_ecall_sbrk_zero_returns_current_break(void) {
  prog.heap_break = 0x5000;
  prog.stack_limit = 0x80000000u;
  prog.registers[17] = 9;
  prog.registers[10] = 0;

  TEST_ASSERT_EQUAL_INT(1, handle_ecall(&prog));
  TEST_ASSERT_EQUAL_UINT32(0x5000, prog.registers[10]);
  TEST_ASSERT_EQUAL_UINT32(0x5000, prog.heap_break);
}

void test_handle_ecall_sbrk_grows_heap(void) {
  prog.heap_break = 0x1000;
  prog.stack_limit = 0x80000000u;
  prog.registers[17] = 9;
  prog.registers[10] = PAGE_SIZE;

  TEST_ASSERT_EQUAL_INT(1, handle_ecall(&prog));
  TEST_ASSERT_EQUAL_UINT32(0x1000, prog.registers[10]); /* returns old break */
  TEST_ASSERT_EQUAL_UINT32(0x1000 + PAGE_SIZE, prog.heap_break);
}

void test_handle_ecall_sbrk_allows_growth_within_stack_limit(void) {
  prog.heap_break = 0;
  prog.stack_limit = PAGE_SIZE * 4;
  prog.registers[17] = 9;
  prog.registers[10] = PAGE_SIZE;

  TEST_ASSERT_EQUAL_INT(1, handle_ecall(&prog));
  TEST_ASSERT_EQUAL_UINT32(PAGE_SIZE, prog.heap_break);
}

void test_handle_ecall_sbrk_rejects_growth_past_stack_limit(void) {
  prog.heap_break = 0;
  prog.stack_limit = PAGE_SIZE; /* stack begins one page in */
  prog.registers[17] = 9;
  prog.registers[10] = PAGE_SIZE * 2; /* requests more than is available */

  int result = handle_ecall(&prog);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT32(0, prog.heap_break); /* unchanged on rejection */
}

void test_handle_ecall_sbrk_protects_existing_stack_memory(void) {
  mem_write_u8(&prog.pt, 0x2000, 0xAB, MEMORY_READ | MEMORY_WRITE); /* sentinel */
  prog.heap_break = 0x0;
  prog.stack_limit = 0x2000;
  prog.registers[17] = 9;
  prog.registers[10] = 0x3000; /* would grow well past stack_limit */

  int result = handle_ecall(&prog);

  TEST_ASSERT_EQUAL_INT(0, result);
  uint8_t val;
  mem_read_u8(&prog.pt, 0x2000, &val);
  TEST_ASSERT_EQUAL_UINT8(0xAB, val); /* sentinel untouched */
}

/* ---------------------------------------------------------------------
 * cpu_step_single
 * ------------------------------------------------------------------- */

void test_cpu_step_single_fetch_fault_halts(void) {
  prog.pc = 0x1000; /* unmapped */

  StepResult r = cpu_step_single(&prog);

  TEST_ASSERT_EQUAL(STEP_HALTED_FETCH, r.status);
  TEST_ASSERT_EQUAL(CPU_HALTED, prog.status);
}

void test_cpu_step_single_decode_fault_halts(void) {
  MemoryPage* page = page_alloc(&prog.pt, 0x1000, MEMORY_READ | MEMORY_EXECUTE);
  page->data[0] = 0x7F; /* reserved/unused opcode byte */
  page->data[1] = 0;
  page->data[2] = 0;
  page->data[3] = 0;
  prog.pc = 0x1000;

  StepResult r = cpu_step_single(&prog);

  TEST_ASSERT_EQUAL(STEP_HALTED_DECODE, r.status);
}

void test_cpu_step_single_executes_and_advances_pc(void) {
  MemoryPage* page = page_alloc(&prog.pt, 0x1000, MEMORY_READ | MEMORY_EXECUTE);
  /* addi x1, x0, 5 */
  uint32_t bin = (5u << 20) | (0u << 15) | (0u << 12) | (1u << 7) | 0x13u;
  page->data[0] = (uint8_t)(bin & 0xFF);
  page->data[1] = (uint8_t)((bin >> 8) & 0xFF);
  page->data[2] = (uint8_t)((bin >> 16) & 0xFF);
  page->data[3] = (uint8_t)((bin >> 24) & 0xFF);
  prog.pc = 0x1000;

  StepResult r = cpu_step_single(&prog);

  TEST_ASSERT_EQUAL(STEP_OK, r.status);
  TEST_ASSERT_EQUAL_UINT32(0x1000, r.pc_old);
  TEST_ASSERT_EQUAL_UINT32(0x1004, r.pc_new);
  TEST_ASSERT_EQUAL_UINT32(5, prog.registers[1]);
}

void test_cpu_step_single_ecall_exit(void) {
  MemoryPage* page = page_alloc(&prog.pt, 0x1000, MEMORY_READ | MEMORY_EXECUTE);
  uint32_t ecall_bin = 0x73;
  page->data[0] = (uint8_t)(ecall_bin & 0xFF);
  page->data[1] = 0;
  page->data[2] = 0;
  page->data[3] = 0;
  prog.pc = 0x1000;
  prog.registers[17] = 10; /* exit */

  StepResult r = cpu_step_single(&prog);

  TEST_ASSERT_EQUAL(STEP_EXITED, r.status);
  TEST_ASSERT_EQUAL(CPU_EXITED, prog.status);
}

/* ---------------------------------------------------------------------
 * step_status_is_fault
 * ------------------------------------------------------------------- */

void test_step_status_is_fault_classification(void) {
  TEST_ASSERT_TRUE(step_status_is_fault(STEP_HALTED_FETCH));
  TEST_ASSERT_TRUE(step_status_is_fault(STEP_HALTED_DECODE));
  TEST_ASSERT_TRUE(step_status_is_fault(STEP_HALTED_INVALID_OP));
  TEST_ASSERT_TRUE(step_status_is_fault(STEP_HALTED_MEM_READ));
  TEST_ASSERT_TRUE(step_status_is_fault(STEP_HALTED_MEM_WRITE));
  TEST_ASSERT_TRUE(step_status_is_fault(STEP_HALTED_ECALL_INVALID));

  TEST_ASSERT_FALSE(step_status_is_fault(STEP_OK));
  TEST_ASSERT_FALSE(step_status_is_fault(STEP_BRANCH_TAKEN));
  TEST_ASSERT_FALSE(step_status_is_fault(STEP_ECALL_HANDLED));
  TEST_ASSERT_FALSE(step_status_is_fault(STEP_EBREAK));
  TEST_ASSERT_FALSE(step_status_is_fault(STEP_EXITED));
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  init_decode_table();

  UNITY_BEGIN();

  RUN_TEST(test_handle_instruction_null_args_fail);
  RUN_TEST(test_handle_instruction_add);
  RUN_TEST(test_handle_instruction_never_writes_x0);
  RUN_TEST(test_handle_instruction_sub);
  RUN_TEST(test_handle_instruction_slt_signed);
  RUN_TEST(test_handle_instruction_sltu_unsigned);
  RUN_TEST(test_handle_instruction_sra_arithmetic_shift);
  RUN_TEST(test_handle_instruction_srl_logical_shift);
  RUN_TEST(test_handle_instruction_div_by_zero);
  RUN_TEST(test_handle_instruction_div_overflow_int_min_by_neg1);
  RUN_TEST(test_handle_instruction_rem_by_zero_returns_dividend);

  RUN_TEST(test_handle_instruction_addi);
  RUN_TEST(test_handle_instruction_lw_and_sw_roundtrip);
  RUN_TEST(test_handle_instruction_lb_sign_extends);
  RUN_TEST(test_handle_instruction_lbu_zero_extends);
  RUN_TEST(test_handle_instruction_load_fault_on_unmapped);
  RUN_TEST(test_handle_instruction_store_fault_on_readonly);
  RUN_TEST(test_handle_instruction_beq_taken);
  RUN_TEST(test_handle_instruction_beq_not_taken);
  RUN_TEST(test_handle_instruction_jal_sets_link_and_pc);
  RUN_TEST(test_handle_instruction_jalr_clears_low_bit);
  RUN_TEST(test_handle_instruction_ecall_and_ebreak);
  RUN_TEST(test_handle_instruction_invalid_op);

  RUN_TEST(test_handle_ecall_null_prog_fails);
  RUN_TEST(test_handle_ecall_print_int_succeeds);
  RUN_TEST(test_handle_ecall_exit_sets_status);
  RUN_TEST(test_handle_ecall_exit_with_code_sets_status_and_code);
  RUN_TEST(test_handle_ecall_unknown_syscall_fails);
  RUN_TEST(test_handle_ecall_sbrk_zero_returns_current_break);
  RUN_TEST(test_handle_ecall_sbrk_grows_heap);
  RUN_TEST(test_handle_ecall_sbrk_allows_growth_within_stack_limit);
  RUN_TEST(test_handle_ecall_sbrk_rejects_growth_past_stack_limit);
  RUN_TEST(test_handle_ecall_sbrk_protects_existing_stack_memory);

  RUN_TEST(test_cpu_step_single_fetch_fault_halts);
  RUN_TEST(test_cpu_step_single_decode_fault_halts);
  RUN_TEST(test_cpu_step_single_executes_and_advances_pc);
  RUN_TEST(test_cpu_step_single_ecall_exit);

  RUN_TEST(test_step_status_is_fault_classification);

  return UNITY_END();
}