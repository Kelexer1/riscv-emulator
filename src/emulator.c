#include "../include/emulator.h"
#include "../include/api_emulator.h"

int execute_program(ProgramState* prog) {
  if (!prog)
    return 0;

  while (prog->status == CPU_RUNNING) {
    StepResult r = cpu_step_single(prog);
    if (r.status == STEP_EXITED)
      return 1;
    if (r.status == STEP_EBREAK) {
      prog->status = CPU_HALTED;
      return 0;
    }
    if (step_status_is_fault(r.status))
      return 0;
  }

  return prog->status == CPU_EXITED;
}