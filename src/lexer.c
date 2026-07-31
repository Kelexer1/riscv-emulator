#include "../include/lexer.h"
#include "../include/dynamic_array.h"
#include "../include/helper.h"
#include "../include/logger.h"
#include "../include/riscv.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a token object, must be freed by caller
 *
 * @param arena The arena to allocate the token's owned text-representation from
 * @param type The type of token
 * @param start The start of the text-representation
 * @param len The size of the text-representation in characters
 * @param in The input value, which will be parsed depending on type. For
 * example, for TOKEN_MNEMONIC, in represents the Opcode enum
 * @param line The line this token is on
 * @param col The column this token starts at
 * @return Token* The token
 */
static Token* create_token(Arena* arena, TokenType type, const char* start, size_t len, int in, uint32_t line,
                           uint32_t col) {
  if (!arena || !start)
    return NULL;

  Token* result = malloc(sizeof(Token));
  if (!result)
    return NULL;

  char* owned = arena_alloc(arena, start, len);
  if (!owned) {
    free(result);
    return NULL;
  }

  if (len > 0)
    memcpy(owned, start, len);

  owned[len] = '\0';

  result->type = type;
  result->start = owned;
  result->len = len;
  result->line = line;
  result->col = col;
  result->next = NULL;

  switch (type) {
  case TOKEN_REGISTER:
    result->reg = in;
    break;
  case TOKEN_IMMEDIATE:
    result->imm = in;
    break;
  case TOKEN_DIRECTIVE:
    result->dir = (Directive)in;
    break;
  default:
    break;
  }

  return result;
}

/**
 * @brief Creates a token for a mnemonic, pseudoinstruction, or ambiguous instruction, storing
 * both possible opcodes
 *
 * @param arena The arena to allocate the token's owned text-representation from
 * @param type The token type; must be TOKEN_MNEMONIC, TOKEN_PSEUDO, or TOKEN_INSTRUCTION_AMBIGUOUS
 * @param start The start of the text-representation
 * @param len The size of the text-representation in characters
 * @param op The real opcode, OP_UNKNOWN if not applicable
 * @param pseudo The pseudoinstruction opcode, PSEUDO_UNKNOWN if not applicable
 * @param line The line this token is on
 * @param col The column this token starts at
 * @return Token* The token, or NULL if type is invalid or an error occurred
 */
static Token* create_token_instruction(Arena* arena, TokenType type, const char* start, size_t len, Opcode op,
                                       PseudoOpcode pseudo, uint32_t line, uint32_t col) {
  if (type != TOKEN_MNEMONIC && type != TOKEN_PSEUDO && type != TOKEN_INSTRUCTION_AMBIGUOUS)
    return NULL;
  Token* result = create_token(arena, type, start, len, 0, line, col);
  if (!result)
    return NULL;
  result->opcode.op = op;
  result->opcode.pseudo = pseudo;
  return result;
}

/**
 * @brief Append a token to a Token linked list
 *
 * @param head The head of the linked list
 * @param tail The tail of the linked list (where the token will be appended to)
 * @param token The token to append
 */
static void append_token(Token** head, Token** tail, Token* token) {
  if (!head || !tail || !token)
    return;
  if (!*head) {
    *head = token;
    *tail = token;
  } else {
    (*tail)->next = token;
    *tail = token;
  }
}

/**
 * @brief Returns whether a character sequence is a valid symbol
 *
 * @param start The start of the array
 * @param len The array size in characters
 * @return int 1 if start and len encode a valid symbol, 0 otherwise
 */
static int is_valid_symbol(const char* start, size_t len) {
  if (!start || len == 0)
    return 0;

  if ('0' <= start[0] && start[0] <= '9')
    return 0;

  for (size_t i = 0; i < len; i++) {
    char c = start[i];
    if (!(('a' <= c && c <= 'z') || ('0' <= c && c <= '9') || (c == '_') || (c == '.')))
      return 0;
  }

  return 1;
}

/**
 * @brief Returns whether a character sequence is a valid immediate
 *
 * @param start The start of the array
 * @param len The array size in characters
 * @param out Where to write the immediate value as an integer, NULL if not
 * needed
 * @return int 1 if start and len encode a valid immediate, 0 otherwise
 */
static int is_valid_immediate(const char* start, size_t len, int* out) {
  if (len == 0)
    return 0;

  if (len == 3 && start[0] == '\'' && start[2] == '\'') {
    *out = (int32_t)start[1];
    return 1;
  }

  char* end = NULL;
  errno = 0;
  long dec = strtol(start, &end, 10);
  if (end == start + len) {
    if (errno == ERANGE || dec < INT32_MIN || dec > INT32_MAX)
      return 0;
    *out = (int)dec;
    return 1;
  }

  if (len >= 3 && start[0] == '0' && start[1] == 'x') {
    errno = 0;
    long hex = strtol(start + 2, &end, 16);
    if (end == start + len) {
      if (errno == ERANGE || hex < INT32_MIN || hex > INT32_MAX)
        return 0;
      *out = (int)hex;
      return 1;
    }
  }

  return 0;
}

/**
 * @brief Returns whether a character sequence is a valid string (enclosed by
 * double quotes)
 *
 * @param start The start of the array
 * @param len The array size in characters
 * @return int 1 if start and len encode a valid string, 0 otherwise
 */
static int is_valid_string(const char* start, size_t len) {
  return len >= 2 && start[0] == '"' && start[len - 1] == '"';
}

/**
 * @brief Resolves escape sequences in a string literal's contents into their raw byte values
 *
 * @param start The start of the string contents, excluding surrounding quotes
 * @param len The number of characters in start
 * @param out_len Where to write the length of the unescaped result; also written on failure
 * to indicate how many bytes were processed before the error
 * @return char* A newly allocated buffer containing the unescaped bytes (not null-terminated),
 * must be freed by the caller. NULL if an invalid escape sequence was encountered
 */
static char* unescape_string(const char* start, size_t len, size_t* out_len) {
  if (!out_len)
    return NULL;
  char* buf = malloc(len > 0 ? len : 0);
  if (!buf)
    return NULL;

  size_t out = 0;
  for (size_t i = 0; i < len; i++) {
    if (start[i] == '\\') {
      if (i + 1 >= len) {
        *out_len = i;
        free(buf);
        return NULL;
      }
      i++;
      char c;
      switch (start[i]) {
      case 'n':
        c = '\n';
        break;
      case 't':
        c = '\t';
        break;
      case 'r':
        c = '\r';
        break;
      case '0':
        c = '\0';
        break;
      case '\\':
        c = '\\';
        break;
      case '"':
        c = '"';
        break;
      case '\'':
        c = '\'';
        break;
      default:
        if (out_len)
          *out_len = i;
        free(buf);
        return NULL;
      }
      buf[out++] = c;
    } else {
      buf[out++] = start[i];
    }
  }

  *out_len = out;
  return buf;
}

/**
 * @brief Converts a character sequence into the type of token it is, and its
 * value if applicable
 *
 * @param start The start of the array
 * @param len The array size in characters
 * @param out Where to write the token value (ex. immediate value if the token
 * is an immediate), NULL if not needed
 * @param out2 Where to write the second token value (only used in case an
 * instruction can be both pseudo or real)
 * @return TokenType The type of token, TOKEN_UNKNOWN if the sequence does not
 * encode a valid token or an error occurred
 */
static TokenType classify_token(const char* start, size_t len, int* out, int* out2) {
  if (!start || len == 0)
    return TOKEN_UNKNOWN;

  if (len == 1) {
    switch (*start) {
    case ',':
      return TOKEN_COMMA;
    case '(':
      return TOKEN_LPARAN;
    case ')':
      return TOKEN_RPARAN;
    }
  }

  if (is_valid_string(start, len))
    return TOKEN_STRING;

  if (is_valid_immediate(start, len, out))
    return TOKEN_IMMEDIATE;

  char* lower = to_lowercase(start, len);
  if (!lower)
    return TOKEN_UNKNOWN;

  if (lower[len - 1] == ':' && is_valid_symbol(lower, len - 1)) {
    free(lower);
    return TOKEN_LABEL;
  }

  Directive dir = parse_directive(lower, len);
  if (dir != DIRECTIVE_UNKNOWN) {
    *out = (int)dir;
    free(lower);
    return TOKEN_DIRECTIVE;
  }

  int reg = parse_register(lower, len);
  if (reg != -1) {
    *out = reg;
    free(lower);
    return TOKEN_REGISTER;
  }

  Opcode op = parse_mnemonic(lower, len);
  PseudoOpcode pseudo = parse_pseudo(lower, len);
  if (op != OP_UNKNOWN && pseudo == PSEUDO_UNKNOWN) {
    *out = (int)op;
    *out2 = (int)PSEUDO_UNKNOWN;
    free(lower);
    return TOKEN_MNEMONIC;
  } else if (op == OP_UNKNOWN && pseudo != PSEUDO_UNKNOWN) {
    *out = (int)OP_UNKNOWN;
    *out2 = (int)pseudo;
    free(lower);
    return TOKEN_PSEUDO;
  } else if (op != OP_UNKNOWN && pseudo != PSEUDO_UNKNOWN) {
    *out = (int)op;
    *out2 = (int)pseudo;
    free(lower);
    return TOKEN_INSTRUCTION_AMBIGUOUS;
  }

  TokenType result = is_valid_symbol(lower, len) ? TOKEN_SYMBOL : TOKEN_UNKNOWN;
  free(lower);
  return result;
}

/**
 * @brief Frees all memory associated with a token list
 *
 * @param head The head of the linked list
 */
static void free_token_list(Token* head) {
  Token* curr = head;
  while (curr) {
    Token* next = curr->next;
    free(curr);
    curr = next;
  }
}

/**
 * @brief Tokenizes a single line of an assembly program
 *
 * @param arena The arena to allocate tokens' owned text-representations from
 * @param line The line of assembly as a character array
 * @param line_len The number of characters in the line
 * @param line_number The line number
 * @param had_error Set to 1 if tokenization failed, 0 on success
 * @return Token* The head of a linked list of tokens encoding the line, NULL if the line was
 * empty or an error occurred
 */
static Token* tokenize_line(Arena* arena, const char* line, size_t line_len, uint32_t line_number, int* had_error) {
  Token* head = NULL;
  Token* tail = NULL;
  const char* ptr = line;
  const char* start = ptr;
  const char* end = line + line_len;

  uint32_t col;

  while (ptr < end) {
    while (ptr < end && (*ptr == ' ' || *ptr == '\t'))
      ptr++;

    if (ptr >= end || *ptr == '#')
      break;

    if (*ptr == ',' || *ptr == '(' || *ptr == ')') {
      col = (uint32_t)(ptr - line) + 1;
      TokenType type = (*ptr == ',') ? TOKEN_COMMA : (*ptr == '(') ? TOKEN_LPARAN : TOKEN_RPARAN;
      Token* tok = create_token(arena, type, ptr, 1, 0, line_number, col);
      if (!tok) {
        LOG_ERROR_LOCATION("Failed to create token", line_number, col);
        goto fail;
      }
      append_token(&head, &tail, tok);
      ptr++;
      continue;
    }

    // String literals
    if (*ptr == '"') {
      col = (uint32_t)(ptr - line) + 1;
      ptr++;
      start = ptr;
      while (ptr < end && *ptr != '"') {
        if (*ptr == '\\' && ptr + 1 < end)
          ptr++;
        ptr++;
      }
      if (ptr >= end || *ptr != '"') {
        LOG_ERROR_LOCATION("Expected closing quote", line_number, col);
        goto fail;
      }

      size_t unescaped_len;
      char* unescaped = unescape_string(start, ptr - start, &unescaped_len);
      if (!unescaped) {
        LOG_ERROR_LOCATION("Invalid escape character '\\%c'", line_number, col + 1 + (int)unescaped_len,
                           start[unescaped_len]);
        goto fail;
      }

      Token* tok = create_token(arena, TOKEN_STRING, unescaped, unescaped_len, 0, line_number, col);
      free(unescaped);
      ptr++;
      if (!tok) {
        LOG_ERROR_LOCATION("Failed to create token", line_number, col);
        goto fail;
      }
      append_token(&head, &tail, tok);
      continue;
    }

    start = ptr;
    col = (uint32_t)(ptr - line) + 1;
    while (ptr < end && *ptr != ' ' && *ptr != '\t' && *ptr != ',' && *ptr != '(' && *ptr != ')' && *ptr != '#')
      ptr++;
    size_t len = ptr - start;
    if (len == 0)
      continue;

    int out = 0;
    int out2 = 0;
    TokenType type = classify_token(start, len, &out, &out2);

    if (type == TOKEN_UNKNOWN) {
      LOG_ERROR_LOCATION("Unrecognized token '%.*s'", line_number, col, (int)len, start);
      goto fail;
    }

    size_t token_len = len;
    if (type == TOKEN_LABEL && start[len - 1] == ':')
      token_len = len - 1;

    Token* token = NULL;
    if (type == TOKEN_MNEMONIC || type == TOKEN_PSEUDO || type == TOKEN_INSTRUCTION_AMBIGUOUS) {
      token =
          create_token_instruction(arena, type, start, token_len, (Opcode)out, (PseudoOpcode)out2, line_number, col);
    } else {
      token = create_token(arena, type, start, token_len, out, line_number, col);
    }

    if (!token) {
      LOG_ERROR_LOCATION("Failed to create token", line_number, col);
      goto fail;
    }
    append_token(&head, &tail, token);
  }

#undef COL

  if (had_error)
    *had_error = 0;
  return head;

fail:
  free_token_list(head);
  if (had_error)
    *had_error = 1;
  return NULL;
}

TokenizedInput* tokenize_input(const char* src) {
  size_t src_len = strlen(src);

  TokenizedInput* result = malloc(sizeof(TokenizedInput));
  if (!result)
    return NULL;

  if (!arena_init(&result->arena, 2 * src_len + 1)) {
    free(result);
    return NULL;
  }

  DynamicArray lines = {0};
  if (!dynamic_array_init(&lines, sizeof(Token*), 256)) {
    arena_free(&result->arena);
    free(result);
    return NULL;
  }

  const char* line_start = src;
  const char* cursor = src;
  uint32_t line_num = 1;
  int line_had_error = 0;

  Token* tokens;
  while (cursor != NULL && *cursor != '\0') {
    if (*cursor == '\n') {
      size_t len = (size_t)(cursor - line_start);
      if (len > 0) {
        tokens = tokenize_line(&result->arena, line_start, len, line_num, &line_had_error);
        if (line_had_error)
          goto fail;
        if (tokens && !dynamic_array_push(&lines, &tokens))
          goto fail;
      }
      line_num++;
      line_start = cursor + 1;
    }
    cursor++;
  }

  if (cursor != line_start) {
    size_t len = (size_t)(cursor - line_start);
    if (len > 0) {
      tokens = tokenize_line(&result->arena, line_start, len, line_num, &line_had_error);
      if (line_had_error)
        goto fail;
      if (tokens && !dynamic_array_push(&lines, &tokens))
        goto fail;
    }
  }

  result->lines = (Token**)lines.data;
  result->count = lines.count;
  return result;

fail:
  for (size_t i = 0; i < lines.count; i++)
    free_token_list(((Token**)lines.data)[i]);
  dynamic_array_free(&lines);
  arena_free(&result->arena);
  free(result);
  return NULL;
}

void free_tokenized_input(TokenizedInput* tokenized) {
  if (!tokenized)
    return;

  for (size_t i = 0; i < tokenized->count; i++)
    free_token_list(tokenized->lines[i]);

  free(tokenized->lines);
  arena_free(&tokenized->arena);
  free(tokenized);
}