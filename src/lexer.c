#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "lexer.h"

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

Token *new_token(TokenKind kind, Token *cur) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  cur->next = tok;
  return tok;
}

Token* new_binop(BinopKind op, Token *cur) {
  Token* tok = new_token(TK_BINOP, cur);
  tok->binop = op;
  return tok;
}

Token* new_number(char** strp, Token *cur) {
  Token* tok = new_token(TK_NUMBER, cur);
  tok->number = strtol(*strp, strp, 10);
  return tok;
}

void end_tokens(Token* cur) {
  new_token(TK_END, cur);
}

Token* tokenize(char* p) {
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    if (isspace(*p)) {
      // skip spaces
      p++;
      continue;
    }

    switch (*p) {
      case '+':
        cur = new_binop(BINOP_PLUS, cur);
        p++;
        continue;
      case '-':
        cur = new_binop(BINOP_MINUS, cur);
        p++;
        continue;
      default:
        if (isdigit(*p)) {
          // call of `new_number` updates p
          cur = new_number(&p, cur);
          continue;
        }
    }

    error("Unexpected charcter: %c", *p);
  }

  end_tokens(cur);
  return head.next;
}

void print_binop(BinopKind kind) {
  switch(kind) {
    case BINOP_PLUS:
      printf("+");
      break;
    case BINOP_MINUS:
      printf("-");
      break;
  }
}

void print_tokens(Token* t) {
  switch(t->kind) {
    case TK_BINOP:
      printf("bop(");
      print_binop(t->binop);
      printf("), ");
      break;
    case TK_NUMBER:
      printf("num(%d), ", t->number);
      break;
    case TK_END:
      printf("end");
      return;
  }
  print_tokens(t->next);
}
