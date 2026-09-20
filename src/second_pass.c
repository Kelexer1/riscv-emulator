#include "../include/second_pass.h"
#include "../include/dynamic_array.h"
#include "../include/helper.h"
#include "../include/instruction_to_binary.h"
#include "../include/logger.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Pushes a word of information into a binary memory segment as bytes
 *
 * @param arr The dynamic array holding bytes
 * @param word The word to push
 * @return int 1 if success, 0 if the operation failed
 */
static int push_word(DynamicArray* arr, uint32_t word) {
  if (!arr)
    return 0;

  uint8_t bytes[4] = {(uint8_t)(word & 0xFF), (uint8_t)((word >> 8) & 0xFF), (uint8_t)((word >> 16) & 0xFF),
                      (uint8_t)((word >> 24) & 0xFF)};

  for (int i = 0; i < 4; i++) {
    if (!dynamic_array_push(arr, &bytes[i]))
      return 0;
  }

  return 1;
}

/**
 * @brief Pushes a half-word of information into a binary memory segment as
 * bytes
 *
 * @param arr The dynamic array holding bytes
 * @param half The half-word to push
 * @return int 1 if success, 0 if the operation failed
 */
static int push_half(DynamicArray* arr, uint16_t half) {
  if (!arr)
    return 0;

  uint8_t bytes[2] = {(uint8_t)(half & 0xFF), (uint8_t)((half >> 8) & 0xFF)};

  for (int i = 0; i < 2; i++) {
    if (!dynamic_array_push(arr, &bytes[i]))
      return 0;
  }

  return 1;
}

/**
 * @brief Pushes a string into a binary memory segment as bytes
 *
 * @param arr The dynamic array holding bytes
 * @param str The char array to push
 * @param len The number of characters in str
 * @param null_terminate A non-zero integer if the string should have a
 * null-terminator appended, 0 otherwise
 * @return int 1 if success, 0 if the operation failed
 */
static int push_string(DynamicArray* arr, char* str, size_t len, int null_terminate) {
  if (!arr || !str)
    return 0;

  for (size_t i = 0; i < len; i++) {
    uint8_t b = (uint8_t)str[i];
    if (!dynamic_array_push(arr, &b))
      return 0;
  }

  if (null_terminate) {
    uint8_t null = 0;
    if (!dynamic_array_push(arr, &null))
      return 0;
  }

  return 1;
}

/**
 * @brief Pushes a byte of information into a binary memory segment as bytes
 *
 * @param arr The dynamic array holding bytes
 * @param byte The byte to push
 * @return int 1 if success, 0 if the operation failed
 */
static int push_byte(DynamicArray* arr, uint8_t byte) {
  if (!arr)
    return 0;
  return dynamic_array_push(arr, &byte);
}

/**
 * @brief Pushes directive data into a memory segment
 *
 * @param line The parsed directive
 * @param current_segment The memory segment to push to
 * @return int 1 if success, 0 if the operation failed
 */
static int handle_directive_data(ParsedLine* line, DynamicArray* current_segment) {
  if (line->type != LINE_DIRECTIVE)
    return 0;

  switch (line->directive.directive) {
  case DIRECTIVE_BYTE:
    for (DirectiveArg* arg = line->directive.args; arg != NULL; arg = arg->next) {
      if (!push_byte(current_segment, (uint8_t)arg->imm))
        return 0;
    }
    break;
  case DIRECTIVE_HALF:
    for (DirectiveArg* arg = line->directive.args; arg != NULL; arg = arg->next) {
      if (!push_half(current_segment, (uint16_t)arg->imm))
        return 0;
    }
    break;
  case DIRECTIVE_WORD:
    for (DirectiveArg* arg = line->directive.args; arg != NULL; arg = arg->next) {
      if (!push_word(current_segment, (uint32_t)arg->imm))
        return 0;
    }
    break;
  case DIRECTIVE_STRING:
    if (!push_string(current_segment, line->directive.args->start, line->directive.args->len, 1))
      return 0;
    break;
  case DIRECTIVE_ASCII:
    if (!push_string(current_segment, line->directive.args->start, line->directive.args->len, 0))
      return 0;
    break;
  case DIRECTIVE_ZERO:
    for (int i = 0; i < line->directive.args->imm; i++) {
      if (!push_byte(current_segment, 0))
        return 0;
    }
    break;
  case DIRECTIVE_ALIGN:
    uint32_t size = directive_size(line, current_segment->count);
    for (uint32_t i = 0; i < size; i++) {
      if (!push_byte(current_segment, 0))
        return 0;
    }
    break;
  case DIRECTIVE_EQU:
    return 1;
  default:
    return 0;
  }

  return 1;
}

/**
 * @brief Returns the offset into the text segment in which the code execution
 * should begin
 * (_start -> main -> 0 offset)
 *
 * @param symbol_table The symbol table
 * @return uint32_t The offset
 */
static uint32_t get_entry_point(SymbolTable* symbol_table) {
  Symbol sym;
  if (resolve_symbol(symbol_table, "_start", 6, &sym) && sym.type == SYMBOL_LABEL)
    return sym.value;
  if (resolve_symbol(symbol_table, "main", 4, &sym) && sym.type == SYMBOL_LABEL)
    return sym.value;
  return 0;
}

AssembledProgram* finalize_assembly(ExpandedInput* expanded, SymbolTable* symbol_table) {
  if (!expanded || !symbol_table)
    return NULL;

  AssembledProgram* result = calloc(1, sizeof(AssembledProgram));
  if (!result)
    return NULL;

  DynamicArray text = {0};
  DynamicArray data = {0};
  DynamicArray bss = {0};
  DynamicArray rodata = {0};

  DynamicArray text_lines = {0};

  DynamicArray* current_segment = &text;

  if (!dynamic_array_init(&text, sizeof(uint8_t), 256))
    goto fail;
  if (!dynamic_array_init(&data, sizeof(uint8_t), 256))
    goto fail;
  if (!dynamic_array_init(&bss, sizeof(uint8_t), 256))
    goto fail;
  if (!dynamic_array_init(&rodata, sizeof(uint8_t), 256))
    goto fail;

  if (!dynamic_array_init(&text_lines, sizeof(uint32_t), 256))
    goto fail;

  for (size_t i = 0; i < expanded->count; i++) {
    ParsedLine* line = &expanded->lines[i];

    switch (line->type) {
    case LINE_INSTRUCTION:
      if (line->instruction.type == INSTRUCTION_PSEUDO)
        goto fail;
      if (current_segment != &text) {
        LOG_ERROR_LINE("Instruction encountered outside .text", line->line);
        goto fail;
      }

      uint32_t bin;
      if (!instruction_to_binary(line, current_segment->count, symbol_table, &bin)) {
        LOG_ERROR_LINE("Failed to convert instruction to binary", line->line);
        goto fail;
      }
      if (!push_word(current_segment, bin))
        goto fail;
      if (!dynamic_array_push(&text_lines, &line->line))
        goto fail;
      break;
    case LINE_DIRECTIVE:
      switch (line->directive.directive) {
      case DIRECTIVE_UNKNOWN:
        LOG_ERROR_LINE("Invalid directive", line->line);
        goto fail;
      case DIRECTIVE_TEXT:
        current_segment = &text;
        continue;
      case DIRECTIVE_DATA:
        current_segment = &data;
        continue;
      case DIRECTIVE_BSS:
        current_segment = &bss;
        continue;
      case DIRECTIVE_RODATA:
        current_segment = &rodata;
        continue;
      default:
        break;
      }

      if (current_segment == &text) {
        LOG_ERROR_LINE("Invalid directive in .text segment", line->line);
        goto fail;
      }

      if (!handle_directive_data(line, current_segment))
        goto fail;

      break;
    case LINE_LABEL:
      break;
    }
  }

  result->symbol_table = symbol_table;
  result->entry_offset = get_entry_point(symbol_table);
  result->text_lines = (uint32_t*)text_lines.data;
  result->text_lines_size = text_lines.count;
  result->text.data = (uint8_t*)text.data;
  result->text.size = text.count;
  result->data.data = (uint8_t*)data.data;
  result->data.size = data.count;
  result->bss.data = (uint8_t*)bss.data;
  result->bss.size = bss.count;
  result->rodata.data = (uint8_t*)rodata.data;
  result->rodata.size = rodata.count;
  return result;

fail:
  dynamic_array_free(&text);
  dynamic_array_free(&data);
  dynamic_array_free(&bss);
  dynamic_array_free(&rodata);
  dynamic_array_free(&text_lines);
  free(result);
  return NULL;
}

void free_assembled_program(AssembledProgram* program) {
  if (!program)
    return;

  free(program->text_lines);
  free(program->text.data);
  free(program->data.data);
  free(program->bss.data);
  free(program->rodata.data);
  free_symbol_table(program->symbol_table);
  free(program);
}