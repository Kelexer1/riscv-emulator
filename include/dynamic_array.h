#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Represents a single dynamic arrays data, element size, capacity, and
 * current element count
 */
typedef struct {
  void* data;
  size_t count;
  size_t capacity;
  size_t element_size;
} DynamicArray;

/**
 * @brief Initializes a dynamic array
 *
 * @param arr The dynamic array
 * @param element_size The size in bytes of one element
 * @param initial_capacity The initial count of elements to allocate size for
 * (small values may cause excess reallocations)
 * @return int 1 if success, 0 if the operation failed
 */
int dynamic_array_init(DynamicArray* arr, size_t element_size, size_t initial_capacity);

/**
 * @brief Returns a pointer to a new slot in the dynamic array, growing the
 * array if needed
 *
 * @param arr The dynamic array
 * @return void* A typeless pointer to the new slot, NULL if an error occurred
 */
void* dynamic_array_emplace(DynamicArray* arr);

/**
 * @brief Pushes a single element to a dynamic array, growing the array if
 * needed
 *
 * @param arr The dynamic array
 * @param elem A pointer to the element to be pushed (a copy is made)
 * @return int 1 if success, 0 if the operation failed
 */
int dynamic_array_push(DynamicArray* arr, const void* elem);

/**
 * @brief Deinitializes a dynamic array, including freeing its associated array
 *
 * @param arr The dynamic array
 */
void dynamic_array_free(DynamicArray* arr);

#endif