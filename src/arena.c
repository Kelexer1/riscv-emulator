#include "../include/arena.h"
#include <stdlib.h>
#include <string.h>

int arena_init(Arena* arena, size_t capacity) {
  if (!arena)
    return 0;

  capacity = capacity > 0 ? capacity : 1;

  arena->data = malloc(capacity);
  if (!arena->data)
    return 0;
  arena->capacity = capacity;
  arena->used = 0;
  return 1;
}

char* arena_alloc(Arena* arena, const char* src, size_t len) {
  if (!arena || !src)
    return NULL;

  if (arena->used + len + 1 > arena->capacity)
    return NULL;
  char* dest = arena->data + arena->used;
  if (len > 0)
    memcpy(dest, src, len);
  dest[len] = '\0';
  arena->used += len + 1;
  return dest;
}

void arena_free(Arena* arena) {
  if (arena) {
    free(arena->data);
    arena->data = NULL;
    arena->capacity = 0;
    arena->used = 0;
  }
}