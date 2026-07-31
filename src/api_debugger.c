#include "../include/api_debugger.h"
#include "../include/api_disassembler.h"
#include "../include/helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void debugger_init(DebuggerState* dbg, ProgramState* prog, AssembledProgram* prog_source, SymbolTable* symbol_table) {
  if (!dbg || !prog || !prog_source)
    return;
  memset(dbg, 0, sizeof(DebuggerState));
  dbg->prog = prog;
  dbg->prog_source = prog_source;
  dbg->symbol_table = symbol_table;
}

int debugger_reset(DebuggerState* dbg) {
  if (!dbg)
    return 0;

  ProgramState* fresh = load_binary(dbg->prog_source);
  if (!fresh)
    return 0;

  free_loaded_binary(dbg->prog);
  dbg->prog = fresh;
  dbg->is_paused = 0;

  return 1;
}

int debugger_add_breakpoint(DebuggerState* dbg, uint32_t addr) {
  if (!dbg)
    return 0;
  if (debugger_find_breakpoint(dbg, addr) != -1)
    return 1;

  for (int i = 0; i < MAX_BREAKPOINTS; i++) {
    if (!dbg->breakpoints[i].set) {
      dbg->breakpoints[i].addr = addr;
      dbg->breakpoints[i].active = 1;
      dbg->breakpoints[i].set = 1;
      return 1;
    }
  }

  return 0;
}

int debugger_add_breakpoint_symbol(DebuggerState* dbg, const char* sym, size_t len) {
  if (!dbg || !dbg->symbol_table || !sym)
    return 0;

  Symbol s;
  if (!resolve_symbol(dbg->symbol_table, sym, len, &s))
    return 0;

  return debugger_add_breakpoint(dbg, s.value);
}

void debugger_remove_breakpoint(DebuggerState* dbg, uint32_t addr) {
  if (!dbg)
    return;

  int i = debugger_find_breakpoint(dbg, addr);
  if (i == -1)
    return;
  dbg->breakpoints[i].set = 0;
}

int debugger_toggle_breakpoint(DebuggerState* dbg, uint32_t addr, int active) {
  if (!dbg)
    return 0;

  int i = debugger_find_breakpoint(dbg, addr);
  if (i == -1)
    return 0;
  dbg->breakpoints[i].active = active;
  return 1;
}

int debugger_find_breakpoint(const DebuggerState* dbg, uint32_t addr) {
  if (!dbg)
    return -1;

  for (int i = 0; i < MAX_BREAKPOINTS; i++) {
    if (dbg->breakpoints[i].set && dbg->breakpoints[i].addr == addr)
      return i;
  }

  return -1;
}

int debugger_add_watchpoint(DebuggerState* dbg, uint32_t addr) {
  if (!dbg)
    return 0;
  if (debugger_find_watchpoint(dbg, addr) != -1)
    return 1;

  for (int i = 0; i < MAX_WATCHPOINTS; i++) {
    if (!dbg->watchpoints[i].set) {
      uint32_t initial_val = 0;
      MemoryAccessResult r = mem_read_u32(&dbg->prog->pt, addr, &initial_val);

      dbg->watchpoints[i].addr = addr;
      dbg->watchpoints[i].last_val = initial_val;
      dbg->watchpoints[i].active = 1;
      dbg->watchpoints[i].set = 1;

      return r == ACCESS_OK ? 1 : 2;
    }
  }

  return 0;
}

int debugger_add_watchpoint_symbol(DebuggerState* dbg, const char* sym, size_t len) {
  if (!dbg || !dbg->symbol_table || !sym)
    return 0;

  Symbol s;
  if (!resolve_symbol(dbg->symbol_table, sym, len, &s))
    return 0;

  return debugger_add_watchpoint(dbg, s.value);
}

void debugger_remove_watchpoint(DebuggerState* dbg, uint32_t addr) {
  if (!dbg)
    return;

  int i = debugger_find_watchpoint(dbg, addr);
  if (i == -1)
    return;
  dbg->watchpoints[i].set = 0;
}

int debugger_toggle_watchpoint(DebuggerState* dbg, uint32_t addr, int active) {
  if (!dbg)
    return 0;

  int i = debugger_find_watchpoint(dbg, addr);
  if (i == -1)
    return 0;
  dbg->watchpoints[i].active = active;
  return 1;
}

int debugger_find_watchpoint(const DebuggerState* dbg, uint32_t addr) {
  if (!dbg)
    return -1;

  for (int i = 0; i < MAX_WATCHPOINTS; i++) {
    if (dbg->watchpoints[i].set && dbg->watchpoints[i].addr == addr)
      return i;
  }

  return -1;
}

/**
 * @brief Checks all active watchpoints for a value change since their last recorded value
 *
 * @param dbg The debugger
 * @return int 1 if a watchpoint's value changed (its last_val is updated to the new value), 0 if
 * no change was detected
 */
static int check_watchpoints(DebuggerState* dbg) {
  for (int i = 0; i < MAX_WATCHPOINTS; i++) {
    Watchpoint* wp = &dbg->watchpoints[i];
    if (!wp->set || !wp->active)
      continue;

    uint32_t current;
    if (mem_read_u32(&dbg->prog->pt, wp->addr, &current) != ACCESS_OK)
      continue;

    if (current != wp->last_val) {
      wp->last_val = current;
      return 1;
    }
  }

  return 0;
}

/**
 * @brief Maps a raw step result to a debugger stop reason, recording the step as the debugger's
 * last step
 *
 * @param dbg The debugger
 * @param r The result of the step to classify
 * @return DebugStopReason The corresponding stop reason
 */
static DebugStopReason classify_step(DebuggerState* dbg, StepResult r) {
  if (!dbg)
    return STOP_FAULT;

  dbg->last_step = r;
  if (step_status_is_fault(r.status))
    return STOP_FAULT;
  if (r.status == STEP_EXITED)
    return STOP_EXITED;
  if (r.status == STEP_EBREAK)
    return STOP_EBREAK;
  return STOP_STEP;
}

DebugStopReason debugger_step(DebuggerState* dbg) {
  if (!dbg)
    return STOP_FAULT;

  StepResult r = cpu_step_single(dbg->prog);
  DebugStopReason reason = classify_step(dbg, r);

  if (reason == STOP_STEP && check_watchpoints(dbg))
    return STOP_WATCHPOINT;

  return reason;
}

DebugStopReason debugger_continue(DebuggerState* dbg) {
  if (!dbg)
    return STOP_FAULT;

  if (debugger_find_breakpoint(dbg, dbg->prog->pc) != -1) {
    DebugStopReason r = debugger_step(dbg);
    if (r != STOP_STEP)
      return r;
  }

  while (dbg->prog->status == CPU_RUNNING) {
    if (debugger_find_breakpoint(dbg, dbg->prog->pc) != -1)
      return STOP_BREAKPOINT;
    DebugStopReason reason = debugger_step(dbg);
    if (reason != STOP_STEP)
      return reason;
  }

  return STOP_EXITED;
}

/**
 * @brief Prints a single number to stdout in the given format
 *
 * @param num The number to print
 * @param fmt The number format to use
 */
static void print_num(uint32_t num, NumberFormat fmt) {
  switch (fmt) {
  case FORMAT_BIN:
    print_binary(num);
    break;
  case FORMAT_HEX:
    fprintf(stdout, "0x%08X", num);
    break;
  case FORMAT_DEC:
    fprintf(stdout, "%u", num);
    break;
  case FORMAT_ASCII:
    fprintf(stdout, "%c%c%c%c", num & 0xFF, (num >> 8) & 0xFF, (num >> 16) & 0xFF, (num >> 24) & 0xFF);
    break;
  }
}

void debugger_print_register(const DebuggerState* dbg, uint32_t reg_mask, NumberFormat fmt) {
  for (uint32_t i = 0; i < 32; i++) {
    if (!((1u << i) & reg_mask))
      continue;

    uint32_t val = dbg->prog->registers[i];
    fprintf(stdout, "x%d: ", i);
    print_num(val, fmt);
    fprintf(stdout, "\n");
  }
}

void debugger_print_memory(const DebuggerState* dbg, uint32_t addr, uint32_t count, uint32_t count_size,
                           NumberFormat fmt) {
  if (!dbg)
    return;
  if (count != 4 && count != 2 && count != 1)
    return;

  for (uint32_t i = 0; i < count; i++) {
    uint32_t offset = i * count_size;
    uint32_t curr_addr = addr + offset;
    uint32_t x = 0;
    MemoryAccessResult r;

    switch (count_size) {
    case 1: {
      uint8_t v;
      r = mem_read_u8(&dbg->prog->pt, curr_addr, &v);
      x = v;
      break;
    }
    case 2: {
      uint16_t v;
      r = mem_read_u16(&dbg->prog->pt, curr_addr, &v);
      x = v;
      break;
    }
    default: {
      r = mem_read_u32(&dbg->prog->pt, curr_addr, &x);
      break;
    }
    }

    fprintf(stdout, "0x%08X: ", curr_addr);
    if (r == ACCESS_OK) {
      print_num(x, fmt);
    } else {
      fprintf(stdout, "<unreadable>");
    }
    fprintf(stdout, "\n");
  }
}

void debugger_print_disas(const DebuggerState* dbg, uint32_t addr, uint32_t words, NumberFormat fmt) {
  if (!dbg)
    return;

  for (uint32_t i = 0; i < words; i++) {
    uint32_t curr_addr = addr + (i * 4);

    const char* pc_marker = (curr_addr == dbg->prog->pc) ? "=> " : "   ";
    const char* bp_marker = debugger_find_breakpoint(dbg, curr_addr) != -1 ? "*" : " ";

    fprintf(stdout, "%s%s0x%08X: ", pc_marker, bp_marker, curr_addr);

    uint32_t bin;
    if (mem_read_u32(&dbg->prog->pt, curr_addr, &bin) != ACCESS_OK) {
      fprintf(stdout, "<unreadable>\n");
      return;
    }

    DecodedInstruction ins = {0};
    if (!binary_to_instruction(bin, &ins)) {
      fprintf(stdout, "<invalid instruction: ");
      print_num(bin, fmt);
      fprintf(stdout, ">\n");
      return;
    }

    char* disas = disassemble_instruction(curr_addr, &ins, dbg->symbol_table);
    if (!disas) {
      fprintf(stdout, "<undecodable>\n");
      return;
    }

    fprintf(stdout, "%s\n", disas);
    free(disas);
  }
}