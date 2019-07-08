#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "parser.h"
#include "error.h"

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

void consume(Token** t) {
  t = &(*t)->next;
}

Token* consuming(Token** t) {
  Token* p = *t;
  consume(t);
  return p;
}

Node* expr(Token** t);

Node* term(Token** t) {
  if (consuming(t)->kind == TK_LPAREN) {
    Node* node = expr(t);
    if (consuming(t)->kind == TK_RPAREN) {
      return node;
    } else {
      error("unmatched parentheses.");
    }
  } else {
    if ((*t)->kind == TK_NUMBER) {
      return new_node_num(consuming(t)->number);
    } else {
      error("unexpected token.");
    }
  }
}

Node* mul(Token** t) {
  Node* node = term(t);

  for (;;) {
    switch (consuming(t)->kind) {
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

  for (;;) {
    switch (consuming(t)->kind) {
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
  return expr(&t);
}

void print_binop(BinopKind kind) {
  switch(kind) {
    case BINOP_ADD:
      printf("+");
      return;
    case BINOP_SUB:
      printf("-");
      return;
    case BINOP_MUL:
      printf("*");
      return;
    case BINOP_DIV:
      printf("/");
      return;
  }
}

void print_tree(Node* node) {
  switch(node->kind) {
    case ND_NUM:
      printf("%d", node->num);
      return;
    case ND_BINOP:
      printf("(");
      print_binop(node->binop);
      printf(" ");
      print_tree(node->lhs);
      printf(" ");
      print_tree(node->rhs);
      printf(")");
      return;
  }
}
