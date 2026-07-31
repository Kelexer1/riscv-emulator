#include "../include/api_assembler.h"
#include "../include/expander.h"
#include "../include/first_pass.h"
#include "../include/lexer.h"
#include "../include/logger.h"
#include "../include/parser.h"
#include <stddef.h>
#include <stdlib.h>

AssembledProgram* assemble(char* assembly) {
  if (!assembly)
    return NULL;

  TokenizedInput* tokenized = NULL;
  ParsedInput* parsed = NULL;
  ExpandedInput* expanded = NULL;
  SymbolTable* symbol_table = NULL;
  AssembledProgram* prog = NULL;

  tokenized = tokenize_input(assembly);
  free(assembly);
  assembly = NULL;
  if (!tokenized) {
    LOG_ERROR("Failed to tokenize assembly");
    goto fail;
  }

  parsed = parse_tokenized_input(tokenized);
  free_tokenized_input(tokenized);
  tokenized = NULL;
  if (!parsed) {
    LOG_ERROR("Failed to parse assembly");
    goto fail;
  }

  expanded = expand_pseudoinstructions(parsed);
  free_parsed_input(parsed);
  parsed = NULL;
  if (!expanded) {
    LOG_ERROR("Failed to expand pseudoinstructions");
    goto fail;
  }

  symbol_table = assemble_symbol_table(expanded);
  if (!symbol_table) {
    LOG_ERROR("Failed to build symbol table");
    goto fail;
  }

  prog = finalize_assembly(expanded, symbol_table);
  free_expanded_input(expanded);
  expanded = NULL;
  if (!prog) {
    LOG_ERROR("Failed to finalize assembly");
    goto fail;
  }

  return prog;

fail:
  free(assembly);
  free_tokenized_input(tokenized);
  free_parsed_input(parsed);
  free_expanded_input(expanded);
  free_symbol_table(symbol_table);
  free_assembled_program(prog);
  return NULL;
}