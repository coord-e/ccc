#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "parser.h"
#include "util.h"

// utilities to build AST
static Node* new_node(NodeKind kind, Node* lhs, Node* rhs) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs  = lhs;
  node->rhs  = rhs;
  node->var  = NULL;
  return node;
}

static Node* new_node_num(int num) {
  Node* node = new_node(ND_NUM, NULL, NULL);
  node->num  = num;
  return node;
}

static Node* new_node_var(char* ident) {
  Node* node = new_node(ND_VAR, NULL, NULL);
  node->var  = strdup(ident);
  return node;
}

static Node* new_node_binop(BinopKind kind, Node* lhs, Node* rhs) {
  Node* node  = new_node(ND_BINOP, lhs, rhs);
  node->binop = kind;
  return node;
}

static void consume(TokenList** t) {
  *t = tail_TokenList(*t);
}

static Token consuming(TokenList** t) {
  TokenList* p = *t;
  consume(t);
  return head_TokenList(p);
}

static TokenKind head_of(TokenList** t) {
  return head_TokenList(*t).kind;
}

static Node* expr(TokenList** t);

static Node* term(TokenList** t) {
  if (head_of(t) == TK_LPAREN) {
    consume(t);
    Node* node = expr(t);
    if (head_of(t) == TK_RPAREN) {
      consume(t);
      return node;
    } else {
      error("unmatched parentheses.");
    }
  } else {
    if (head_of(t) == TK_NUMBER) {
      return new_node_num(consuming(t).number);
    } else if (head_of(t) == TK_IDENT) {
      return new_node_var(consuming(t).ident);
    } else {
      error("unexpected token.");
    }
  }
}

static Node* unary(TokenList** t) {
  switch (head_of(t)) {
    case TK_PLUS:
      consume(t);
      // parse `+n` as `n`
      return term(t);
    case TK_MINUS:
      consume(t);
      // parse `-n` as `0 - n`
      return new_node_binop(BINOP_SUB, new_node_num(0), term(t));
    default:
      return term(t);
  }
}

static Node* mul(TokenList** t) {
  Node* node = unary(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_STAR:
        consume(t);
        node = new_node_binop(BINOP_MUL, node, unary(t));
        break;
      case TK_SLASH:
        consume(t);
        node = new_node_binop(BINOP_DIV, node, unary(t));
        break;
      default:
        return node;
    }
  }
}

static Node* add(TokenList** t) {
  Node* node = mul(t);

  for (;;) {
    switch (head_of(t)) {
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

static Node* relational(TokenList** t) {
  Node* node = add(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_GT:
        consume(t);
        node = new_node_binop(BINOP_GT, node, add(t));
        break;
      case TK_GE:
        consume(t);
        node = new_node_binop(BINOP_GE, node, add(t));
        break;
      case TK_LT:
        consume(t);
        node = new_node_binop(BINOP_LT, node, add(t));
        break;
      case TK_LE:
        consume(t);
        node = new_node_binop(BINOP_LE, node, add(t));
        break;
      default:
        return node;
    }
  }
}

static Node* equality(TokenList** t) {
  Node* node = relational(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_EQ:
        consume(t);
        node = new_node_binop(BINOP_EQ, node, relational(t));
        break;
      case TK_NE:
        consume(t);
        node = new_node_binop(BINOP_NE, node, relational(t));
        break;
      default:
        return node;
    }
  }
}

// the expression parser
static Node* expr(TokenList** t) {
  return equality(t);
}

// parse tokens into AST
// currently this parses expressions
Node* parse(TokenList* t) {
  return expr(&t);
}

void print_binop(FILE* p, BinopKind kind) {
  switch (kind) {
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
    case BINOP_EQ:
      fprintf(p, "==");
      return;
    case BINOP_NE:
      fprintf(p, "!=");
      return;
    case BINOP_GT:
      fprintf(p, ">");
      return;
    case BINOP_GE:
      fprintf(p, ">=");
      return;
    case BINOP_LT:
      fprintf(p, "<");
      return;
    case BINOP_LE:
      fprintf(p, "<=");
      return;
    default:
      CCC_UNREACHABLE;
  }
}

static void print_tree_(FILE* p, Node* node) {
  switch (node->kind) {
    case ND_NUM:
      fprintf(p, "%d", node->num);
      return;
    case ND_VAR:
      fprintf(p, "%s", node->var);
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

void release_tree(Node* node) {
  if (node == NULL) {
    return;
  }

  release_tree(node->lhs);
  release_tree(node->rhs);
  free(node->var);
  free(node);
}
