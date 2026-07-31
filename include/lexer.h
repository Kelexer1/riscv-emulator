#ifndef LEXER_H
#define LEXER_H

#include "arena.h"
#include "riscv.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Represents what kind of token a Token struct is encoding
 */
typedef enum : uint8_t {
  TOKEN_UNKNOWN,
  TOKEN_LABEL,
  TOKEN_SYMBOL,
  TOKEN_MNEMONIC,
  TOKEN_PSEUDO,
  TOKEN_INSTRUCTION_AMBIGUOUS,
  TOKEN_REGISTER,
  TOKEN_IMMEDIATE,
  TOKEN_COMMA,
  TOKEN_LPARAN,
  TOKEN_RPARAN,
  TOKEN_DIRECTIVE,
  TOKEN_STRING
} TokenType;

/**
 * @brief Encodes a single token of information from the raw text input
 */
typedef struct token {
  TokenType type;
  union {
    struct {
      Opcode op;
      PseudoOpcode pseudo;
    } opcode;
    Directive dir;
    int imm;
    int reg;
  };
  char* start;
  size_t len;
  uint32_t line;
  uint32_t col;
  struct token* next;
} Token;

/**
 * @brief Encodes the result of tokenizing a full assembly program, as one token
 * linked list per line
 */
typedef struct {
  Token** lines;
  size_t count;
  Arena arena;
} TokenizedInput;

/**
 * @brief Tokenizes an assembly program
 *
 * @param src The string representation of the raw code, null-terminated
 * @return TokenizedInput* An array of linked lists, where each element is the head for the
 * tokens of one line of code, NULL if an error occurred
 */
TokenizedInput* tokenize_input(const char* src);

/**
 * @brief Frees all memory associated with a tokenized input, such as from
 * tokenize_input (including char*'s, freed via the arena)
 *
 * @param tokenized The tokenized input
 */
void free_tokenized_input(TokenizedInput* tokenized);

#endif