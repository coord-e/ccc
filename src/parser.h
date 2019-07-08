#ifndef CCC_PARSER_H
#define CCC_PARSER_H

typedef enum {
  ND_ADD,
  ND_SUB,
  ND_MUL,
  ND_DIV,
  ND_NUM,
} NodeKind;

typedef struct Node Node;

// the AST
struct Node {
  NodeKind kind;
  Node *lhs;
  Node *rhs;
  int num;  // for ND_NUM
};

// parse a list of tokens into AST.
Node* parse(Token* tokens);

#endif
