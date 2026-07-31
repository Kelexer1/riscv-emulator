#ifndef EMULATOR_H
#define EMULATOR_H

#include "loader.h"

/**
 * @brief Runs a loaded program to completion by repeatedly single-stepping the CPU
 *
 * Execution stops on exit, EBREAK, or a fault; on EBREAK the program is halted rather than exited
 *
 * @param prog The state of the program to run
 * @return int 1 if the program exited normally, 0 if it halted due to EBREAK, a fault, or invalid input
 */
int execute_program(ProgramState* prog);

#endif