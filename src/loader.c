#include "../include/loader.h"
#include "../include/elf32.h"
#include "../include/logger.h"
#include "../include/memory.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t page_align_up(uint64_t v) { return (v + PAGE_SIZE - 1u) & ~(uint64_t)(PAGE_SIZE - 1u); }

static PermissionMask perms_from_flags(uint32_t flags) {
  PermissionMask perms = 0;
  if (flags & PF_R)
    perms |= MEMORY_READ;
  if (flags & PF_W)
    perms |= MEMORY_WRITE;
  if (flags & PF_X)
    perms |= MEMORY_EXECUTE;
  return perms;
}

/**
 * @brief Writes len zero bytes into the page table starting at addr
 */
static int zero_fill(PageTable* pt, uint32_t addr, uint32_t len, PermissionMask perms) {
  static const uint8_t zeros[PAGE_SIZE] = {0};
  while (len > 0) {
    uint32_t n = len < PAGE_SIZE ? len : PAGE_SIZE;
    if (mem_write_raw(pt, addr, zeros, n, perms) != ACCESS_OK)
      return 0;
    addr += n;
    len -= n;
  }
  return 1;
}

/**
 * @brief Returns 1 if any two loadable segments occupy the same page
 */
static int segments_share_page(const Elf32Image* img) {
  for (size_t i = 0; i < img->nsegments; i++) {
    const Elf32LoadSegment* a = &img->segments[i];
    if (a->memsz == 0)
      continue;
    uint64_t a_start = a->vaddr & ~(uint64_t)(PAGE_SIZE - 1u);
    uint64_t a_end = page_align_up((uint64_t)a->vaddr + a->memsz);
    for (size_t j = i + 1; j < img->nsegments; j++) {
      const Elf32LoadSegment* b = &img->segments[j];
      if (b->memsz == 0)
        continue;
      uint64_t b_start = b->vaddr & ~(uint64_t)(PAGE_SIZE - 1u);
      uint64_t b_end = page_align_up((uint64_t)b->vaddr + b->memsz);
      if (a_start < b_end && b_start < a_end)
        return 1;
    }
  }
  return 0;
}

/**
 * @brief Returns 1 if the entry point lies inside an executable segment
 */
static int entry_is_executable(const Elf32Image* img) {
  for (size_t i = 0; i < img->nsegments; i++) {
    const Elf32LoadSegment* seg = &img->segments[i];
    if ((seg->flags & PF_X) && img->entry >= seg->vaddr && img->entry - seg->vaddr < seg->memsz)
      return 1;
  }
  return 0;
}

ProgramState* load_binary(const Elf32Image* img) {
  if (!img)
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

  if (segments_share_page(img)) {
    LOG_ERROR("ELF segments share a page; per-page permissions would conflict");
    goto fail;
  }

  if (!entry_is_executable(img)) {
    LOG_ERROR("ELF entry point is not inside an executable segment");
    goto fail;
  }

  uint64_t image_end = 0;
  for (size_t i = 0; i < img->nsegments; i++) {
    const Elf32LoadSegment* seg = &img->segments[i];
    if (seg->memsz == 0)
      continue;

    PermissionMask perms = perms_from_flags(seg->flags);
    if (seg->filesz > 0 && mem_write_raw(&state->pt, seg->vaddr, seg->data, seg->filesz, perms) != ACCESS_OK) {
      LOG_ERROR("Failed to load ELF segment contents into memory");
      goto fail;
    }
    if (!zero_fill(&state->pt, seg->vaddr + seg->filesz, seg->memsz - seg->filesz, perms)) {
      LOG_ERROR("Failed to zero-fill ELF segment tail");
      goto fail;
    }

    uint64_t end = page_align_up((uint64_t)seg->vaddr + seg->memsz);
    if (end > image_end)
      image_end = end;
  }

  if (image_end > stack_base) {
    LOG_ERROR("Program image overlaps configured stack region");
    goto fail;
  }

  state->heap_break = (uint32_t)image_end;
  state->stack_limit = stack_base;

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

  state->pc = img->entry;

  return state;

fail:
  free_loaded_binary(state);
  return NULL;
}

ProgramState* load_elf_file(const char* path) {
  if (!path)
    return NULL;

  FILE* f = fopen(path, "rb");
  if (!f) {
    LOG_ERROR("Failed to open ELF file '%s'", path);
    return NULL;
  }

  ProgramState* state = NULL;
  uint8_t* buf = NULL;

  if (fseek(f, 0, SEEK_END) != 0)
    goto done;
  long size = ftell(f);
  if (size <= 0 || (unsigned long)size > MAX_ELF_FILE_SIZE) {
    LOG_ERROR("ELF file '%s' has an invalid size", path);
    goto done;
  }
  rewind(f);

  buf = malloc((size_t)size);
  if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
    LOG_ERROR("Failed to read ELF file '%s'", path);
    goto done;
  }

  Elf32Image img;
  Elf32Status status = elf32_parse(buf, (size_t)size, &img);
  if (status != ELF32_OK) {
    LOG_ERROR("Invalid ELF file '%s': %s", path, elf32_status_str(status));
    goto done;
  }

  state = load_binary(&img);

done:
  free(buf);
  fclose(f);
  return state;
}

void free_loaded_binary(ProgramState* state) {
  if (!state)
    return;
  free_page_table(&state->pt);
  free(state);
}