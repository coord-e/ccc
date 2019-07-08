#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "lexer.h"
#include "error.h"

Token *new_token(TokenKind kind, Token *cur) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  cur->next = tok;
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
        cur = new_token(TK_PLUS, cur);
        p++;
        continue;
      case '-':
        cur = new_token(TK_MINUS, cur);
        p++;
        continue;
      case '*':
        cur = new_token(TK_STAR, cur);
        p++;
        continue;
      case '/':
        cur = new_token(TK_SLASH, cur);
        p++;
        continue;
      case '(':
        cur = new_token(TK_LPAREN, cur);
        p++;
        continue;
      case ')':
        cur = new_token(TK_RPAREN, cur);
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

void print_tokens(Token* t) {
  switch(t->kind) {
    case TK_PLUS:
      printf("(+), ");
      break;
    case TK_MINUS:
      printf("(-), ");
      break;
    case TK_STAR:
      printf("(*), ");
      break;
    case TK_SLASH:
      printf("(/), ");
      break;
    case TK_LPAREN:
      printf("((), ");
      break;
    case TK_RPAREN:
      printf("()), ");
      break;
    case TK_NUMBER:
      printf("num(%d), ", t->number);
      break;
    case TK_END:
      printf("end\n");
      return;
  }
  print_tokens(t->next);
}
