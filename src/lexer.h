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
  TK_EQUAL,  // =
  TK_EQ,     // ==
  TK_NE,
  TK_GT,
  TK_LT,
  TK_GE,
  TK_LE,
  TK_SEMICOLON,
  TK_RETURN,
  TK_IDENT,   // identifiers
  TK_NUMBER,  // numbers
  TK_END,     // end of tokens
} TokenKind;

typedef struct Token Token;

struct Token {
  TokenKind kind;
  char* ident;  // for TK_IDENT, owned
  int number;   // for TK_NUMBER
};

DECLARE_LIST(Token, TokenList)
DECLARE_LIST_PRINTER(TokenList)

// divide `input` into a linked list of `Token`s.
TokenList* tokenize(char* input);

#endif
