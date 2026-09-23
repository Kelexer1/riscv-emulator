#include "../include/symbol_table.h"
#include "../include/helper.h"
#include <stdlib.h>
#include <string.h>

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