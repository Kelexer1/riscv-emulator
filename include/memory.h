#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096
#define PAGE_BITS 12
#define PT_INDEX_BITS 10
#define PT_SIZE 1024

#define PAGE_OFFSET(vaddr) ((vaddr) & (PAGE_SIZE - 1))
#define PT_L2_INDEX(vaddr) (((vaddr) >> PAGE_BITS) & (PT_SIZE - 1))
#define PT_L1_INDEX(vaddr) (((vaddr) >> (PAGE_BITS + PT_INDEX_BITS)) & (PT_SIZE - 1))

typedef uint8_t PermissionMask;

typedef enum : uint8_t { MEMORY_READ = 1 << 0, MEMORY_WRITE = 1 << 1, MEMORY_EXECUTE = 1 << 2 } MemoryPermission;

typedef enum : uint8_t {
  ACCESS_OK,
  ACCESS_INVALID_ADDRESS,
  ACCESS_NO_READ,
  ACCESS_NO_WRITE,
  ACCESS_NO_EXECUTE
} MemoryAccessResult;

typedef struct {
  uint8_t data[PAGE_SIZE];
  PermissionMask permissions;
} MemoryPage;

typedef struct {
  MemoryPage* pages[PT_SIZE];
} PageTableL2;

typedef struct {
  PageTableL2* l2[PT_SIZE];
} PageTable;

MemoryAccessResult mem_read_raw(PageTable* pt, uint32_t vaddr, void* out, size_t size);

MemoryAccessResult mem_read_u8(PageTable* pt, uint32_t vaddr, uint8_t* out);

MemoryAccessResult mem_read_u16(PageTable* pt, uint32_t vaddr, uint16_t* out);

MemoryAccessResult mem_read_u32(PageTable* pt, uint32_t vaddr, uint32_t* out);

MemoryAccessResult mem_write_raw(PageTable* pt, uint32_t vaddr, const void* src, size_t size, PermissionMask perms);

MemoryAccessResult mem_write_u8(PageTable* pt, uint32_t vaddr, uint8_t val, PermissionMask perms);

MemoryAccessResult mem_write_u16(PageTable* pt, uint32_t vaddr, uint16_t val, PermissionMask perms);

MemoryAccessResult mem_write_u32(PageTable* pt, uint32_t vaddr, uint32_t val, PermissionMask perms);

MemoryAccessResult mem_fetch_instruction(PageTable* pt, uint32_t vaddr, uint32_t* out);

const char* mem_access_result_to_str(MemoryAccessResult r);

void free_page_table(PageTable* pt);

#endif