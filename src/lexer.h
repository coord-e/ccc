#ifndef CCC_TOKEN_H
#define CCC_TOKEN_H

typedef enum {
  TK_BINOP,   // binary operators
  TK_NUMBER,  // numbers
  TK_END,     // end of tokens
} TokenKind;

typedef enum {
  BINOP_PLUS,
  BINOP_MINUS,
} BinopKind;

typedef struct Token Token;

struct Token {
  TokenKind kind;
  Token *next;
  int number;        // for TK_NUMBER
  BinopKind binop;  // for TK_BINOP
};

Token* tokenize(char*);

#endif
