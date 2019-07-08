#include "parser.h"

// utilities to build AST
Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int num) {
  Node *node = new_node(ND_NUM, NULL, NULL);
  node->num = num;
  return node;
}

Node *new_node_binop(BinopKind kind, Node *lhs, Node *rhs) {
  Node* node = new_node(ND_BINOP, lhs, rhs);
  node->binop = kind;
  return node;
}

Node* term(Token** t) {
  if (tk->kind == TK_LPAREN) {
    Node* node = expr(t);
    if (tk->kind == TK_RPAREN) {
      return node;
    } else {
      error("unmatched parentheses.");
    }
  } else {
    if (tk->kind == TK_NUMBER) {
      return new_node_num(tk->number);
    } else {
      error("unexpected token.");
    }
  }
}

Node* mul(Token** t) {
  Node* node = term(t);

  // immutable version of `t`
  Token* tk = *t;

  for (;;) {
    switch (tk->kind) {
    case TK_STAR:
      node = new_node_binop(BINOP_MUL, node, term(t));
    case TK_SLASH:
      node = new_node_binop(BINOP_DIV, node, term(t));
    default:
      return node;
    }
  }
}

Node* add(Token** t) {
  Node* node = mul(t);

  // immutable version of `t`
  Token* tk = *t;

  for (;;) {
    switch (tk->kind) {
      case TK_PLUS:
        node = new_node_binop(BINOP_ADD, node, mul(t));
      case TK_MINUS:
        node = new_node_binop(BINOP_SUB, node, mul(t));
      default:
        return node;
    }
  }
}

// the expression parser
Node* expr(Token** t) {
  return add(t);
}

// parse tokens into AST
// currently this parses expressions
Node* parse(Token* t) {
  return expr(t);
}
