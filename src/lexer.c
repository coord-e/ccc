#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "lexer.h"
#include "list.h"
#include "util.h"

static void release_token(Token t) {
  free(t.ident);
}
DEFINE_LIST(release_token, Token, TokenList)

static TokenList* add_token(TokenKind kind, TokenList* cur) {
  Token t;
  t.ident = NULL;
  t.kind  = kind;
  return snoc_TokenList(t, cur);
}

// will seek `strp` to the end of number
static TokenList* add_number(char** strp, TokenList* cur) {
  TokenList* t   = add_token(TK_NUMBER, cur);
  t->head.number = strtol(*strp, strp, 10);
  return t;
}

static bool is_ident_char(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || (c == '_');
}

// will seek `strp` to the end of ident
static TokenList* add_ident(char** strp, TokenList* cur) {
  char* init = *strp;
  while (is_ident_char(**strp)) {
    (*strp)++;
  }

#define IS_SAME(sv, sc) (memcmp(sv, sc, sizeof(sc) - 1) == 0)
  if (IS_SAME(init, "return")) {
    return add_token(TK_RETURN, cur);
  } else if (IS_SAME(init, "if")) {
    return add_token(TK_IF, cur);
  } else if (IS_SAME(init, "else")) {
    return add_token(TK_ELSE, cur);
  } else if (IS_SAME(init, "while")) {
    return add_token(TK_WHILE, cur);
  } else if (IS_SAME(init, "for")) {
    return add_token(TK_FOR, cur);
  } else if (IS_SAME(init, "do")) {
    return add_token(TK_DO, cur);
  }
#undef IS_SAME

  TokenList* t  = add_token(TK_IDENT, cur);
  t->head.ident = strndup(init, *strp - init);
  return t;
}

static void end_tokens(TokenList* cur) {
  add_token(TK_END, cur);
}

TokenList* tokenize(char* p) {
  TokenList* init = nil_TokenList();
  TokenList* cur  = init;

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
      case '{':
        cur = add_token(TK_LBRACE, cur);
        p++;
        continue;
      case '}':
        cur = add_token(TK_RBRACE, cur);
        p++;
        continue;
      case ';':
        cur = add_token(TK_SEMICOLON, cur);
        p++;
        continue;
      case '=':
        p++;
        switch (*p) {
          case '=':
            cur = add_token(TK_EQ, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_EQUAL, cur);
            continue;
        }
      case '!':
        p++;
        switch (*p) {
          case '=':
            cur = add_token(TK_NE, cur);
            p++;
            continue;
        }
      case '>':
        p++;
        switch (*p) {
          case '=':
            cur = add_token(TK_GE, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_GT, cur);
            continue;
        }
      case '<':
        p++;
        switch (*p) {
          case '=':
            cur = add_token(TK_LE, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_LT, cur);
            continue;
        }
      default:
        if (isdigit(*p)) {
          // call of `add_number` updates p
          cur = add_number(&p, cur);
          continue;
        } else if (isalpha(*p)) {
          // call of `add_ident` updates p
          cur = add_ident(&p, cur);
          continue;
        }
    }

    error("Unexpected charcter: %c", *p);
  }

  end_tokens(cur);
  return init;
}

static void print_token(FILE* p, Token t) {
  switch (t.kind) {
    case TK_PLUS:
      fprintf(p, "(+)");
      break;
    case TK_MINUS:
      fprintf(p, "(-)");
      break;
    case TK_STAR:
      fprintf(p, "(*)");
      break;
    case TK_SLASH:
      fprintf(p, "(/)");
      break;
    case TK_LPAREN:
      fprintf(p, "(()");
      break;
    case TK_RPAREN:
      fprintf(p, "())");
      break;
    case TK_LBRACE:
      fprintf(p, "({)");
      break;
    case TK_RBRACE:
      fprintf(p, "(})");
      break;
    case TK_EQUAL:
      fprintf(p, "(=)");
      break;
    case TK_EQ:
      fprintf(p, "(==)");
      break;
    case TK_NE:
      fprintf(p, "(!=)");
      break;
    case TK_GT:
      fprintf(p, "(>)");
      break;
    case TK_GE:
      fprintf(p, "(>=)");
      break;
    case TK_LT:
      fprintf(p, "(<)");
      break;
    case TK_LE:
      fprintf(p, "(<=)");
      break;
    case TK_SEMICOLON:
      fprintf(p, "(;)");
      break;
    case TK_RETURN:
      fprintf(p, "(return)");
      break;
    case TK_IF:
      fprintf(p, "(if)");
      break;
    case TK_ELSE:
      fprintf(p, "(else)");
      break;
    case TK_WHILE:
      fprintf(p, "(while)");
      break;
    case TK_DO:
      fprintf(p, "(do)");
      break;
    case TK_FOR:
      fprintf(p, "(for)");
      break;
    case TK_NUMBER:
      fprintf(p, "num(%d)", t.number);
      break;
    case TK_END:
      fprintf(p, "end");
      return;
    case TK_IDENT:
      fprintf(p, "ident(%s)", t.ident);
      return;
    default:
      CCC_UNREACHABLE;
  }
}

DEFINE_LIST_PRINTER(print_token, ", ", "\n", TokenList)
