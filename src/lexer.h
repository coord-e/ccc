#ifndef CCC_TOKEN_H
#define CCC_TOKEN_H

#include <stdio.h>
#include "list.h"

typedef enum {
  TK_PLUS,
  TK_MINUS,
  TK_STAR,
  TK_SLASH,
  TK_DOUBLE_PLUS,
  TK_DOUBLE_MINUS,
  TK_AND,
  TK_VERTICAL,
  TK_DOUBLE_AND,
  TK_DOUBLE_VERTICAL,
  TK_PERCENT,
  TK_DOT,
  TK_HAT,
  TK_LEFT,    // <<
  TK_RIGHT,   // >>
  TK_LPAREN,  // ()
  TK_RPAREN,
  TK_LBRACE,  // {}
  TK_RBRACE,
  TK_LBRACKET,  // []
  TK_RBRACKET,
  TK_QUESTION,
  TK_COLON,
  TK_STAR_EQUAL,
  TK_SLASH_EQUAL,
  TK_PERCENT_EQUAL,
  TK_PLUS_EQUAL,
  TK_MINUS_EQUAL,
  TK_LEFT_EQUAL,
  TK_RIGHT_EQUAL,
  TK_AND_EQUAL,
  TK_HAT_EQUAL,
  TK_VERTICAL_EQUAL,
  TK_EQUAL,  // =
  TK_EQ,     // ==
  TK_NE,
  TK_GT,
  TK_LT,
  TK_GE,
  TK_LE,
  TK_EXCL,
  TK_TILDE,
  TK_SEMICOLON,
  TK_COMMA,
  TK_RETURN,
  TK_IF,
  TK_ELSE,
  TK_WHILE,
  TK_FOR,
  TK_DO,
  TK_BREAK,
  TK_CONTINUE,
  TK_INT,
  TK_CHAR,
  TK_LONG,
  TK_SHORT,
  TK_VOID,
  TK_SIGNED,
  TK_UNSIGNED,
  TK_BOOL,  // _Bool
  TK_STRUCT,
  TK_SIZEOF,
  TK_SWITCH,
  TK_GOTO,
  TK_CASE,
  TK_DEFAULT,
  TK_IDENT,   // identifiers
  TK_NUMBER,  // numbers
  TK_STRING,  // strings
  TK_END,     // end of tokens
} TokenKind;

typedef struct Token Token;

struct Token {
  TokenKind kind;
  char* ident;    // for TK_IDENT, owned
  char* string;   // for TK_STRING, owned
  size_t length;  // for TK_STRING
  int number;     // for TK_NUMBER
};

DECLARE_LIST(Token, TokenList)
DECLARE_LIST_PRINTER(TokenList)

// divide `input` into a linked list of `Token`s.
TokenList* tokenize(char* input);

#endif
