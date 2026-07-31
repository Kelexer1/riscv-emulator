#include "../include/api_assembler.h"
#include "../include/api_debugger.h"
#include "../include/binary_to_instruction.h"
#include "../include/debugger_cli.h"
#include "../include/dynamic_array.h"
#include "../include/emulator.h"
#include "../include/loader.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHUNK_SIZE 1024

/**
 * @brief Reads an entire file into a newly allocated, null-terminated string
 *
 * @param path The path of the file to read
 * @return char* A null-terminated buffer containing the file's contents, must be freed by the
 * caller, NULL if the file could not be opened or read
 */
static char* file_to_string(const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file)
    return NULL;

  DynamicArray result = {0};
  if (!dynamic_array_init(&result, 1, 512)) {
    fclose(file);
    return NULL;
  }

  char chunk[CHUNK_SIZE];
  size_t num_read;
  while ((num_read = fread(chunk, 1, CHUNK_SIZE, file))) {
    for (size_t i = 0; i < num_read; i++) {
      if (!dynamic_array_push(&result, &chunk[i]))
        goto fail;
    }
  }

  if (ferror(file))
    goto fail;

  char null = '\0';
  if (!dynamic_array_push(&result, &null))
    goto fail;

  fclose(file);
  return (char*)result.data;

fail:
  fclose(file);
  dynamic_array_free(&result);
  return NULL;
}

/**
 * @brief Prints command-line usage instructions to stderr
 *
 * @param prog_name The name of the program, typically argv[0]
 */
static void print_usage(const char* prog_name) { fprintf(stderr, "Usage: %s <run|debug> <program.*>\n", prog_name); }

/**
 * @brief Entry point; assembles, loads, and either runs or interactively debugs a RISC-V
 * assembly program given on the command line
 *
 * @param argc The argument count; must be 3
 * @param argv The argument vector: program name, mode ("run" or "debug"), and the path to the
 * assembly source file
 * @return int 0 on success; 1 on usage/file error, 2 on assembly failure, 3 on load failure, 4
 * if the run-mode program did not exit successfully
 *
 * @note In debug mode the exit code does not reflect the outcome of the debugger session; it is
 * always treated as success
 */
int main(int argc, char** argv) {
  if (argc != 3) {
    print_usage(argv[0]);
    return 1;
  }

  init_decode_table();

  const char* mode = argv[1];
  const char* path = argv[2];

  int debug_mode;
  if (strcmp(mode, "run") == 0) {
    debug_mode = 0;
  } else if (strcmp(mode, "debug") == 0) {
    debug_mode = 1;
  } else {
    print_usage(argv[0]);
    return 1;
  }

  char* assembly = file_to_string(path);
  if (!assembly) {
    fprintf(stderr, "Failed to open file \"%s\"\n", argv[2]);
    return 1;
  }

  AssembledProgram* prog = assemble(assembly);
  if (!prog) {
    fprintf(stderr, "Failed to assemble binary\n");
    return 2;
  }

  ProgramState* state = load_binary(prog);
  if (!state) {
    free_assembled_program(prog);
    fprintf(stderr, "Failed to load binary into virtual memory\n");
    return 3;
  }

  int result;
  if (debug_mode) {
    DebuggerState dbg;
    debugger_init(&dbg, state, prog, prog->symbol_table);
    debugger_cli(&dbg);
    state = dbg.prog;
    result = 1;
  } else {
    result = execute_program(state);
  }

  free_loaded_binary(state);
  free_assembled_program(prog);

  if (!result) {
    fprintf(stderr, "Program execution failed\n");
    return 4;
  }

  return 0;
}