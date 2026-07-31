#define _POSIX_C_SOURCE 200809L

#include "../src/memory.c"
#include "unity/unity.h"
#include <string.h>

static PageTable pt;

void setUp(void) { memset(&pt, 0, sizeof(PageTable)); }

void tearDown(void) {
  for (int i = 0; i < PT_SIZE; i++) {
    if (pt.l2[i]) {
      for (int j = 0; j < PT_SIZE; j++)
        free(pt.l2[i]->pages[j]);
      free(pt.l2[i]);
    }
  }
}

/* ---------------------------------------------------------------------
 * mem_access_result_to_str
 * ------------------------------------------------------------------- */

void test_mem_access_result_to_str_samples(void) {
  TEST_ASSERT_EQUAL_STRING("Access OK", mem_access_result_to_str(ACCESS_OK));
  TEST_ASSERT_EQUAL_STRING("Invalid address accessed", mem_access_result_to_str(ACCESS_INVALID_ADDRESS));
  TEST_ASSERT_EQUAL_STRING("Address does not have read permissions", mem_access_result_to_str(ACCESS_NO_READ));
  TEST_ASSERT_EQUAL_STRING("Address does not have write permissions", mem_access_result_to_str(ACCESS_NO_WRITE));
  TEST_ASSERT_EQUAL_STRING("Address does not have execute permissions", mem_access_result_to_str(ACCESS_NO_EXECUTE));
}

/* ---------------------------------------------------------------------
 * page_lookup / page_alloc
 * ------------------------------------------------------------------- */

void test_page_lookup_null_pt_returns_null(void) { TEST_ASSERT_NULL(page_lookup(NULL, 0)); }

void test_page_lookup_unmapped_returns_null(void) { TEST_ASSERT_NULL(page_lookup(&pt, 0x1000)); }

void test_page_alloc_null_pt_returns_null(void) { TEST_ASSERT_NULL(page_alloc(NULL, 0, MEMORY_READ)); }

void test_page_alloc_then_lookup_returns_same_page(void) {
  MemoryPage* allocated = page_alloc(&pt, 0x1000, MEMORY_READ | MEMORY_WRITE);

  TEST_ASSERT_NOT_NULL(allocated);
  TEST_ASSERT_EQUAL_UINT8(MEMORY_READ | MEMORY_WRITE, allocated->permissions);
  TEST_ASSERT_EQUAL_PTR(allocated, page_lookup(&pt, 0x1000));
}

void test_page_alloc_twice_reuses_existing_page(void) {
  MemoryPage* first = page_alloc(&pt, 0x1000, MEMORY_READ);
  MemoryPage* second = page_alloc(&pt, 0x1000, MEMORY_WRITE); /* different perms, should be ignored */

  TEST_ASSERT_EQUAL_PTR(first, second);
  TEST_ASSERT_EQUAL_UINT8(MEMORY_READ, second->permissions); /* unchanged from first alloc */
}

void test_page_alloc_different_pages_independent(void) {
  MemoryPage* a = page_alloc(&pt, 0x1000, MEMORY_READ);
  MemoryPage* b = page_alloc(&pt, 0x400000, MEMORY_WRITE); /* different L1 index */

  TEST_ASSERT_NOT_EQUAL(a, b);
}

/* ---------------------------------------------------------------------
 * page_resolve / page_resolve_alloc
 * ------------------------------------------------------------------- */

void test_page_resolve_null_args_fail(void) {
  uint8_t* out;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, page_resolve(NULL, 0, &out));
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, page_resolve(&pt, 0, NULL));
}

void test_page_resolve_unmapped_fails(void) {
  uint8_t* out;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, page_resolve(&pt, 0x1000, &out));
}

void test_page_resolve_mapped_returns_correct_offset(void) {
  MemoryPage* page = page_alloc(&pt, 0x1000, MEMORY_READ);
  page->data[0x10] = 0xAB;
  uint8_t* out;

  TEST_ASSERT_EQUAL(ACCESS_OK, page_resolve(&pt, 0x1010, &out));
  TEST_ASSERT_EQUAL_UINT8(0xAB, *out);
}

void test_page_resolve_alloc_null_args_fail(void) {
  uint8_t* out;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, page_resolve_alloc(NULL, 0, &out, MEMORY_READ));
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, page_resolve_alloc(&pt, 0, NULL, MEMORY_READ));
}

void test_page_resolve_alloc_allocates_when_unmapped(void) {
  uint8_t* out;
  TEST_ASSERT_EQUAL(ACCESS_OK, page_resolve_alloc(&pt, 0x1000, &out, MEMORY_WRITE));
  TEST_ASSERT_NOT_NULL(page_lookup(&pt, 0x1000));
}

/* ---------------------------------------------------------------------
 * mem_read_raw / mem_read_u8 / mem_read_u16 / mem_read_u32
 * ------------------------------------------------------------------- */

void test_mem_read_raw_null_args_fail(void) {
  uint32_t val;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_read_raw(NULL, 0, &val, sizeof(val)));
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_read_raw(&pt, 0, NULL, sizeof(val)));
}

void test_mem_read_raw_unmapped_fails(void) {
  uint32_t val;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_read_raw(&pt, 0x1000, &val, sizeof(val)));
}

void test_mem_read_raw_single_page(void) {
  MemoryPage* page = page_alloc(&pt, 0x1000, MEMORY_READ);
  page->data[0] = 0x78;
  page->data[1] = 0x56;
  page->data[2] = 0x34;
  page->data[3] = 0x12;

  uint32_t val = 0;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_raw(&pt, 0x1000, &val, sizeof(val)));
  TEST_ASSERT_EQUAL_UINT32(0x12345678, val);
}

void test_mem_read_raw_crosses_page_boundary(void) {
  MemoryPage* page0 = page_alloc(&pt, 0x1000, MEMORY_READ);
  MemoryPage* page1 = page_alloc(&pt, 0x2000, MEMORY_READ);
  page0->data[PAGE_SIZE - 2] = 0x78;
  page0->data[PAGE_SIZE - 1] = 0x56;
  page1->data[0] = 0x34;
  page1->data[1] = 0x12;

  uint32_t val = 0;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_raw(&pt, 0x1000 + PAGE_SIZE - 2, &val, sizeof(val)));
  TEST_ASSERT_EQUAL_UINT32(0x12345678, val);
}

void test_mem_read_raw_oversized_size_falls_back_to_safe_path(void) {
  MemoryPage* page0 = page_alloc(&pt, 0x1000, MEMORY_READ);
  MemoryPage* page1 = page_alloc(&pt, 0x1000 + PAGE_SIZE, MEMORY_READ);
  memset(page0->data, 0xAA, PAGE_SIZE);
  memset(page1->data, 0xBB, 4);

  uint8_t buf[PAGE_SIZE + 4] = {0};
  MemoryAccessResult r = mem_read_raw(&pt, 0x1000, buf, sizeof(buf));

  TEST_ASSERT_EQUAL(ACCESS_OK, r);
  TEST_ASSERT_EQUAL_UINT8(0xAA, buf[0]);
  TEST_ASSERT_EQUAL_UINT8(0xAA, buf[PAGE_SIZE - 1]);
  TEST_ASSERT_EQUAL_UINT8(0xBB, buf[PAGE_SIZE]);
  TEST_ASSERT_EQUAL_UINT8(0xBB, buf[PAGE_SIZE + 3]);
}

void test_mem_read_u8_null_args_fail(void) {
  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_read_u8(NULL, 0, &val));
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_read_u8(&pt, 0, NULL));
}

void test_mem_read_u8_unmapped_fails(void) {
  uint8_t val;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_read_u8(&pt, 0x1000, &val));
}

void test_mem_read_u8_no_read_permission_fails(void) {
  MemoryPage* page = page_alloc(&pt, 0x1000, MEMORY_WRITE); /* no READ */
  page->data[0] = 42;
  uint8_t val;

  TEST_ASSERT_EQUAL(ACCESS_NO_READ, mem_read_u8(&pt, 0x1000, &val));
}

void test_mem_read_u8_with_permission_succeeds(void) {
  MemoryPage* page = page_alloc(&pt, 0x1000, MEMORY_READ);
  page->data[0] = 42;
  uint8_t val = 0;

  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u8(&pt, 0x1000, &val));
  TEST_ASSERT_EQUAL_UINT8(42, val);
}

void test_mem_read_u16_with_permission_succeeds(void) {
  MemoryPage* page = page_alloc(&pt, 0x1000, MEMORY_READ);
  page->data[0] = 0x34;
  page->data[1] = 0x12;
  uint16_t val = 0;

  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u16(&pt, 0x1000, &val));
  TEST_ASSERT_EQUAL_UINT16(0x1234, val);
}

void test_mem_read_u32_with_permission_succeeds(void) {
  MemoryPage* page = page_alloc(&pt, 0x1000, MEMORY_READ);
  page->data[0] = 0x78;
  page->data[1] = 0x56;
  page->data[2] = 0x34;
  page->data[3] = 0x12;
  uint32_t val = 0;

  TEST_ASSERT_EQUAL(ACCESS_OK, mem_read_u32(&pt, 0x1000, &val));
  TEST_ASSERT_EQUAL_UINT32(0x12345678, val);
}

/* ---------------------------------------------------------------------
 * mem_write_raw
 * ------------------------------------------------------------------- */

void test_mem_write_raw_null_args_fail(void) {
  uint32_t val = 5;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_write_raw(NULL, 0, &val, sizeof(val), MEMORY_WRITE));
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_write_raw(&pt, 0, NULL, sizeof(val), MEMORY_WRITE));
}

void test_mem_write_raw_allocates_page(void) {
  uint32_t val = 0x12345678;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_raw(&pt, 0x1000, &val, sizeof(val), MEMORY_WRITE));

  MemoryPage* page = page_lookup(&pt, 0x1000);
  TEST_ASSERT_NOT_NULL(page);
  TEST_ASSERT_EQUAL_UINT8(0x78, page->data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x12, page->data[3]);
}

void test_mem_write_raw_crosses_page_boundary(void) {
  uint32_t val = 0x12345678;
  uint32_t addr = 0x1000 + PAGE_SIZE - 2;

  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_raw(&pt, addr, &val, sizeof(val), MEMORY_WRITE));

  MemoryPage* page0 = page_lookup(&pt, 0x1000);
  MemoryPage* page1 = page_lookup(&pt, 0x2000);
  TEST_ASSERT_EQUAL_UINT8(0x78, page0->data[PAGE_SIZE - 2]);
  TEST_ASSERT_EQUAL_UINT8(0x56, page0->data[PAGE_SIZE - 1]);
  TEST_ASSERT_EQUAL_UINT8(0x34, page1->data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x12, page1->data[1]);
}

void test_mem_write_raw_is_intentionally_permissionless(void) {
  page_alloc(&pt, 0x1000, MEMORY_READ); /* no MEMORY_WRITE */
  uint32_t val = 0xDEADBEEF;

  MemoryAccessResult result = mem_write_raw(&pt, 0x1000, &val, sizeof(val), MEMORY_READ);

  TEST_ASSERT_EQUAL(ACCESS_OK, result);

  uint32_t readback;
  mem_read_u32(&pt, 0x1000, &readback);
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, readback);
}

/* ---------------------------------------------------------------------
 * mem_write_u8 / mem_write_u16 / mem_write_u32
 * ------------------------------------------------------------------- */

void test_mem_write_u8_null_pt_fails(void) {
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_write_u8(NULL, 0, 1, MEMORY_WRITE));
}

void test_mem_write_u8_unmapped_auto_allocates(void) {
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_u8(&pt, 0x1000, 42, MEMORY_WRITE));
  MemoryPage* page = page_lookup(&pt, 0x1000);
  TEST_ASSERT_NOT_NULL(page);
  TEST_ASSERT_EQUAL_UINT8(42, page->data[0]);
}

void test_mem_write_u8_readonly_page_fails(void) {
  page_alloc(&pt, 0x1000, MEMORY_READ); /* no WRITE */
  TEST_ASSERT_EQUAL(ACCESS_NO_WRITE, mem_write_u8(&pt, 0x1000, 42, MEMORY_READ));
}

void test_mem_write_u8_writable_page_succeeds(void) {
  page_alloc(&pt, 0x1000, MEMORY_READ | MEMORY_WRITE);
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_u8(&pt, 0x1000, 42, MEMORY_WRITE));

  uint8_t val;
  mem_read_u8(&pt, 0x1000, &val);
  TEST_ASSERT_EQUAL_UINT8(42, val);
}

void test_mem_write_u16_writable_page_succeeds(void) {
  page_alloc(&pt, 0x1000, MEMORY_READ | MEMORY_WRITE);
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_u16(&pt, 0x1000, 0x1234, MEMORY_WRITE));

  uint16_t val;
  mem_read_u16(&pt, 0x1000, &val);
  TEST_ASSERT_EQUAL_UINT16(0x1234, val);
}

void test_mem_write_u32_writable_page_succeeds(void) {
  page_alloc(&pt, 0x1000, MEMORY_READ | MEMORY_WRITE);
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_write_u32(&pt, 0x1000, 0x12345678, MEMORY_WRITE));

  uint32_t val;
  mem_read_u32(&pt, 0x1000, &val);
  TEST_ASSERT_EQUAL_UINT32(0x12345678, val);
}

/* ---------------------------------------------------------------------
 * mem_fetch_instruction
 * ------------------------------------------------------------------- */

void test_mem_fetch_instruction_null_args_fail(void) {
  uint32_t val;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_fetch_instruction(NULL, 0, &val));
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_fetch_instruction(&pt, 0, NULL));
}

void test_mem_fetch_instruction_unmapped_fails(void) {
  uint32_t val;
  TEST_ASSERT_EQUAL(ACCESS_INVALID_ADDRESS, mem_fetch_instruction(&pt, 0x1000, &val));
}

void test_mem_fetch_instruction_no_execute_permission_fails(void) {
  page_alloc(&pt, 0x1000, MEMORY_READ);
  uint32_t val;
  TEST_ASSERT_EQUAL(ACCESS_NO_EXECUTE, mem_fetch_instruction(&pt, 0x1000, &val));
}

void test_mem_fetch_instruction_with_execute_succeeds(void) {
  MemoryPage* page = page_alloc(&pt, 0x1000, MEMORY_EXECUTE);
  page->data[0] = 0x33;
  page->data[1] = 0x01;
  page->data[2] = 0x00;
  page->data[3] = 0x00;

  uint32_t val = 0;
  TEST_ASSERT_EQUAL(ACCESS_OK, mem_fetch_instruction(&pt, 0x1000, &val));
  TEST_ASSERT_EQUAL_UINT32(0x00000133, val);
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_mem_access_result_to_str_samples);

  RUN_TEST(test_page_lookup_null_pt_returns_null);
  RUN_TEST(test_page_lookup_unmapped_returns_null);
  RUN_TEST(test_page_alloc_null_pt_returns_null);
  RUN_TEST(test_page_alloc_then_lookup_returns_same_page);
  RUN_TEST(test_page_alloc_twice_reuses_existing_page);
  RUN_TEST(test_page_alloc_different_pages_independent);

  RUN_TEST(test_page_resolve_null_args_fail);
  RUN_TEST(test_page_resolve_unmapped_fails);
  RUN_TEST(test_page_resolve_mapped_returns_correct_offset);
  RUN_TEST(test_page_resolve_alloc_null_args_fail);
  RUN_TEST(test_page_resolve_alloc_allocates_when_unmapped);

  RUN_TEST(test_mem_read_raw_null_args_fail);
  RUN_TEST(test_mem_read_raw_unmapped_fails);
  RUN_TEST(test_mem_read_raw_single_page);
  RUN_TEST(test_mem_read_raw_crosses_page_boundary);
  RUN_TEST(test_mem_read_raw_oversized_size_falls_back_to_safe_path);

  RUN_TEST(test_mem_read_u8_null_args_fail);
  RUN_TEST(test_mem_read_u8_unmapped_fails);
  RUN_TEST(test_mem_read_u8_no_read_permission_fails);
  RUN_TEST(test_mem_read_u8_with_permission_succeeds);
  RUN_TEST(test_mem_read_u16_with_permission_succeeds);
  RUN_TEST(test_mem_read_u32_with_permission_succeeds);

  RUN_TEST(test_mem_write_raw_null_args_fail);
  RUN_TEST(test_mem_write_raw_allocates_page);
  RUN_TEST(test_mem_write_raw_crosses_page_boundary);
  RUN_TEST(test_mem_write_raw_is_intentionally_permissionless);

  RUN_TEST(test_mem_write_u8_null_pt_fails);
  RUN_TEST(test_mem_write_u8_unmapped_auto_allocates);
  RUN_TEST(test_mem_write_u8_readonly_page_fails);
  RUN_TEST(test_mem_write_u8_writable_page_succeeds);
  RUN_TEST(test_mem_write_u16_writable_page_succeeds);
  RUN_TEST(test_mem_write_u32_writable_page_succeeds);

  RUN_TEST(test_mem_fetch_instruction_null_args_fail);
  RUN_TEST(test_mem_fetch_instruction_unmapped_fails);
  RUN_TEST(test_mem_fetch_instruction_no_execute_permission_fails);
  RUN_TEST(test_mem_fetch_instruction_with_execute_succeeds);

  return UNITY_END();
}