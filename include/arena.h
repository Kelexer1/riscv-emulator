#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

/**
 * @brief A fixed-capacity bump allocator; all allocations are freed together via arena_free
 */
typedef struct {
  char* data;
  size_t capacity;
  size_t used;
} Arena;

/**
 * @brief Initializes an arena with a fixed backing buffer of the given capacity
 *
 * @param arena The arena to initialize
 * @param capacity The capacity in bytes to allocate; if 0, a capacity of 1 is used instead
 * @return int 1 if successful, 0 if failure or error
 */
int arena_init(Arena* arena, size_t capacity);

/**
 * @brief Copies len bytes from src into the arena and null-terminates the copy
 *
 * The arena has fixed capacity and does not grow; the allocation fails if there is not enough
 * remaining space for len + 1 bytes
 *
 * @param arena The arena to allocate from
 * @param src The source buffer to copy
 * @param len The number of bytes to copy from src, excluding the null terminator
 * @return char* A pointer into the arena's buffer containing the null-terminated copy, or NULL if
 * allocation fails. This pointer is owned by the arena and must not be individually freed
 */
char* arena_alloc(Arena* arena, const char* src, size_t len);

/**
 * @brief Frees the arena's backing buffer, invalidating all pointers previously returned by arena_alloc
 *
 * @param arena The arena to free
 */
void arena_free(Arena* arena);

#endif