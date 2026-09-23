#include "../include/api_debugger.h"
#include "../include/assembled_program.h"
#include "../include/binary_to_instruction.h"
#include "../include/debugger_cli.h"
#include "../include/elf32.h"
#include "../include/elf32_load.h"
#include "../include/elf_symbols.h"
#include "../include/emulator.h"
#include "../include/loader.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Reads an entire file into a newly allocated buffer
 *
 * @param path The path of the file to read
 * @param len_out Where to write the number of bytes read
 * @return uint8_t* The file's contents, must be freed by the caller, NULL if the file could not be opened or read
 */
static uint8_t* read_file(const char* path, size_t* len_out) {
  FILE* file = fopen(path, "rb");
  if (!file)
    return NULL;

  uint8_t* buf = NULL;
  if (fseek(file, 0, SEEK_END) != 0)
    goto fail;
  long size = ftell(file);
  if (size <= 0 || (unsigned long)size > MAX_ELF_FILE_SIZE)
    goto fail;
  rewind(file);

  buf = malloc((size_t)size);
  if (!buf || fread(buf, 1, (size_t)size, file) != (size_t)size)
    goto fail;

  fclose(file);
  *len_out = (size_t)size;
  return buf;

fail:
  free(buf);
  fclose(file);
  return NULL;
}

/**
 * @brief Fills an AssembledProgram view over a parsed ELF image for the debugger
 *
 * @param prog The program view to fill; borrows segment data from the image and does not own anything
 * @param img The parsed ELF image, whose backing buffer must outlive the view
 * @param symbols The symbol table built from the image
 */
static void fill_program_view(AssembledProgram* prog, const Elf32Image* img, SymbolTable* symbols) {
  memset(prog, 0, sizeof *prog);
  prog->symbol_table = symbols;

  uint32_t text_vaddr = 0;
  for (size_t i = 0; i < img->nsegments; i++) {
    const Elf32LoadSegment* seg = &img->segments[i];
    MemorySegment* dst;
    if (seg->flags & PF_X)
      dst = &prog->text;
    else if (seg->flags & PF_W)
      dst = seg->filesz ? &prog->data : &prog->bss;
    else
      dst = &prog->rodata;

    if (dst->size > 0)
      continue;
    dst->data = (uint8_t*)seg->data;
    dst->size = seg->filesz ? seg->filesz : seg->memsz;
    if (dst == &prog->text)
      text_vaddr = seg->vaddr;
  }
  prog->entry_offset = img->entry >= text_vaddr ? img->entry - text_vaddr : 0;
}

/**
 * @brief Prints command-line usage instructions to stderr
 *
 * @param prog_name The name of the program, typically argv[0]
 */
static void print_usage(const char* prog_name) { fprintf(stderr, "Usage: %s <run|debug> <program.elf>\n", prog_name); }

/**
 * @brief Entry point; loads a RISC-V ELF32 executable and either runs or interactively debugs it
 *
 * @param argc The argument count; must be 3
 * @param argv The argument vector: program name, mode ("run" or "debug"), and the path to the ELF file
 * @return int 0 on success; 1 on usage/file error, 2 on an invalid ELF, 3 on load failure, 4 if the run-mode program
 * did not exit successfully
 *
 * @note In debug mode the exit code does not reflect the outcome of the debugger session; it is always treated as
 * success
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

  size_t len = 0;
  uint8_t* buf = read_file(path, &len);
  if (!buf) {
    fprintf(stderr, "Failed to read file \"%s\"\n", path);
    return 1;
  }

  Elf32Image img;
  Elf32Status status = elf32_parse(buf, len, &img);
  if (status != ELF32_OK) {
    fprintf(stderr, "Invalid ELF file \"%s\": %s\n", path, elf32_status_str(status));
    free(buf);
    return 2;
  }

  ProgramState* state = load_binary(&img);
  if (!state) {
    free(buf);
    fprintf(stderr, "Failed to load binary into virtual memory\n");
    return 3;
  }

  SymbolTable* symbols = NULL;
  int result;
  if (debug_mode) {
    symbols = symbol_table_from_elf(&img);
    if (!symbols) {
      free_loaded_binary(state);
      free(buf);
      fprintf(stderr, "Failed to read symbols from ELF file\n");
      return 3;
    }

    AssembledProgram prog;
    fill_program_view(&prog, &img, symbols);

    DebuggerState dbg;
    debugger_init(&dbg, state, &img, symbols);
    debugger_cli(&dbg);
    state = dbg.prog;
    result = 1;
  } else {
    result = execute_program(state);
  }

  free_loaded_binary(state);
  free_symbol_table(symbols);
  free(buf);

  if (!result) {
    fprintf(stderr, "Program execution failed\n");
    return 4;
  }

  return 0;
}