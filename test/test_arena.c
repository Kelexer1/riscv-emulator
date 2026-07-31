#include "../include/arena.h"
#include "unity/unity.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static Arena arena;

void setUp(void) { memset(&arena, 0, sizeof(Arena)); }

void tearDown(void) { arena_free(&arena); }

/* ---------------------------------------------------------------------
 * arena_init
 * ------------------------------------------------------------------- */

void test_arena_init_normal_capacity_succeeds(void) {
  int result = arena_init(&arena, 64);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_NOT_NULL(arena.data);
  TEST_ASSERT_EQUAL_UINT(64, arena.capacity);
  TEST_ASSERT_EQUAL_UINT(0, arena.used);
}

void test_arena_init_zero_capacity_coerces_to_one(void) {
  int result = arena_init(&arena, 0);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_NOT_NULL(arena.data);
  TEST_ASSERT_EQUAL_UINT(1, arena.capacity);
}

void test_arena_init_null_arena_returns_zero(void) {
  int result = arena_init(NULL, 64);

  TEST_ASSERT_EQUAL_INT(0, result);
}

void test_arena_init_large_capacity_succeeds(void) {
  int result = arena_init(&arena, 1024 * 1024);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_EQUAL_UINT(1024 * 1024, arena.capacity);
}

void test_arena_init_reinit_overwrites_previous_state(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 16));
  char* first_buffer = arena.data;

  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 32));
  TEST_ASSERT_NOT_EQUAL(first_buffer, arena.data);
  free(first_buffer);
}

/* ---------------------------------------------------------------------
 * arena_alloc
 * ------------------------------------------------------------------- */

void test_arena_alloc_basic_copy_and_null_terminates(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 64));

  char* result = arena_alloc(&arena, "hello", 5);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("hello", result);
  TEST_ASSERT_EQUAL_UINT(6, arena.used); /* 5 bytes + null terminator */
}

void test_arena_alloc_null_arena_returns_null(void) {
  char* result = arena_alloc(NULL, "hello", 5);

  TEST_ASSERT_NULL(result);
}

void test_arena_alloc_null_src_returns_null(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 64));

  char* result = arena_alloc(&arena, NULL, 5);

  TEST_ASSERT_NULL(result);
}

void test_arena_alloc_zero_length_still_null_terminates(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 64));

  char* result = arena_alloc(&arena, "", 0);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("", result);
  TEST_ASSERT_EQUAL_UINT(1, arena.used);
}

void test_arena_alloc_exact_fit_succeeds(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 6));

  /* len=5 + 1 null terminator == capacity exactly. */
  char* result = arena_alloc(&arena, "hello", 5);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(6, arena.used);
}

void test_arena_alloc_one_byte_over_capacity_fails(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 5));

  /* len=5 + 1 null terminator == 6 > capacity of 5. */
  char* result = arena_alloc(&arena, "hello", 5);

  TEST_ASSERT_NULL(result);
  TEST_ASSERT_EQUAL_UINT(0, arena.used); /* used must not be mutated on failure */
}

void test_arena_alloc_sequential_allocations_pack_contiguously(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 64));

  char* first = arena_alloc(&arena, "ab", 2);
  char* second = arena_alloc(&arena, "cde", 3);

  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_NOT_NULL(second);
  TEST_ASSERT_EQUAL_STRING("ab", first);
  TEST_ASSERT_EQUAL_STRING("cde", second);
  TEST_ASSERT_EQUAL_PTR(first + 3, second); /* 2 bytes + null terminator */
  TEST_ASSERT_EQUAL_UINT(7, arena.used);
}

void test_arena_alloc_returns_null_once_full_but_preserves_prior_data(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 4));

  char* first = arena_alloc(&arena, "ab", 2); /* uses 3 of 4 bytes */
  TEST_ASSERT_NOT_NULL(first);

  char* second = arena_alloc(&arena, "zz", 2); /* needs 3 more, only 1 left */
  TEST_ASSERT_NULL(second);

  /* Earlier allocation must remain intact after a failed later allocation. */
  TEST_ASSERT_EQUAL_STRING("ab", first);
}

void test_arena_alloc_does_not_null_terminate_beyond_capacity(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 6));

  char* result = arena_alloc(&arena, "abcde", 5);

  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_CHAR('\0', result[5]);
}

/* ---------------------------------------------------------------------
 * arena_free
 * ------------------------------------------------------------------- */

void test_arena_free_resets_fields(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 64));
  arena_alloc(&arena, "data", 4);

  arena_free(&arena);

  TEST_ASSERT_NULL(arena.data);
  TEST_ASSERT_EQUAL_UINT(0, arena.capacity);
  TEST_ASSERT_EQUAL_UINT(0, arena.used);
}

void test_arena_free_null_arena_does_not_crash(void) {
  arena_free(NULL); /* should be a safe no-op */
  TEST_PASS();
}

void test_arena_free_double_free_is_safe(void) {
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 32));

  arena_free(&arena);
  arena_free(&arena); /* data is NULL, free(NULL) is a no-op */

  TEST_ASSERT_NULL(arena.data);
  TEST_ASSERT_EQUAL_UINT(0, arena.capacity);
  TEST_ASSERT_EQUAL_UINT(0, arena.used);
}

void test_arena_alloc_after_free_fails_safely(void) {
  /* After free, capacity is 0, so any non-overflowing len should be
   * rejected by the capacity check rather than dereferencing NULL data. */
  TEST_ASSERT_EQUAL_INT(1, arena_init(&arena, 32));
  arena_free(&arena);

  char* result = arena_alloc(&arena, "x", 1);

  TEST_ASSERT_NULL(result);
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_arena_init_normal_capacity_succeeds);
  RUN_TEST(test_arena_init_zero_capacity_coerces_to_one);
  RUN_TEST(test_arena_init_null_arena_returns_zero);
  RUN_TEST(test_arena_init_large_capacity_succeeds);
  RUN_TEST(test_arena_init_reinit_overwrites_previous_state);

  RUN_TEST(test_arena_alloc_basic_copy_and_null_terminates);
  RUN_TEST(test_arena_alloc_null_arena_returns_null);
  RUN_TEST(test_arena_alloc_null_src_returns_null);
  RUN_TEST(test_arena_alloc_zero_length_still_null_terminates);
  RUN_TEST(test_arena_alloc_exact_fit_succeeds);
  RUN_TEST(test_arena_alloc_one_byte_over_capacity_fails);
  RUN_TEST(test_arena_alloc_sequential_allocations_pack_contiguously);
  RUN_TEST(test_arena_alloc_returns_null_once_full_but_preserves_prior_data);
  RUN_TEST(test_arena_alloc_does_not_null_terminate_beyond_capacity);

  RUN_TEST(test_arena_free_resets_fields);
  RUN_TEST(test_arena_free_null_arena_does_not_crash);
  RUN_TEST(test_arena_free_double_free_is_safe);
  RUN_TEST(test_arena_alloc_after_free_fails_safely);

  return UNITY_END();
}