#define _POSIX_C_SOURCE 200809L

#include "../src/loader.c"
#include "unity/unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static AssembledProgram make_program(uint8_t* text, size_t text_size, uint8_t* rodata, size_t rodata_size,
                                     uint8_t* data, size_t data_size, uint8_t* bss, size_t bss_size,
                                     uint32_t entry_offset) {
  AssembledProgram prog = {0};
  prog.text.data = text;
  prog.text.size = text_size;
  prog.rodata.data = rodata;
  prog.rodata.size = rodata_size;
  prog.data.data = data;
  prog.data.size = data_size;
  prog.bss.data = bss;
  prog.bss.size = bss_size;
  prog.entry_offset = entry_offset;
  return prog;
}

/* ---------------------------------------------------------------------
 * load_mem_segment
 * ------------------------------------------------------------------- */

void test_load_mem_segment_null_args_fail(void) {
  MemorySegment seg = {0};
  PageTable pt = {0};
  TEST_ASSERT_EQUAL_INT(0, load_mem_segment(NULL, 0, &seg, MEMORY_READ));
  TEST_ASSERT_EQUAL_INT(0, load_mem_segment(&pt, 0, NULL, MEMORY_READ));
}

void test_load_mem_segment_zero_size_is_noop_success(void) {
  MemorySegment seg = {0};
  seg.size = 0;
  PageTable pt = {0};

  TEST_ASSERT_EQUAL_INT(1, load_mem_segment(&pt, 0, &seg, MEMORY_READ));
  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_read_u8(&pt, 0, &val));
}

void test_load_mem_segment_writes_data(void) {
  uint8_t data[4] = {0xAA, 0xBB, 0xCC, 0xDD};
  MemorySegment seg = {0};
  seg.data = data;
  seg.size = 4;
  PageTable pt = {0};

  TEST_ASSERT_EQUAL_INT(1, load_mem_segment(&pt, 0x1000, &seg, MEMORY_READ | MEMORY_WRITE));

  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&pt, 0x1000, &val));
  TEST_ASSERT_EQUAL_UINT8(0xAA, val);
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&pt, 0x1003, &val));
  TEST_ASSERT_EQUAL_UINT8(0xDD, val);

  for (int i = 0; i < PT_SIZE; i++) {
    if (pt.l2[i]) {
      for (int j = 0; j < PT_SIZE; j++)
        free(pt.l2[i]->pages[j]);
      free(pt.l2[i]);
    }
  }
}

/* ---------------------------------------------------------------------
 * load_binary / free_loaded_binary
 * ------------------------------------------------------------------- */

void test_load_binary_null_prog_returns_null(void) { TEST_ASSERT_NULL(load_binary(NULL)); }

void test_load_binary_sets_pc_and_stack_pointer(void) {
  uint8_t text[4] = {0x13, 0x00, 0x00, 0x00}; /* addi x0,x0,0 (nop) */
  AssembledProgram prog = make_program(text, 4, NULL, 0, NULL, 0, NULL, 0, 0);

  ProgramState* state = load_binary(&prog);

  TEST_ASSERT_NOT_NULL(state);
  TEST_ASSERT_EQUAL_UINT32(0, state->pc);
  TEST_ASSERT_EQUAL_UINT32(0x80000000u, state->registers[2]);

  free_loaded_binary(state);
}

void test_load_binary_text_is_readable_but_not_writable(void) {
  uint8_t text[4] = {0x13, 0x00, 0x00, 0x00};
  AssembledProgram prog = make_program(text, 4, NULL, 0, NULL, 0, NULL, 0, 0);

  ProgramState* state = load_binary(&prog);
  TEST_ASSERT_NOT_NULL(state);

  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&state->pt, 0, &val));
  TEST_ASSERT_EQUAL_UINT8(0x13, val);
  TEST_ASSERT_EQUAL(ACCESS_NO_WRITE, mem_write_u8(&state->pt, 0, 0xFF, 0));

  free_loaded_binary(state);
}

void test_load_binary_data_is_readable_and_writable(void) {
  uint8_t text[4] = {0};
  uint8_t data[4] = {1, 2, 3, 4};
  AssembledProgram prog = make_program(text, 4, NULL, 0, data, 4, NULL, 0, 0);

  ProgramState* state = load_binary(&prog);
  TEST_ASSERT_NOT_NULL(state);

  /* data segment is placed after .text, page-rounded */
  uint32_t data_base = (4 + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&state->pt, data_base, &val));
  TEST_ASSERT_EQUAL_UINT8(1, val);
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_u8(&state->pt, data_base, 99, MEMORY_WRITE));

  free_loaded_binary(state);
}

void test_load_binary_rodata_is_readable_but_not_writable(void) {
  uint8_t text[4] = {0};
  uint8_t rodata[4] = {9, 9, 9, 9};
  AssembledProgram prog = make_program(text, 4, rodata, 4, NULL, 0, NULL, 0, 0);

  ProgramState* state = load_binary(&prog);
  TEST_ASSERT_NOT_NULL(state);

  uint32_t rodata_base = (4 + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&state->pt, rodata_base, &val));
  TEST_ASSERT_EQUAL_UINT8(9, val);
  TEST_ASSERT_EQUAL(ACCESS_NO_WRITE, mem_write_u8(&state->pt, rodata_base, 1, 0));

  free_loaded_binary(state);
}

void test_load_binary_stack_guard_page_is_unmapped(void) {
  uint8_t text[4] = {0};
  AssembledProgram prog = make_program(text, 4, NULL, 0, NULL, 0, NULL, 0, 0);

  ProgramState* state = load_binary(&prog);
  TEST_ASSERT_NOT_NULL(state);

  const uint32_t stack_top = 0x80000000u;
  const uint32_t stack_size = 2u * 1024u * 1024u;
  const uint32_t stack_guard_bytes = PAGE_SIZE;
  const uint32_t stack_base = stack_top - stack_guard_bytes - stack_size;

  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_read_u8(&state->pt, stack_base, &val));

  free_loaded_binary(state);
}

void test_load_binary_stack_usable_region_is_mapped(void) {
  uint8_t text[4] = {0};
  AssembledProgram prog = make_program(text, 4, NULL, 0, NULL, 0, NULL, 0, 0);

  ProgramState* state = load_binary(&prog);
  TEST_ASSERT_NOT_NULL(state);

  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&state->pt, 0x80000000u - 4, &val));
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_u8(&state->pt, 0x80000000u - 4, 5, MEMORY_WRITE));

  free_loaded_binary(state);
}

void test_load_binary_sets_stack_limit(void) {
  uint8_t text[4] = {0};
  AssembledProgram prog = make_program(text, 4, NULL, 0, NULL, 0, NULL, 0, 0);

  const uint32_t stack_top = 0x80000000u;
  const uint32_t stack_size = 2u * 1024u * 1024u;
  const uint32_t stack_guard_bytes = PAGE_SIZE;
  const uint32_t expected_stack_base = stack_top - stack_guard_bytes - stack_size;

  ProgramState* state = load_binary(&prog);

  TEST_ASSERT_NOT_NULL(state);
  TEST_ASSERT_EQUAL_UINT32(expected_stack_base, state->stack_limit);

  free_loaded_binary(state);
}

void test_load_binary_heap_break_is_page_aligned_after_bss(void) {
  uint8_t text[4] = {0};
  uint8_t bss_dummy[10] = {0}; /* only size matters; .bss content is zero anyway */
  AssembledProgram prog = make_program(text, 4, NULL, 0, NULL, 0, bss_dummy, 10, 0);

  ProgramState* state = load_binary(&prog);

  TEST_ASSERT_NOT_NULL(state);
  TEST_ASSERT_EQUAL_UINT32(0, state->heap_break % PAGE_SIZE);
  TEST_ASSERT_TRUE(state->heap_break > 0);

  free_loaded_binary(state);
}

void test_free_loaded_binary_null_does_not_crash(void) {
  free_loaded_binary(NULL);
  TEST_PASS();
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_load_mem_segment_null_args_fail);
  RUN_TEST(test_load_mem_segment_zero_size_is_noop_success);
  RUN_TEST(test_load_mem_segment_writes_data);

  RUN_TEST(test_load_binary_null_prog_returns_null);
  RUN_TEST(test_load_binary_sets_pc_and_stack_pointer);
  RUN_TEST(test_load_binary_text_is_readable_but_not_writable);
  RUN_TEST(test_load_binary_data_is_readable_and_writable);
  RUN_TEST(test_load_binary_rodata_is_readable_but_not_writable);
  RUN_TEST(test_load_binary_stack_guard_page_is_unmapped);
  RUN_TEST(test_load_binary_stack_usable_region_is_mapped);
  RUN_TEST(test_load_binary_sets_stack_limit);
  RUN_TEST(test_load_binary_heap_break_is_page_aligned_after_bss);
  RUN_TEST(test_free_loaded_binary_null_does_not_crash);

  return UNITY_END();
}