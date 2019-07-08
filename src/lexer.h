#ifndef CCC_TOKEN_H
#define CCC_TOKEN_H

typedef enum {
  TK_PLUS,
  TK_MINUS,
  TK_NUMBER,  // numbers
  TK_END,     // end of tokens
} TokenKind;

typedef struct Token Token;

struct Token {
  TokenKind kind;
  Token *next;
  int number;        // for TK_NUMBER
};

// divide `input` into a linked list of `Token`s.
Token* tokenize(char* input);

// print a list of tokens returned from `tokenize` for debugging purpose.
void print_tokens(Token* tokens);

#endif
