#include "../include/dynamic_array.h"
#include "unity/unity.h"
#include <stdint.h>
#include <string.h>

static DynamicArray arr;

typedef struct {
  int32_t a;
  char b;
  double c;
} PaddedStruct;

void setUp(void) { memset(&arr, 0, sizeof(DynamicArray)); }

void tearDown(void) { dynamic_array_free(&arr); }

/* ---------------------------------------------------------------------
 * dynamic_array_init
 * ------------------------------------------------------------------- */

void test_init_normal_capacity(void) {
  int result = dynamic_array_init(&arr, sizeof(int), 10);

  TEST_ASSERT_EQUAL_INT(1, result);
  TEST_ASSERT_NOT_NULL(arr.data);
  TEST_ASSERT_EQUAL_UINT(sizeof(int), arr.element_size);
  TEST_ASSERT_EQUAL_UINT(10, arr.capacity);
  TEST_ASSERT_EQUAL_UINT(0, arr.count);
}

void test_init_null_arr_fails(void) { TEST_ASSERT_EQUAL_INT(0, dynamic_array_init(NULL, sizeof(int), 10)); }

void test_init_zero_element_size_fails(void) { TEST_ASSERT_EQUAL_INT(0, dynamic_array_init(&arr, 0, 10)); }

void test_init_capacity_below_two_coerces_to_two(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 0));
  TEST_ASSERT_EQUAL_UINT(2, arr.capacity);
}

void test_init_capacity_one_coerces_to_two(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 1));
  TEST_ASSERT_EQUAL_UINT(2, arr.capacity);
}

void test_init_rejects_element_size_capacity_overflow(void) {
  size_t huge_capacity = ((size_t)1 << (sizeof(size_t) * 8 - 1)) + 1;
  int result = dynamic_array_init(&arr, 16, huge_capacity);

  TEST_ASSERT_EQUAL_INT(0, result);
}

/* ---------------------------------------------------------------------
 * dynamic_array_emplace
 * ------------------------------------------------------------------- */

void test_emplace_null_arr_returns_null(void) { TEST_ASSERT_NULL(dynamic_array_emplace(NULL)); }

void test_emplace_returns_slot_and_increments_count(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 4));

  void* slot = dynamic_array_emplace(&arr);

  TEST_ASSERT_NOT_NULL(slot);
  TEST_ASSERT_EQUAL_UINT(1, arr.count);
  TEST_ASSERT_EQUAL_PTR(arr.data, slot);
}

void test_emplace_sequential_slots_are_contiguous(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 4));

  void* first = dynamic_array_emplace(&arr);
  void* second = dynamic_array_emplace(&arr);

  TEST_ASSERT_EQUAL_PTR((char*)first + sizeof(int), second);
}

void test_emplace_grows_array_when_full(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 2));

  dynamic_array_emplace(&arr);
  dynamic_array_emplace(&arr);
  TEST_ASSERT_EQUAL_UINT(2, arr.capacity);

  void* third = dynamic_array_emplace(&arr);

  TEST_ASSERT_NOT_NULL(third);
  TEST_ASSERT_EQUAL_UINT(4, arr.capacity);
  TEST_ASSERT_EQUAL_UINT(3, arr.count);
}

void test_emplace_growth_preserves_existing_elements(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 2));

  for (int i = 0; i < 10; i++) {
    int* slot = dynamic_array_emplace(&arr);
    TEST_ASSERT_NOT_NULL(slot);
    *slot = i;
  }

  int* data = (int*)arr.data;
  for (int i = 0; i < 10; i++) {
    TEST_ASSERT_EQUAL_INT(i, data[i]);
  }
  TEST_ASSERT_EQUAL_UINT(10, arr.count);
  TEST_ASSERT_TRUE(arr.capacity >= 10);
}

/* ---------------------------------------------------------------------
 * dynamic_array_push
 * ------------------------------------------------------------------- */

void test_push_null_arr_fails(void) {
  int val = 5;
  TEST_ASSERT_EQUAL_INT(0, dynamic_array_push(NULL, &val));
}

void test_push_null_elem_fails(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 4));
  TEST_ASSERT_EQUAL_INT(0, dynamic_array_push(&arr, NULL));
}

void test_push_copies_value_not_pointer(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 4));

  int val = 42;
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_push(&arr, &val));
  val = 99; /* mutate source after push */

  int* data = (int*)arr.data;
  TEST_ASSERT_EQUAL_INT(42, data[0]);
}

void test_push_multiple_elements_preserves_order(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 2));

  for (int i = 0; i < 6; i++) {
    TEST_ASSERT_EQUAL_INT(1, dynamic_array_push(&arr, &i));
  }

  int* data = (int*)arr.data;
  for (int i = 0; i < 6; i++) {
    TEST_ASSERT_EQUAL_INT(i, data[i]);
  }
}

void test_push_struct_with_padding(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(PaddedStruct), 2));

  PaddedStruct s = {.a = 1, .b = 'x', .c = 3.14};
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_push(&arr, &s));

  PaddedStruct* data = (PaddedStruct*)arr.data;
  TEST_ASSERT_EQUAL_INT32(1, data[0].a);
  TEST_ASSERT_EQUAL_CHAR('x', data[0].b);
  TEST_ASSERT_TRUE(data[0].c > 3.139 &&
                   data[0].c < 3.141); /* Unity double-precision asserts are disabled in this build */
}

/* ---------------------------------------------------------------------
 * dynamic_array_free
 * ------------------------------------------------------------------- */

void test_free_resets_fields(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 4));
  int val = 1;
  dynamic_array_push(&arr, &val);

  dynamic_array_free(&arr);

  TEST_ASSERT_NULL(arr.data);
  TEST_ASSERT_EQUAL_UINT(0, arr.count);
  TEST_ASSERT_EQUAL_UINT(0, arr.capacity);
}

void test_free_null_arr_does_not_crash(void) {
  dynamic_array_free(NULL);
  TEST_PASS();
}

void test_free_double_free_is_safe(void) {
  TEST_ASSERT_EQUAL_INT(1, dynamic_array_init(&arr, sizeof(int), 4));

  dynamic_array_free(&arr);
  dynamic_array_free(&arr);

  TEST_ASSERT_NULL(arr.data);
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_init_normal_capacity);
  RUN_TEST(test_init_null_arr_fails);
  RUN_TEST(test_init_zero_element_size_fails);
  RUN_TEST(test_init_capacity_below_two_coerces_to_two);
  RUN_TEST(test_init_capacity_one_coerces_to_two);
  RUN_TEST(test_init_rejects_element_size_capacity_overflow);

  RUN_TEST(test_emplace_null_arr_returns_null);
  RUN_TEST(test_emplace_returns_slot_and_increments_count);
  RUN_TEST(test_emplace_sequential_slots_are_contiguous);
  RUN_TEST(test_emplace_grows_array_when_full);
  RUN_TEST(test_emplace_growth_preserves_existing_elements);

  RUN_TEST(test_push_null_arr_fails);
  RUN_TEST(test_push_null_elem_fails);
  RUN_TEST(test_push_copies_value_not_pointer);
  RUN_TEST(test_push_multiple_elements_preserves_order);
  RUN_TEST(test_push_struct_with_padding);

  RUN_TEST(test_free_resets_fields);
  RUN_TEST(test_free_null_arr_does_not_crash);
  RUN_TEST(test_free_double_free_is_safe);

  return UNITY_END();
}