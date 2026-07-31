#include "../include/debugger_cli.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKENS 16
#define MAX_INPUT_LENGTH 512

typedef int (*cmd_handler)(DebuggerState* dbg, int argc, char** argv);

typedef struct {
  const char** names;
  cmd_handler handler;
  const char* usage;
  const char* help;
} DebugCommand;

/**
 * @brief Encodes the result of dispatching a single debugger command
 */
typedef enum : uint8_t { CMD_OK, CMD_ERROR, CMD_QUIT } CommandResult;

static const char* STEP_NAMES[] = {"step", "s", NULL};
static const char* CONTINUE_NAMES[] = {"continue", "c", NULL};
static const char* BREAK_NAMES[] = {"break", "b", NULL};
static const char* DELETE_NAMES[] = {"delete", "d", NULL};
static const char* WATCH_NAMES[] = {"watch", "w", NULL};
static const char* INFO_NAMES[] = {"info", "i", NULL};
static const char* EXAMINE_NAMES[] = {"x", NULL};
static const char* DISAS_NAMES[] = {"disas", "disassemble", NULL};
static const char* RUN_NAMES[] = {"run", "r", NULL};
static const char* QUIT_NAMES[] = {"quit", "q", "exit", NULL};
static const char* HELP_NAMES[] = {"help", "h", "?", NULL};

static int cmd_step(DebuggerState* dbg, int argc, char** argv);
static int cmd_continue(DebuggerState* dbg, int argc, char** argv);
static int cmd_break(DebuggerState* dbg, int argc, char** argv);
static int cmd_delete(DebuggerState* dbg, int argc, char** argv);
static int cmd_watch(DebuggerState* dbg, int argc, char** argv);
static int cmd_info(DebuggerState* dbg, int argc, char** argv);
static int cmd_examine(DebuggerState* dbg, int argc, char** argv);
static int cmd_disas(DebuggerState* dbg, int argc, char** argv);
static int cmd_run(DebuggerState* dbg, int argc, char** argv);
static int cmd_quit(DebuggerState* dbg, int argc, char** argv);
static int cmd_help(DebuggerState* dbg, int argc, char** argv);

static const DebugCommand COMMANDS[] = {
    {STEP_NAMES, cmd_step, "step", "Execute one instruction"},
    {CONTINUE_NAMES, cmd_continue, "continue", "Run until breakpoint/exit/fault"},
    {BREAK_NAMES, cmd_break, "break <addr|symbol>", "Add a breakpoint at an address/symbol"},
    {DELETE_NAMES, cmd_delete, "delete <addr|symbol>", "Remove a breakpoint/watchpoint at an address/symbol"},
    {WATCH_NAMES, cmd_watch, "watch <addr|symbol>", "Add a watchpoint at an address/symbol"},
    {INFO_NAMES, cmd_info, "info <registers|breakpoints|watchpoints>", "Print information about various states"},
    {EXAMINE_NAMES, cmd_examine, "x <addr|symbol> <count>",
     "Print a number of words at the memory location of an address/symbol"},
    {DISAS_NAMES, cmd_disas, "disas <addr> [count=10]",
     "Print disassembly for a number of instructions starting at an address/symbol"},
    {RUN_NAMES, cmd_run, "run", "Reset and run the program"},
    {QUIT_NAMES, cmd_quit, "exit", "Exit the debugger"},
    {HELP_NAMES, cmd_help, "help", "Information about supported commands"},
};

/**
 * @brief Finds the command matching a given name token
 *
 * @param token The command name or alias to look up
 * @return const DebugCommand* The matching command, NULL if no command matches
 */
static const DebugCommand* find_command(const char* token) {
  if (!token)
    return NULL;

  for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++) {
    for (const char** name = COMMANDS[i].names; *name != NULL; name++) {
      if (strcmp(*name, token) == 0)
        return &COMMANDS[i];
    }
  }

  return NULL;
}

/**
 * @brief Splits a line into whitespace-separated tokens in place, null-terminating each
 *
 * @param line The line to tokenize; mutated in place
 * @param tokens Where to write pointers to each token
 * @param max_tokens The maximum number of tokens to extract
 * @return int The number of tokens extracted
 */
static int tokenize_command(char* line, char** tokens, int max_tokens) {
  if (!line || !tokens)
    return 0;

  int count = 0;
  char* p = line;

  while (*p != '\0' && count < max_tokens) {
    while (*p == ' ' || *p == '\t')
      p++;

    if (*p == '\0')
      break;

    tokens[count++] = p;

    while (*p != '\0' && *p != ' ' && *p != '\t')
      p++;

    if (*p != '\0') {
      *p = '\0';
      p++;
    }
  }

  return count;
}

/**
 * @brief Tokenizes a line and dispatches it to the matching command's handler
 *
 * @param dbg The debugger
 * @param line The command line to dispatch; mutated in place by tokenization
 * @return int The handler's CommandResult, or 0 if the line was empty or the command was unrecognized
 */
static int dispatch(DebuggerState* dbg, char* line) {
  if (!dbg || !line)
    return 0;

  char* tokens[MAX_TOKENS];
  int count = tokenize_command(line, tokens, MAX_TOKENS);
  if (count == 0)
    return 0;

  const DebugCommand* cmd = find_command(tokens[0]);
  if (!cmd) {
    fprintf(stdout, "Unknown command: %s (try \"help\")\n", tokens[0]);
    return 0;
  }

  return cmd->handler(dbg, count - 1, &tokens[1]);
}

void debugger_cli(DebuggerState* dbg) {
  char line[MAX_INPUT_LENGTH];
  char last_line[MAX_INPUT_LENGTH] = {0};

  while (1) {
    fprintf(stdout, "(dbg) ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
      fprintf(stdout, "\n");
      break;
    }

    line[strcspn(line, "\n")] = '\0';

    if (line[0] == '\0') {
      if (last_line[0] == '\0')
        continue;
      snprintf(line, sizeof(line), "%s", last_line);
    } else {
      snprintf(last_line, sizeof(last_line), "%s", line);
    }

    int result = dispatch(dbg, line);
    if (result == CMD_QUIT)
      break;
  }
}

/**
 * @brief Parses a token as either a numeric address literal or a symbol name
 *
 * @param dbg The debugger, used to resolve symbol names
 * @param token The address literal or symbol name to parse
 * @param out Where to write the resolved address
 * @return int 1 if successful, 0 if token is not a valid address or a known symbol
 */
static int parse_address_or_symbol(DebuggerState* dbg, const char* token, uint32_t* out) {
  if (!dbg || !token || !out)
    return 0;

  char* end;
  unsigned long val = strtoul(token, &end, 0);
  if (end != token && *end == '\0') {
    *out = (uint32_t)val;
    return 1;
  }

  Symbol s;
  if (!resolve_symbol(dbg->symbol_table, token, strlen(token), &s))
    return 0;

  *out = s.value;
  return 1;
}

/**
 * @brief Prints a human-readable message for a debugger stop reason, followed by the current
 * instruction's disassembly if the program has not exited
 *
 * @param dbg The debugger
 * @param reason The stop reason to report
 */
static void print_stop_reason(DebuggerState* dbg, DebugStopReason reason) {
  switch (reason) {
  case STOP_BREAKPOINT:
    fprintf(stdout, "Breakpoint hit\n");
    break;
  case STOP_WATCHPOINT:
    fprintf(stdout, "Watchpoint triggered\n");
    break;
  case STOP_EBREAK:
    fprintf(stdout, "EBREAK\n");
    break;
  case STOP_EXITED:
    fprintf(stdout, "Program exited (code %d)\n", dbg->prog->exit_code);
    return;
  case STOP_FAULT:
    fprintf(stdout, "Halted (fault)\n");
    break;
  case STOP_STEP:
    break;
  }
  if (dbg->prog->status != CPU_EXITED) {
    debugger_print_disas(dbg, dbg->prog->pc, 1, FORMAT_HEX);
  }
}

static int cmd_step(DebuggerState* dbg, int argc, char** argv) {
  (void)argc;
  (void)argv;
  DebugStopReason r = debugger_step(dbg);
  print_stop_reason(dbg, r);
  return (r == STOP_FAULT) ? CMD_ERROR : CMD_OK;
}

static int cmd_continue(DebuggerState* dbg, int argc, char** argv) {
  (void)argc;
  (void)argv;
  DebugStopReason r = debugger_continue(dbg);
  print_stop_reason(dbg, r);
  return (r == STOP_FAULT) ? CMD_ERROR : CMD_OK;
}

static int cmd_run(DebuggerState* dbg, int argc, char** argv) {
  (void)argc;
  (void)argv;
  if (!debugger_reset(dbg)) {
    fprintf(stdout, "Could not reset program state\n");
    return CMD_ERROR;
  }
  DebugStopReason r = debugger_continue(dbg);
  print_stop_reason(dbg, r);
  return CMD_OK;
}

static int cmd_break(DebuggerState* dbg, int argc, char** argv) {
  if (argc < 1) {
    fprintf(stdout, "Usage: break <addr|symbol>\n");
    return CMD_ERROR;
  }

  uint32_t addr;
  if (!parse_address_or_symbol(dbg, argv[0], &addr)) {
    fprintf(stdout, "Unknown address or symbol: %s\n", argv[0]);
    return CMD_ERROR;
  }

  if (!debugger_add_breakpoint(dbg, addr)) {
    fprintf(stdout, "Could not add breakpoint (table full?)\n");
    return CMD_ERROR;
  }

  fprintf(stdout, "Breakpoint set at 0x%08X\n", addr);
  return CMD_OK;
}

static int cmd_delete(DebuggerState* dbg, int argc, char** argv) {
  if (argc < 1) {
    fprintf(stdout, "Usage: delete <addr|symbol>\n");
    return CMD_ERROR;
  }

  uint32_t addr;
  if (!parse_address_or_symbol(dbg, argv[0], &addr)) {
    fprintf(stdout, "Unknown address or symbol: %s\n", argv[0]);
    return CMD_ERROR;
  }

  int removed_bp = debugger_find_breakpoint(dbg, addr);
  int removed_wp = debugger_find_watchpoint(dbg, addr);

  if (removed_bp != -1)
    debugger_remove_breakpoint(dbg, addr);
  if (removed_wp != -1)
    debugger_remove_watchpoint(dbg, addr);

  if (removed_bp == -1 && removed_wp == -1) {
    fprintf(stdout, "No breakpoint or watchpoint at 0x%08X\n", addr);
    return CMD_ERROR;
  }
  if (removed_bp != -1)
    fprintf(stdout, "Breakpoint removed at 0x%08X\n", addr);
  if (removed_wp != -1)
    fprintf(stdout, "Watchpoint removed at 0x%08X\n", addr);
  return CMD_OK;
}

static int cmd_watch(DebuggerState* dbg, int argc, char** argv) {
  if (argc < 1) {
    fprintf(stdout, "Usage: watch <addr|symbol>\n");
    return CMD_ERROR;
  }

  uint32_t addr;
  if (!parse_address_or_symbol(dbg, argv[0], &addr)) {
    fprintf(stdout, "Unknown address or symbol: %s\n", argv[0]);
    return CMD_ERROR;
  }

  int r = debugger_add_watchpoint(dbg, addr);
  if (r == 0) {
    fprintf(stdout, "Could not add watchpoint (table full?)\n");
    return CMD_ERROR;
  }
  if (r == 2)
    fprintf(stdout, "Watchpoint set at 0x%08X (warning: initial read failed)\n", addr);
  else
    fprintf(stdout, "Watchpoint set at 0x%08X\n", addr);

  return CMD_OK;
}

static int cmd_info(DebuggerState* dbg, int argc, char** argv) {
  if (argc < 1) {
    fprintf(stdout, "Usage: info registers | breakpoints | watchpoints\n");
    return CMD_ERROR;
  }

  if (strcmp(argv[0], "registers") == 0) {
    debugger_print_register(dbg, 0xFFFFFFFF, FORMAT_HEX);
  } else if (strcmp(argv[0], "breakpoints") == 0) {
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
      if (dbg->breakpoints[i].set) {
        fprintf(stdout, "  [%d] 0x%08X %s\n", i, dbg->breakpoints[i].addr,
                dbg->breakpoints[i].active ? "" : "(disabled)");
      }
    }
  } else if (strcmp(argv[0], "watchpoints") == 0) {
    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
      if (dbg->watchpoints[i].set) {
        fprintf(stdout, "  [%d] 0x%08X = 0x%08X %s\n", i, dbg->watchpoints[i].addr, dbg->watchpoints[i].last_val,
                dbg->watchpoints[i].active ? "" : "(disabled)");
      }
    }
  } else {
    fprintf(stdout, "Unknown info target: %s\n", argv[0]);
    return CMD_ERROR;
  }
  return CMD_OK;
}

static int cmd_disas(DebuggerState* dbg, int argc, char** argv) {
  uint32_t addr = dbg->prog->pc;
  uint32_t count = 10;

  if (argc >= 1 && !parse_address_or_symbol(dbg, argv[0], &addr)) {
    fprintf(stdout, "Unknown address or symbol: %s\n", argv[0]);
    return CMD_ERROR;
  }
  if (argc >= 2)
    count = (uint32_t)strtoul(argv[1], NULL, 0);

  debugger_print_disas(dbg, addr, count, FORMAT_HEX);
  return CMD_OK;
}

static int cmd_examine(DebuggerState* dbg, int argc, char** argv) {
  if (argc < 2) {
    fprintf(stdout, "Usage: x <addr> <count>\n");
    return CMD_ERROR;
  }

  uint32_t addr;
  if (!parse_address_or_symbol(dbg, argv[0], &addr)) {
    fprintf(stdout, "Unknown address or symbol: %s\n", argv[0]);
    return CMD_ERROR;
  }
  uint32_t count = (uint32_t)strtoul(argv[1], NULL, 0);

  debugger_print_memory(dbg, addr, count, 4, FORMAT_HEX);
  return CMD_OK;
}

static int cmd_quit(DebuggerState* dbg, int argc, char** argv) {
  (void)dbg;
  (void)argc;
  (void)argv;
  return CMD_QUIT;
}

static int cmd_help(DebuggerState* dbg, int argc, char** argv) {
  (void)dbg;
  (void)argc;
  (void)argv;
  for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++) {
    fprintf(stdout, "\t%-12s %-45s %s\n", COMMANDS[i].names[0], COMMANDS[i].usage, COMMANDS[i].help);
  }
  return CMD_OK;
}