#ifndef CCC_PARSER_H
#define CCC_PARSER_H

#include <stdio.h>

#include "binop.h"
#include "lexer.h"

typedef enum {
  ND_BINOP,
  ND_NUM,
} NodeKind;

typedef struct Node Node;

// the AST
struct Node {
  NodeKind kind;
  Node* lhs;
  Node* rhs;
  BinopKind binop;  // for ND_BINOP
  int num;          // for ND_NUM
};

// parse a list of tokens into AST.
Node* parse(TokenList* tokens);

// print a AST for debugging purpose
void print_tree(FILE*, Node* tree);
void print_binop(FILE*, BinopKind kind);

// free the memory space used in AST
void release_tree(Node*);

#endif
