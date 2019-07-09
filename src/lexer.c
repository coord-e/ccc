#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "lexer.h"
#include "list.h"
#include "error.h"

DEFINE_LIST(Token, TokenList)
DEFINE_LIST_PRINTER(print_token, TokenList)

TokenList* add_token(TokenKind kind, TokenList *cur) {
  Token t;
  t.kind = kind;
  return scons_TokenList(t, cur);
}

// will seek `strp` to the end of number
TokenList* add_number(char** strp, TokenList* cur) {
  TokenList* t = add_token(TK_NUMBER, cur);
  t->head.number = strtol(*strp, strp, 10);
  return t;
}

void end_tokens(TokenList* cur) {
  add_token(TK_END, cur);
}

TokenList* tokenize(char* p) {
  TokenList* init = nil_TokenList();
  TokenList* cur = init;

  while (*p) {
    if (isspace(*p)) {
      // skip spaces
      p++;
      continue;
    }

    switch (*p) {
      case '+':
        cur = add_token(TK_PLUS, cur);
        p++;
        continue;
      case '-':
        cur = add_token(TK_MINUS, cur);
        p++;
        continue;
      case '*':
        cur = add_token(TK_STAR, cur);
        p++;
        continue;
      case '/':
        cur = add_token(TK_SLASH, cur);
        p++;
        continue;
      case '(':
        cur = add_token(TK_LPAREN, cur);
        p++;
        continue;
      case ')':
        cur = add_token(TK_RPAREN, cur);
        p++;
        continue;
      default:
        if (isdigit(*p)) {
          // call of `new_number` updates p
          cur = add_number(&p, cur);
          continue;
        }
    }

    error("Unexpected charcter: %c", *p);
  }

  end_tokens(cur);
  return init;
}

void print_token(FILE* p, Token t) {
  switch(t.kind) {
    case TK_PLUS:
      fprintf(p, "(+), ");
      break;
    case TK_MINUS:
      fprintf(p, "(-), ");
      break;
    case TK_STAR:
      fprintf(p, "(*), ");
      break;
    case TK_SLASH:
      fprintf(p, "(/), ");
      break;
    case TK_LPAREN:
      fprintf(p, "((), ");
      break;
    case TK_RPAREN:
      fprintf(p, "()), ");
      break;
    case TK_NUMBER:
      fprintf(p, "num(%d), ", t.number);
      break;
    case TK_END:
      fprintf(p, "end");
      return;
    default:
      CCC_UNREACHABLE;
  }
}
