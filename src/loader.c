#include "../include/loader.h"
#include "../include/logger.h"
#include "../include/memory.h"
#include <stddef.h>
#include <stdlib.h>

/**
 * @brief Writes a memory segment's data into the page table at the given base address
 *
 * @param pt The page table to write into
 * @param base_addr The address to write the segment's data at
 * @param seg The memory segment to load
 * @param perms The permission mask to apply to the written pages
 * @return int 1 if successful (including when the segment is empty), 0 if the write failed
 */
static int load_mem_segment(PageTable* pt, uint32_t base_addr, MemorySegment* seg, PermissionMask perms) {
  if (!pt || !seg)
    return 0;
  if (seg->size == 0)
    return 1;
  return mem_write_raw(pt, base_addr, seg->data, seg->size, perms) == ACCESS_OK;
}

ProgramState* load_binary(AssembledProgram* prog) {
  if (!prog)
    return NULL;

  ProgramState* state = calloc(1, sizeof(ProgramState));
  if (!state)
    return NULL;

  const uint32_t stack_size_mb = 2u;
  const uint32_t stack_guard_pages = 1u;
  const uint32_t stack_top = 0x80000000u;
  const uint32_t stack_size = stack_size_mb * 1024u * 1024u;
  const uint32_t stack_guard_bytes = stack_guard_pages * PAGE_SIZE;
  const uint32_t stack_base = stack_top - stack_guard_bytes - stack_size;
  static const uint8_t zero_page[PAGE_SIZE] = {0};

  if (stack_base < stack_guard_bytes) {
    LOG_ERROR("Configured stack region underflows address space");
    goto fail;
  }

  uint32_t text_base = 0;
  uint32_t rodata_base = (text_base + prog->text.size + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
  uint32_t data_base = (rodata_base + prog->rodata.size + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
  uint32_t bss_base = (data_base + prog->data.size + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);

  if (!load_mem_segment(&state->pt, text_base, &prog->text, MEMORY_READ | MEMORY_EXECUTE)) {
    LOG_ERROR("Failed to load .text into memory");
    goto fail;
  }

  if (!load_mem_segment(&state->pt, rodata_base, &prog->rodata, MEMORY_READ)) {
    LOG_ERROR("Failed to load .rodata into memory");
    goto fail;
  }
  if (!load_mem_segment(&state->pt, data_base, &prog->data, MEMORY_READ | MEMORY_WRITE)) {
    LOG_ERROR("Failed to load .data into memory");
    goto fail;
  }
  if (!load_mem_segment(&state->pt, bss_base, &prog->bss, MEMORY_READ | MEMORY_WRITE)) {
    LOG_ERROR("Failed to load .bss into memory");
    goto fail;
  }

  state->heap_break = (bss_base + prog->bss.size + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
  state->stack_limit = stack_base;

  if (state->heap_break > stack_base) {
    LOG_ERROR("Program image overlaps configured stack region");
    goto fail;
  }

  for (uint32_t addr = stack_base; addr < stack_top; addr += PAGE_SIZE) {
    if (addr >= stack_base && addr < stack_base + stack_guard_bytes) {
      continue;
    }

    if (mem_write_raw(&state->pt, addr, zero_page, PAGE_SIZE, MEMORY_READ | MEMORY_WRITE) != ACCESS_OK) {
      LOG_ERROR("Failed to allocate stack region");
      goto fail;
    }
  }

  state->registers[2] = stack_top;

  state->pc = prog->entry_offset;

  return state;

fail:
  free_loaded_binary(state);
  return NULL;
}

void free_loaded_binary(ProgramState* state) {
  if (!state)
    return;
  free_page_table(&state->pt);
  free(state);
}