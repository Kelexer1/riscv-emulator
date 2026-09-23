#define _POSIX_C_SOURCE 200809L

#include "../src/loader.c"
#include "unity/unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct {
  uint32_t vaddr;
  uint32_t filesz;
  uint32_t memsz;
  uint32_t flags;
  const uint8_t* data;
} SegSpec;

static Elf32Image make_image(uint32_t entry, const SegSpec* specs, size_t n) {
  Elf32Image img = {0};
  img.entry = entry;
  img.nsegments = n;
  for (size_t i = 0; i < n; i++) {
    img.segments[i].vaddr = specs[i].vaddr;
    img.segments[i].filesz = specs[i].filesz;
    img.segments[i].memsz = specs[i].memsz;
    img.segments[i].flags = specs[i].flags;
    img.segments[i].data = specs[i].data;
  }
  return img;
}

/* rodata/data/bss are placed right after .text, page-rounded, matching the layout the assembler's
 * elf_emit.c lays out; individual tests only need one extra segment at a time */
static uint32_t after_text(uint32_t text_size) { return (text_size + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u); }

/* ---------------------------------------------------------------------
 * zero_fill / segments_share_page / entry_is_executable
 * ------------------------------------------------------------------- */

void test_zero_fill_zero_len_is_noop_success(void) {
  PageTable pt = {0};
  TEST_ASSERT_EQUAL_INT(1, zero_fill(&pt, 0x1000, 0, MEMORY_READ | MEMORY_WRITE));
}

void test_zero_fill_writes_zeros_across_page_boundary(void) {
  PageTable pt = {0};
  TEST_ASSERT_EQUAL_INT(1, zero_fill(&pt, PAGE_SIZE - 2, 4, MEMORY_READ | MEMORY_WRITE));

  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&pt, PAGE_SIZE - 2, &val));
  TEST_ASSERT_EQUAL_UINT8(0, val);
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&pt, PAGE_SIZE + 1, &val));
  TEST_ASSERT_EQUAL_UINT8(0, val);

  free_page_table(&pt);
}

void test_segments_share_page_detects_overlap(void) {
  SegSpec specs[] = {
      {0, 4, 4, PF_R | PF_X, NULL},
      {PAGE_SIZE - 4, 4, 4, PF_R | PF_W, NULL},
  };
  Elf32Image img = make_image(0, specs, 2);
  TEST_ASSERT_EQUAL_INT(1, segments_share_page(&img));
}

void test_segments_share_page_false_when_page_aligned(void) {
  SegSpec specs[] = {
      {0, 4, 4, PF_R | PF_X, NULL},
      {PAGE_SIZE, 4, 4, PF_R | PF_W, NULL},
  };
  Elf32Image img = make_image(0, specs, 2);
  TEST_ASSERT_EQUAL_INT(0, segments_share_page(&img));
}

void test_entry_is_executable_true_inside_x_segment(void) {
  SegSpec specs[] = {{0, 8, 8, PF_R | PF_X, NULL}};
  Elf32Image img = make_image(4, specs, 1);
  TEST_ASSERT_EQUAL_INT(1, entry_is_executable(&img));
}

void test_entry_is_executable_false_outside_any_segment(void) {
  SegSpec specs[] = {{0, 8, 8, PF_R | PF_X, NULL}};
  Elf32Image img = make_image(0x1000, specs, 1);
  TEST_ASSERT_EQUAL_INT(0, entry_is_executable(&img));
}

void test_entry_is_executable_false_in_non_x_segment(void) {
  SegSpec specs[] = {{0, 8, 8, PF_R | PF_W, NULL}};
  Elf32Image img = make_image(0, specs, 1);
  TEST_ASSERT_EQUAL_INT(0, entry_is_executable(&img));
}

/* ---------------------------------------------------------------------
 * load_binary / free_loaded_binary
 * ------------------------------------------------------------------- */

void test_load_binary_null_img_returns_null(void) { TEST_ASSERT_NULL(load_binary(NULL)); }

void test_load_binary_sets_pc_and_stack_pointer(void) {
  uint8_t text[4] = {0x13, 0x00, 0x00, 0x00}; /* addi x0,x0,0 (nop) */
  SegSpec specs[] = {{0, 4, 4, PF_R | PF_X, text}};
  Elf32Image img = make_image(0, specs, 1);

  ProgramState* state = load_binary(&img);

  TEST_ASSERT_NOT_NULL(state);
  TEST_ASSERT_EQUAL_UINT32(0, state->pc);
  TEST_ASSERT_EQUAL_UINT32(0x80000000u, state->registers[2]);

  free_loaded_binary(state);
}

void test_load_binary_text_is_readable_but_not_writable(void) {
  uint8_t text[4] = {0x13, 0x00, 0x00, 0x00};
  SegSpec specs[] = {{0, 4, 4, PF_R | PF_X, text}};
  Elf32Image img = make_image(0, specs, 1);

  ProgramState* state = load_binary(&img);
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
  uint32_t data_base = after_text(4);
  SegSpec specs[] = {
      {0, 4, 4, PF_R | PF_X, text},
      {data_base, 4, 4, PF_R | PF_W, data},
  };
  Elf32Image img = make_image(0, specs, 2);

  ProgramState* state = load_binary(&img);
  TEST_ASSERT_NOT_NULL(state);

  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&state->pt, data_base, &val));
  TEST_ASSERT_EQUAL_UINT8(1, val);
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_u8(&state->pt, data_base, 99, MEMORY_WRITE));

  free_loaded_binary(state);
}

void test_load_binary_rodata_is_readable_but_not_writable(void) {
  uint8_t text[4] = {0};
  uint8_t rodata[4] = {9, 9, 9, 9};
  uint32_t rodata_base = after_text(4);
  SegSpec specs[] = {
      {0, 4, 4, PF_R | PF_X, text},
      {rodata_base, 4, 4, PF_R, rodata},
  };
  Elf32Image img = make_image(0, specs, 2);

  ProgramState* state = load_binary(&img);
  TEST_ASSERT_NOT_NULL(state);

  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&state->pt, rodata_base, &val));
  TEST_ASSERT_EQUAL_UINT8(9, val);
  TEST_ASSERT_EQUAL(ACCESS_NO_WRITE, mem_write_u8(&state->pt, rodata_base, 1, 0));

  free_loaded_binary(state);
}

void test_load_binary_bss_is_zero_filled_and_writable(void) {
  uint8_t text[4] = {0};
  uint32_t bss_base = after_text(4);
  /* NOBITS segment: filesz 0, memsz 10, no backing data */
  SegSpec specs[] = {
      {0, 4, 4, PF_R | PF_X, text},
      {bss_base, 0, 10, PF_R | PF_W, NULL},
  };
  Elf32Image img = make_image(0, specs, 2);

  ProgramState* state = load_binary(&img);
  TEST_ASSERT_NOT_NULL(state);

  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&state->pt, bss_base, &val));
  TEST_ASSERT_EQUAL_UINT8(0, val);
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_u8(&state->pt, bss_base, 7, MEMORY_WRITE));

  free_loaded_binary(state);
}

void test_load_binary_stack_guard_page_is_unmapped(void) {
  uint8_t text[4] = {0};
  SegSpec specs[] = {{0, 4, 4, PF_R | PF_X, text}};
  Elf32Image img = make_image(0, specs, 1);

  ProgramState* state = load_binary(&img);
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
  SegSpec specs[] = {{0, 4, 4, PF_R | PF_X, text}};
  Elf32Image img = make_image(0, specs, 1);

  ProgramState* state = load_binary(&img);
  TEST_ASSERT_NOT_NULL(state);

  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&state->pt, 0x80000000u - 4, &val));
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_u8(&state->pt, 0x80000000u - 4, 5, MEMORY_WRITE));

  free_loaded_binary(state);
}

void test_load_binary_sets_stack_limit(void) {
  uint8_t text[4] = {0};
  SegSpec specs[] = {{0, 4, 4, PF_R | PF_X, text}};
  Elf32Image img = make_image(0, specs, 1);

  const uint32_t stack_top = 0x80000000u;
  const uint32_t stack_size = 2u * 1024u * 1024u;
  const uint32_t stack_guard_bytes = PAGE_SIZE;
  const uint32_t expected_stack_base = stack_top - stack_guard_bytes - stack_size;

  ProgramState* state = load_binary(&img);

  TEST_ASSERT_NOT_NULL(state);
  TEST_ASSERT_EQUAL_UINT32(expected_stack_base, state->stack_limit);

  free_loaded_binary(state);
}

void test_load_binary_heap_break_is_page_aligned_after_last_segment(void) {
  uint8_t text[4] = {0};
  uint32_t bss_base = after_text(4);
  SegSpec specs[] = {
      {0, 4, 4, PF_R | PF_X, text},
      {bss_base, 0, 10, PF_R | PF_W, NULL},
  };
  Elf32Image img = make_image(0, specs, 2);

  ProgramState* state = load_binary(&img);

  TEST_ASSERT_NOT_NULL(state);
  TEST_ASSERT_EQUAL_UINT32(0, state->heap_break % PAGE_SIZE);
  TEST_ASSERT_TRUE(state->heap_break > bss_base);

  free_loaded_binary(state);
}

void test_load_binary_rejects_segments_sharing_a_page(void) {
  uint8_t text[4] = {0};
  uint8_t data[4] = {0};
  SegSpec specs[] = {
      {0, 4, 4, PF_R | PF_X, text},
      {8, 4, 4, PF_R | PF_W, data}, /* same page as .text */
  };
  Elf32Image img = make_image(0, specs, 2);

  TEST_ASSERT_NULL(load_binary(&img));
}

void test_load_binary_rejects_entry_outside_executable_segment(void) {
  uint8_t text[4] = {0};
  SegSpec specs[] = {{0, 4, 4, PF_R | PF_X, text}};
  Elf32Image img = make_image(0x2000, specs, 1);

  TEST_ASSERT_NULL(load_binary(&img));
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

  RUN_TEST(test_zero_fill_zero_len_is_noop_success);
  RUN_TEST(test_zero_fill_writes_zeros_across_page_boundary);
  RUN_TEST(test_segments_share_page_detects_overlap);
  RUN_TEST(test_segments_share_page_false_when_page_aligned);
  RUN_TEST(test_entry_is_executable_true_inside_x_segment);
  RUN_TEST(test_entry_is_executable_false_outside_any_segment);
  RUN_TEST(test_entry_is_executable_false_in_non_x_segment);

  RUN_TEST(test_load_binary_null_img_returns_null);
  RUN_TEST(test_load_binary_sets_pc_and_stack_pointer);
  RUN_TEST(test_load_binary_text_is_readable_but_not_writable);
  RUN_TEST(test_load_binary_data_is_readable_and_writable);
  RUN_TEST(test_load_binary_rodata_is_readable_but_not_writable);
  RUN_TEST(test_load_binary_bss_is_zero_filled_and_writable);
  RUN_TEST(test_load_binary_stack_guard_page_is_unmapped);
  RUN_TEST(test_load_binary_stack_usable_region_is_mapped);
  RUN_TEST(test_load_binary_sets_stack_limit);
  RUN_TEST(test_load_binary_heap_break_is_page_aligned_after_last_segment);
  RUN_TEST(test_load_binary_rejects_segments_sharing_a_page);
  RUN_TEST(test_load_binary_rejects_entry_outside_executable_segment);
  RUN_TEST(test_free_loaded_binary_null_does_not_crash);

  return UNITY_END();
}