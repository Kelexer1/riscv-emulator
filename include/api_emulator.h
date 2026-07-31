#ifndef API_EMULATOR_H
#define API_EMULATOR_H

#include "binary_to_instruction.h"
#include "loader.h"
#include <stdint.h>

/**
 * @brief Encodes the outcome of a single CPU step
 */
typedef enum : uint8_t {
  STEP_OK,
  STEP_BRANCH_TAKEN,
  STEP_ECALL_HANDLED,
  STEP_EBREAK,
  STEP_EXITED,
  STEP_HALTED_MEM_READ,
  STEP_HALTED_MEM_WRITE,
  STEP_HALTED_DECODE,
  STEP_HALTED_INVALID_OP,
  STEP_HALTED_ECALL_INVALID,
  STEP_HALTED_FETCH,
} StepStatus;

/**
 * @brief Encodes the full result of executing a single instruction
 */
typedef struct {
  StepStatus status;
  uint32_t pc_old;
  uint32_t pc_new;
  uint32_t raw;
  DecodedInstruction decoded;
} StepResult;

/**
 * @brief Fetches, decodes, and executes a single instruction, advancing the program state
 *
 * @param prog The state of the program to step
 * @return StepResult The result of the step, including the outcome status and decoded instruction
 */
StepResult cpu_step_single(ProgramState* prog);

/**
 * @brief Determines whether a step status represents a fault that halted the CPU
 *
 * @param s The step status to check
 * @return int 1 if the status is a fault, 0 otherwise
 */
int step_status_is_fault(StepStatus s);

#endif