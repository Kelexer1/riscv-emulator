#include "../include/first_pass.h"
#include "../include/helper.h"
#include "../include/logger.h"
#include "../include/memory.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Append a symbol to the symbol table linked list
 *
 * @param head The head of the linked list
 * @param tail The tail of the linked list (where the symbol will be appended
 * to)
 * @param sym The symbol to append
 */
static void append_symbol(Symbol** head, Symbol** tail, Symbol* sym) {
  if (!head || !tail || !sym)
    return;

  if (!*head) {
    *head = sym;
    *tail = sym;
  } else {
    (*tail)->next = sym;
    *tail = sym;
  }
}

int resolve_symbol(SymbolTable* symbol_table, const char* symbol, size_t len, Symbol* out) {
  if (!symbol_table || !symbol)
    return 0;

  for (Symbol* curr = symbol_table->head; curr != NULL; curr = curr->next) {
    if (strlenequal(curr->name, curr->len, symbol, len)) {
      if (out)
        memcpy(out, curr, sizeof(Symbol));
      return 1;
    }
  }

  return 0;
}

const Symbol* value_to_symbol(SymbolTable* symbol_table, uint32_t val, SymbolType expected_type) {
  if (!symbol_table)
    return NULL;

  for (Symbol* curr = symbol_table->head; curr != NULL; curr = curr->next) {
    if (curr->value == val && curr->type == expected_type)
      return curr;
  }

  return NULL;
}

SymbolTable* assemble_symbol_table(ExpandedInput* expanded) {
  if (!expanded)
    return NULL;

  SymbolTable* result = malloc(sizeof(SymbolTable));
  if (!result)
    return NULL;

  if (!arena_init(&result->arena, expanded->arena.used)) {
    free(result);
    return NULL;
  }

  result->head = NULL;

  Symbol* head = NULL;
  Symbol* tail = NULL;

  uint32_t lc_text = 0;
  uint32_t lc_data = 0;
  uint32_t lc_bss = 0;
  uint32_t lc_rodata = 0;
  Section curr_section = SECTION_TEXT;

  for (size_t i = 0; i < expanded->count; i++) {
    ParsedLine ins = expanded->lines[i];
    if (resolve_symbol(result, ins.label, ins.len, NULL)) {
      LOG_ERROR_LINE("Duplicate symbol \"%.*s\"", ins.line, (int)ins.len, ins.label);
      goto fail;
    }

    uint32_t* lc = curr_section == SECTION_TEXT   ? &lc_text
                   : curr_section == SECTION_DATA ? &lc_data
                   : curr_section == SECTION_BSS  ? &lc_bss
                                                  : &lc_rodata;

    if (ins.label && ins.len != 0) {
      Symbol* new = malloc(sizeof(Symbol));
      if (!new)
        goto fail;
      char* owned = arena_alloc(&result->arena, ins.label, ins.len);
      if (!owned) {
        free(new);
        goto fail;
      }
      new->type = SYMBOL_LABEL;
      new->line = ins.line;
      new->value = *lc;
      new->section = curr_section;
      new->name = owned;
      new->len = ins.len;
      new->next = NULL;
      append_symbol(&head, &tail, new);
      result->head = head;
    } else if (ins.type == LINE_DIRECTIVE && ins.directive.directive == DIRECTIVE_EQU) {
      Symbol* new = malloc(sizeof(Symbol));
      if (!new)
        goto fail;
      char* owned = arena_alloc(&result->arena, ins.directive.args->start, ins.directive.args->len);
      if (!owned) {
        free(new);
        goto fail;
      }
      new->type = SYMBOL_CONSTANT;
      new->line = ins.line;
      new->value = ins.directive.args->next->imm;
      new->section = SECTION_TEXT; // Section is unused for symbol
                                   // constants, so a placeholder is used
      new->name = owned;
      new->len = ins.directive.args->len;
      new->next = NULL;
      append_symbol(&head, &tail, new);
      result->head = head;
    }

    switch (ins.type) {
    case LINE_INSTRUCTION:
      if (ins.instruction.type == INSTRUCTION_PSEUDO)
        goto fail;
      if (curr_section != SECTION_TEXT) {
        LOG_ERROR_LINE("Instruction encountered outside .text", ins.line);
        goto fail;
      }
      *lc += 4;
      break;
    case LINE_DIRECTIVE:
      switch (ins.directive.directive) {
      case DIRECTIVE_UNKNOWN:
        LOG_ERROR_LINE("Invalid directive", ins.line);
        goto fail;
      case DIRECTIVE_TEXT:
        curr_section = SECTION_TEXT;
        break;
      case DIRECTIVE_DATA:
        curr_section = SECTION_DATA;
        break;
      case DIRECTIVE_BSS:
        curr_section = SECTION_BSS;
        break;
      case DIRECTIVE_RODATA:
        curr_section = SECTION_RODATA;
        break;
      default:
        *lc += directive_size(&ins, *lc);
      }
      break;
    case LINE_LABEL:
      break;
    }
  }

  uint32_t base_text = 0;
  uint32_t base_rodata = (base_text + lc_text + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
  uint32_t base_data = (base_rodata + lc_rodata + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
  uint32_t base_bss = (base_data + lc_data + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);

  for (Symbol* curr = head; curr != NULL; curr = curr->next) {
    if (curr->type != SYMBOL_LABEL)
      continue;
    switch (curr->section) {
    case SECTION_TEXT:
      curr->value += base_text;
      break;
    case SECTION_DATA:
      curr->value += base_data;
      break;
    case SECTION_BSS:
      curr->value += base_bss;
      break;
    case SECTION_RODATA:
      curr->value += base_rodata;
      break;
    }
  }

  result->head = head;
  return result;

fail:
  Symbol* curr = head;
  while (curr) {
    Symbol* next = curr->next;
    free(curr);
    curr = next;
  }
  arena_free(&result->arena);
  free(result);
  return NULL;
}

void free_symbol_table(SymbolTable* symbol_table) {
  if (!symbol_table)
    return;

  Symbol* curr = symbol_table->head;
  while (curr) {
    Symbol* next = curr->next;
    free(curr);
    curr = next;
  }

  arena_free(&symbol_table->arena);
  free(symbol_table);
}