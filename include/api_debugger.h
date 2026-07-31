#ifndef API_DEBUGGER_H
#define API_DEBUGGER_H

#include "api_emulator.h"
#include "first_pass.h"
#include "loader.h"
#include "second_pass.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief The maximum number of breakpoints the user can define before an error is thrown due to no more space
 */
#define MAX_BREAKPOINTS 128

/**
 * @brief The maximum number of watchpoints a user can define before an error is thrown due to no more space
 */
#define MAX_WATCHPOINTS 64

/**
 * @brief Encodes the reason that a debugger has paused code execution
 */
typedef enum : uint8_t {
  STOP_BREAKPOINT,
  STOP_WATCHPOINT,
  STOP_EBREAK,
  STOP_EXITED,
  STOP_FAULT,
  STOP_STEP,
} DebugStopReason;

/**
 * @brief Encodes a single breakpoint
 */
typedef struct {
  uint32_t addr;
  int active;
  int set;
} Breakpoint;

/**
 * @brief Encodes a single watchpoint
 */
typedef struct {
  uint32_t addr;
  uint32_t last_val;
  int active;
  int set;
} Watchpoint;

/**
 * @brief Encodes all the state required for a debugger
 */
typedef struct {
  ProgramState* prog;
  AssembledProgram* prog_source;
  SymbolTable* symbol_table;
  Breakpoint breakpoints[MAX_BREAKPOINTS];
  Watchpoint watchpoints[MAX_WATCHPOINTS];
  StepResult last_step;
  int is_paused;
} DebuggerState;

/**
 * @brief Encodes a number format, for tasks such as printing a value
 */
typedef enum : uint8_t {
  FORMAT_BIN,
  FORMAT_HEX,
  FORMAT_DEC,
  FORMAT_ASCII,
} NumberFormat;

/**
 * @brief Initializes a debugger struct
 *
 * @param dbg The debugger struct to be initalized
 * @param prog The state of the program
 * @param prog_source The assembled source code of the program
 * @param symbol_table The symbol table, for tasks such as setting watchpoints and disassembly
 */
void debugger_init(DebuggerState* dbg, ProgramState* prog, AssembledProgram* prog_source, SymbolTable* symbol_table);

/**
 * @brief Reloads the assembled binary into the address space of the debuggers program, freeing the old binary
 *
 * @param dbg The debugger
 * @return int 1 if successful, 0 if failure or error
 */
int debugger_reset(DebuggerState* dbg);

/**
 * @brief Appends a breakpoint to a debuggers breakpoint array by value
 *
 * @param dbg The debugger
 * @param addr The address of the breakpoint
 * @return int 1 if successful, 0 if failure or error
 */
int debugger_add_breakpoint(DebuggerState* dbg, uint32_t addr);

/**
 * @brief Appends a breakpoint to a debuggers breakpoint array by symbol name
 *
 * @param dbg The debugger
 * @param sym The symbol as a character array
 * @param len The length of the character array
 * @return int 1 if successful, 0 if failure or error
 */
int debugger_add_breakpoint_symbol(DebuggerState* dbg, const char* sym, size_t len);

/**
 * @brief Removes a breakpoint from a debuggers breakpoint array by value
 *
 * @param dbg The debugger
 * @param addr The address of the breakpoint
 */
void debugger_remove_breakpoint(DebuggerState* dbg, uint32_t addr);

/**
 * @brief Sets whether a breakpoint is active without removing it from the debugger
 *
 * @param dbg The debugger
 * @param addr The address of the breakpoint
 * @param active Whether the breakpoint should be active
 * @return int 1 if successful, 0 if failure or error
 */
int debugger_toggle_breakpoint(DebuggerState* dbg, uint32_t addr, int active);

/**
 * @brief Finds the index of a breakpoint in a debuggers breakpoint array by address
 *
 * @param dbg The debugger
 * @param addr The address of the breakpoint
 * @return int The index of the breakpoint if found, -1 otherwise
 */
int debugger_find_breakpoint(const DebuggerState* dbg, uint32_t addr);

/**
 * @brief Appends a watchpoint to a debuggers watchpoint array by value
 *
 * @param dbg The debugger
 * @param addr The address of the watchpoint
 * @return int 1 if successful, 2 if added but the initial value could not be read, 0 if failure or error
 */
int debugger_add_watchpoint(DebuggerState* dbg, uint32_t addr);

/**
 * @brief Appends a watchpoint to a debuggers watchpoint array by symbol name
 *
 * @param dbg The debugger
 * @param sym The symbol as a character array
 * @param len The length of the character array
 * @return int 1 if successful, 2 if added but the initial value could not be read, 0 if failure or error
 */
int debugger_add_watchpoint_symbol(DebuggerState* dbg, const char* sym, size_t len);

/**
 * @brief Removes a watchpoint from a debuggers watchpoint array by value
 *
 * @param dbg The debugger
 * @param addr The address of the watchpoint
 */
void debugger_remove_watchpoint(DebuggerState* dbg, uint32_t addr);

/**
 * @brief Sets whether a watchpoint is active without removing it from the debugger
 *
 * @param dbg The debugger
 * @param addr The address of the watchpoint
 * @param active Whether the watchpoint should be active
 * @return int 1 if successful, 0 if failure or error
 */
int debugger_toggle_watchpoint(DebuggerState* dbg, uint32_t addr, int active);

/**
 * @brief Finds the index of a watchpoint in a debuggers watchpoint array by address
 *
 * @param dbg The debugger
 * @param addr The address of the watchpoint
 * @return int The index of the watchpoint if found, -1 otherwise
 */
int debugger_find_watchpoint(const DebuggerState* dbg, uint32_t addr);

/**
 * @brief Executes a single instruction and checks for triggered breakpoints or watchpoints
 *
 * @param dbg The debugger
 * @return DebugStopReason The reason execution stopped
 */
DebugStopReason debugger_step(DebuggerState* dbg);

/**
 * @brief Runs the program until a breakpoint, watchpoint, fault, or exit is encountered
 *
 * If execution is currently paused on a breakpoint, that breakpoint is stepped past before resuming
 *
 * @param dbg The debugger
 * @return DebugStopReason The reason execution stopped
 */
DebugStopReason debugger_continue(DebuggerState* dbg);

/**
 * @brief Prints the value of each register selected by a bitmask to stdout
 *
 * @param dbg The debugger
 * @param reg_mask Bitmask of registers to print, where bit i corresponds to register xi
 * @param fmt The number format to print values in
 */
void debugger_print_register(const DebuggerState* dbg, uint32_t reg_mask, NumberFormat fmt);

/**
 * @brief Prints count values of size count_size read from memory starting at addr to stdout
 *
 * @param dbg The debugger
 * @param addr The starting address to read from
 * @param count The number of values to print
 * @param count_size The size in bytes of each value; must be 1, 2, or 4
 * @param fmt The number format to print values in
 */
void debugger_print_memory(const DebuggerState* dbg, uint32_t addr, uint32_t count, uint32_t count_size,
                           NumberFormat fmt);

/**
 * @brief Prints words disassembled instructions starting at addr to stdout, marking the current pc and breakpoints
 *
 * @param dbg The debugger
 * @param addr The starting address to disassemble from
 * @param words The number of instructions to disassemble
 * @param fmt The number format used for undecodable instruction bytes
 */
void debugger_print_disas(const DebuggerState* dbg, uint32_t addr, uint32_t words, NumberFormat fmt);

#endif