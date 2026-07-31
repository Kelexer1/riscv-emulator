#include "../include/dynamic_array.h"
#include <stdlib.h>
#include <string.h>

int dynamic_array_init(DynamicArray* arr, size_t element_size, size_t initial_capacity) {
  if (!arr || !element_size)
    return 0;

  if (initial_capacity != 0 && element_size > SIZE_MAX / initial_capacity)
    return 0;

  arr->element_size = element_size;
  arr->count = 0;
  arr->capacity = initial_capacity < 2 ? 2 : initial_capacity;
  arr->data = calloc(1, arr->element_size * arr->capacity);
  return arr->data != NULL;
}

int dynamic_array_push(DynamicArray* arr, const void* elem) {
  if (!arr || !elem)
    return 0;

  void* slot = dynamic_array_emplace(arr);
  if (!slot)
    return 0;
  memcpy(slot, elem, arr->element_size);
  return 1;
}

void* dynamic_array_emplace(DynamicArray* arr) {
  if (!arr)
    return NULL;

  if (arr->count >= arr->capacity) {
    size_t new_capacity = arr->capacity * 2;
    void* tmp = realloc(arr->data, arr->element_size * new_capacity);
    if (!tmp)
      return 0;
    arr->data = tmp;
    arr->capacity = new_capacity;
  }

  void* slot = (char*)arr->data + arr->count * arr->element_size;
  arr->count++;
  return slot;
}

void dynamic_array_free(DynamicArray* arr) {
  if (!arr)
    return;
  free(arr->data);
  arr->data = NULL;
  arr->count = 0;
  arr->capacity = 0;
}