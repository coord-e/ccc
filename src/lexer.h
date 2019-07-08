#ifndef CCC_TOKEN_H
#define CCC_TOKEN_H

#include "binop.h"

typedef enum {
  TK_BINOP,   // binary operators
  TK_NUMBER,  // numbers
  TK_END,     // end of tokens
} TokenKind;

typedef struct Token Token;

struct Token {
  TokenKind kind;
  Token *next;
  int number;        // for TK_NUMBER
  BinopKind binop;  // for TK_BINOP
};

// divide `input` into a linked list of `Token`s.
Token* tokenize(char* input);

// print a list of tokens returned from `tokenize` for debugging purpose.
void print_tokens(Token* tokens);

#endif
