#ifndef DEBUGGER_CLI_H
#define DEBUGGER_CLI_H

#include "api_debugger.h"

/**
 * @brief Runs an interactive read-eval-print loop for the debugger on stdin/stdout
 *
 * Reads commands such as step, continue, break, watch, and disas until the user quits or exits;
 * an empty line repeats the last executed command
 *
 * @param dbg The debugger
 */
void debugger_cli(DebuggerState* dbg);

#endif