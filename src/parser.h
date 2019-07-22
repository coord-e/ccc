#ifndef CCC_PARSER_H
#define CCC_PARSER_H

#include <stdio.h>

#include "binop.h"
#include "lexer.h"

typedef enum {
  ND_BINOP,
  ND_ASSIGN,
  ND_VAR,
  ND_NUM,
} NodeKind;

typedef struct Node Node;

// the AST
struct Node {
  NodeKind kind;
  Node* lhs;
  Node* rhs;
  BinopKind binop;  // for ND_BINOP
  char* var;        // for ND_VAR, owned
  int num;          // for ND_NUM
};

// parse a list of tokens into AST.
Node* parse(TokenList* tokens);

#endif
