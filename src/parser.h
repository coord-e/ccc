#ifndef CCC_PARSER_H
#define CCC_PARSER_H

#include <stdio.h>

#include "lexer.h"
#include "binop.h"

typedef enum {
  ND_BINOP,
  ND_NUM,
} NodeKind;

typedef struct Node Node;

// the AST
struct Node {
  NodeKind kind;
  Node *lhs;
  Node *rhs;
  BinopKind binop;  // for ND_BINOP
  int num;          // for ND_NUM
};

// parse a list of tokens into AST.
Node* parse(Token* tokens);

// print a AST for debugging purpose
void print_tree(FILE*, Node* tree);

#endif
