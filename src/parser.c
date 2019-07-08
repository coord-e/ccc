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
  *t = (*t)->next;
}

Token* consuming(Token** t) {
  Token* p = *t;
  consume(t);
  return p;
}

Node* expr(Token** t);

Node* term(Token** t) {
  if ((*t)->kind == TK_LPAREN) {
    consume(t);
    Node* node = expr(t);
    if ((*t)->kind == TK_RPAREN) {
      consume(t);
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
    switch ((*t)->kind) {
    case TK_STAR:
      consume(t);
      node = new_node_binop(BINOP_MUL, node, term(t));
      break;
    case TK_SLASH:
      consume(t);
      node = new_node_binop(BINOP_DIV, node, term(t));
      break;
    default:
      return node;
    }
  }
}

Node* add(Token** t) {
  Node* node = mul(t);

  for (;;) {
    switch ((*t)->kind) {
      case TK_PLUS:
        consume(t);
        node = new_node_binop(BINOP_ADD, node, mul(t));
        break;
      case TK_MINUS:
        consume(t);
        node = new_node_binop(BINOP_SUB, node, mul(t));
        break;
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

void print_binop(FILE* p, BinopKind kind) {
  switch(kind) {
    case BINOP_ADD:
      fprintf(p, "+");
      return;
    case BINOP_SUB:
      fprintf(p, "-");
      return;
    case BINOP_MUL:
      fprintf(p, "*");
      return;
    case BINOP_DIV:
      fprintf(p, "/");
      return;
    default:
      CCC_UNREACHABLE;
  }
}

void print_tree_(FILE* p, Node* node) {
  switch(node->kind) {
    case ND_NUM:
      fprintf(p, "%d", node->num);
      return;
    case ND_BINOP:
      fprintf(p, "(");
      print_binop(p, node->binop);
      fprintf(p, " ");
      print_tree_(p, node->lhs);
      fprintf(p, " ");
      print_tree_(p, node->rhs);
      fprintf(p, ")");
      return;
    default:
      CCC_UNREACHABLE;
  }
}

void print_tree(FILE* p, Node* node) {
  print_tree_(p, node);
  fprintf(p, "\n");
}
