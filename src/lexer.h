#ifndef CCC_TOKEN_H
#define CCC_TOKEN_H

#include <stdio.h>
#include "list.h"

typedef enum {
  TK_PLUS,
  TK_MINUS,
  TK_STAR,
  TK_SLASH,
  TK_LPAREN,
  TK_RPAREN,
  TK_NUMBER,  // numbers
  TK_END,     // end of tokens
} TokenKind;

typedef struct Token Token;

struct Token {
  TokenKind kind;
  int number;        // for TK_NUMBER
};

void print_token(FILE*, Token);

DEFINE_LIST(Token, TokenList)
DEFINE_LIST_PRINTER(print_token, TokenList)

// divide `input` into a linked list of `Token`s.
TokenList* tokenize(char* input);

#endif
