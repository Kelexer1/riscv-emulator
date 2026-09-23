#ifndef LOADER_H
#define LOADER_H

#include "elf32_load.h"
#include "memory.h"

#define MAX_ELF_FILE_SIZE (256u * 1024u * 1024u)

/**
 * @brief Encodes the run status of a loaded program
 */
typedef enum : uint8_t { CPU_RUNNING, CPU_PAUSED, CPU_EXITED, CPU_HALTED } CPUStatus;

/**
 * @brief Encodes the full runtime state of a loaded, running program
 */
typedef struct {
  PageTable pt;
  uint32_t pc;
  uint32_t registers[32];
  uint32_t heap_break;
  uint32_t stack_limit;
  CPUStatus status;
  int exit_code;
} ProgramState;

/**
 * @brief Loads a parsed ELF32 image into a fresh virtual address space and sets up the stack, heap break, and pc
 *
 * @param img The parsed ELF image; its segment data is copied, so the backing buffer may be freed afterward
 * @return ProgramState* The loaded program state, or NULL if loading failed
 */
ProgramState* load_binary(const Elf32Image* img);

/**
 * @brief Reads, parses, and loads an ELF32 file from disk
 *
 * @param path The path of the ELF file
 * @return ProgramState* The loaded program state, or NULL if the file could not be read, parsed, or loaded
 */
ProgramState* load_elf_file(const char* path);

/**
 * @brief Frees a loaded program's page table and the state struct itself
 *
 * @param state The program state to free
 */
void free_loaded_binary(ProgramState* state);

#endif