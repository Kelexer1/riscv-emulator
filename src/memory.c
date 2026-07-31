#include "../include/memory.h"
#include "../include/logger.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static MemoryPage* page_lookup(PageTable* pt, uint32_t vaddr) {
  if (!pt)
    return NULL;

  uint32_t l1 = PT_L1_INDEX(vaddr);
  uint32_t l2 = PT_L2_INDEX(vaddr);
  if (!pt->l2[l1])
    return NULL;
  return pt->l2[l1]->pages[l2];
}

MemoryPage* page_alloc(PageTable* pt, uint32_t vaddr, PermissionMask perms) {
  if (!pt)
    return NULL;

  uint32_t l1 = PT_L1_INDEX(vaddr);
  uint32_t l2 = PT_L2_INDEX(vaddr);

  if (!pt->l2[l1]) {
    pt->l2[l1] = calloc(1, sizeof(PageTableL2));
    if (!pt->l2[l1])
      return NULL;
  }

  if (!pt->l2[l1]->pages[l2]) {
    pt->l2[l1]->pages[l2] = calloc(1, sizeof(MemoryPage));
    if (!pt->l2[l1]->pages[l2])
      return NULL;
    pt->l2[l1]->pages[l2]->permissions = perms;
  }

  return pt->l2[l1]->pages[l2];
}

static MemoryAccessResult page_resolve(PageTable* pt, uint32_t vaddr, uint8_t** out) {
  if (!pt || !out)
    return ACCESS_INVALID_ADDRESS;

  MemoryPage* page = page_lookup(pt, vaddr);
  if (!page)
    return ACCESS_INVALID_ADDRESS;
  *out = &page->data[PAGE_OFFSET(vaddr)];
  return ACCESS_OK;
}

static MemoryAccessResult page_resolve_alloc(PageTable* pt, uint32_t vaddr, uint8_t** out, PermissionMask perms) {
  if (!pt || !out)
    return ACCESS_INVALID_ADDRESS;

  MemoryPage* page = page_lookup(pt, vaddr);
  if (!page) {
    page = page_alloc(pt, vaddr, perms);
    if (!page)
      return ACCESS_INVALID_ADDRESS;
  }
  *out = &page->data[PAGE_OFFSET(vaddr)];
  return ACCESS_OK;
}

MemoryAccessResult mem_read_raw(PageTable* pt, uint32_t vaddr, void* out, size_t size) {
  if (!pt || !out)
    return ACCESS_INVALID_ADDRESS;

  if (PAGE_OFFSET(vaddr) + size <= PAGE_SIZE) {
    uint8_t* ptr;
    MemoryAccessResult r = page_resolve(pt, vaddr, &ptr);
    if (r != ACCESS_OK)
      return r;
    memcpy(out, ptr, size);
    return ACCESS_OK;
  }

  for (size_t i = 0; i < size; i++) {
    uint8_t* ptr;
    MemoryAccessResult r = page_resolve(pt, vaddr + i, &ptr);
    if (r != ACCESS_OK)
      return r;
    ((uint8_t*)out)[i] = *ptr;
  }

  return ACCESS_OK;
}

MemoryAccessResult mem_read_u8(PageTable* pt, uint32_t vaddr, uint8_t* out) {
  if (!pt || !out)
    return ACCESS_INVALID_ADDRESS;

  MemoryPage* page = page_lookup(pt, vaddr);
  if (!page)
    return ACCESS_INVALID_ADDRESS;
  if (!(page->permissions & MEMORY_READ))
    return ACCESS_NO_READ;
  return mem_read_raw(pt, vaddr, out, sizeof(uint8_t));
}

MemoryAccessResult mem_read_u16(PageTable* pt, uint32_t vaddr, uint16_t* out) {
  if (!pt || !out)
    return ACCESS_INVALID_ADDRESS;

  MemoryPage* page = page_lookup(pt, vaddr);
  if (!page)
    return ACCESS_INVALID_ADDRESS;
  if (!(page->permissions & MEMORY_READ))
    return ACCESS_NO_READ;
  return mem_read_raw(pt, vaddr, out, sizeof(uint16_t));
}

MemoryAccessResult mem_read_u32(PageTable* pt, uint32_t vaddr, uint32_t* out) {
  if (!pt || !out)
    return ACCESS_INVALID_ADDRESS;

  MemoryPage* page = page_lookup(pt, vaddr);
  if (!page)
    return ACCESS_INVALID_ADDRESS;
  if (!(page->permissions & MEMORY_READ))
    return ACCESS_NO_READ;
  return mem_read_raw(pt, vaddr, out, sizeof(uint32_t));
}

MemoryAccessResult mem_write_raw(PageTable* pt, uint32_t vaddr, const void* src, size_t size, PermissionMask perms) {
  if (!pt || !src)
    return ACCESS_INVALID_ADDRESS;

  size_t written = 0;
  while (written < size) {
    uint32_t current_addr = vaddr + (uint32_t)written;
    size_t page_offset = PAGE_OFFSET(current_addr);
    size_t chunk = PAGE_SIZE - page_offset;
    if (chunk > size - written)
      chunk = size - written;

    uint8_t* ptr;
    MemoryAccessResult r = page_resolve_alloc(pt, current_addr, &ptr, perms);
    if (r != ACCESS_OK) {
      LOG_ERROR_ADDRESS("Failed to write memory (%s, %zu bytes)", current_addr, mem_access_result_to_str(r), size);
      return r;
    }
    memcpy(ptr, (const uint8_t*)src + written, chunk);
    written += chunk;
  }

  return ACCESS_OK;
}

MemoryAccessResult mem_write_u8(PageTable* pt, uint32_t vaddr, uint8_t val, PermissionMask perms) {
  if (!pt)
    return ACCESS_INVALID_ADDRESS;

  MemoryPage* page = page_lookup(pt, vaddr);
  if (page && !(page->permissions & MEMORY_WRITE)) {
    LOG_ERROR_ADDRESS("Failed to write memory (%s)", vaddr, mem_access_result_to_str(ACCESS_NO_WRITE));
    return ACCESS_NO_WRITE;
  }
  return mem_write_raw(pt, vaddr, &val, sizeof(uint8_t), perms);
}

MemoryAccessResult mem_write_u16(PageTable* pt, uint32_t vaddr, uint16_t val, PermissionMask perms) {
  if (!pt)
    return ACCESS_INVALID_ADDRESS;

  MemoryPage* page = page_lookup(pt, vaddr);
  if (page && !(page->permissions & MEMORY_WRITE)) {
    LOG_ERROR_ADDRESS("Failed to write memory (%s)", vaddr, mem_access_result_to_str(ACCESS_NO_WRITE));
    return ACCESS_NO_WRITE;
  }
  return mem_write_raw(pt, vaddr, &val, sizeof(uint16_t), perms);
}

MemoryAccessResult mem_write_u32(PageTable* pt, uint32_t vaddr, uint32_t val, PermissionMask perms) {
  if (!pt)
    return ACCESS_INVALID_ADDRESS;
  MemoryPage* page = page_lookup(pt, vaddr);
  if (page && !(page->permissions & MEMORY_WRITE)) {
    LOG_ERROR_ADDRESS("Failed to write memory (%s)", vaddr, mem_access_result_to_str(ACCESS_NO_WRITE));
    return ACCESS_NO_WRITE;
  }
  return mem_write_raw(pt, vaddr, &val, sizeof(uint32_t), perms);
}

MemoryAccessResult mem_fetch_instruction(PageTable* pt, uint32_t vaddr, uint32_t* out) {
  if (!pt || !out)
    return ACCESS_INVALID_ADDRESS;

  MemoryPage* page = page_lookup(pt, vaddr);
  if (!page)
    return ACCESS_INVALID_ADDRESS;
  if (!(page->permissions & MEMORY_EXECUTE))
    return ACCESS_NO_EXECUTE;
  return mem_read_raw(pt, vaddr, out, sizeof(uint32_t));
}

const char* mem_access_result_to_str(MemoryAccessResult r) {
  switch (r) {
  case ACCESS_OK:
    return "Access OK";
  case ACCESS_INVALID_ADDRESS:
    return "Invalid address accessed";
  case ACCESS_NO_READ:
    return "Address does not have read permissions";
  case ACCESS_NO_WRITE:
    return "Address does not have write permissions";
  case ACCESS_NO_EXECUTE:
    return "Address does not have execute permissions";
  }

  return "";
}

void free_page_table(PageTable* pt) {
  if (!pt)
    return;

  for (int i = 0; i < PT_SIZE; i++) {
    if (!pt->l2[i])
      continue;
    for (int j = 0; j < PT_SIZE; j++) {
      free(pt->l2[i]->pages[j]);
    }
    free(pt->l2[i]);
  }
}