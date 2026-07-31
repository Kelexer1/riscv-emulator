#ifndef LOADER_H
#define LOADER_H

#include "memory.h"
#include "second_pass.h"

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
 * @brief Loads an assembled program into a fresh address space, laying out .text, .rodata,
 * .data, .bss, and a guarded stack region
 *
 * @param prog The assembled program to load
 * @return ProgramState* The initialized program state, or NULL if an error occurred
 *
 * @note An unmapped guard page is left directly below the stack, so stack overflow faults
 * instead of silently corrupting adjacent memory
 */
ProgramState* load_binary(AssembledProgram* prog);

/**
 * @brief Frees a loaded program's page table and the state struct itself
 *
 * @param state The program state to free
 */
void free_loaded_binary(ProgramState* state);

#endif