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
  free(t.string);
}
DEFINE_LIST(release_token, Token, TokenList)

static TokenList* add_token(TokenKind kind, TokenList* cur) {
  Token t;
  t.ident  = NULL;
  t.string = NULL;
  t.kind   = kind;
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
  } else if (IS_SAME(init, "break")) {
    return add_token(TK_BREAK, cur);
  } else if (IS_SAME(init, "continue")) {
    return add_token(TK_CONTINUE, cur);
  } else if (IS_SAME(init, "int")) {
    return add_token(TK_INT, cur);
  } else if (IS_SAME(init, "char")) {
    return add_token(TK_CHAR, cur);
  } else if (IS_SAME(init, "long")) {
    return add_token(TK_LONG, cur);
  } else if (IS_SAME(init, "short")) {
    return add_token(TK_SHORT, cur);
  } else if (IS_SAME(init, "void")) {
    return add_token(TK_VOID, cur);
  } else if (IS_SAME(init, "signed")) {
    return add_token(TK_SIGNED, cur);
  } else if (IS_SAME(init, "unsigned")) {
    return add_token(TK_UNSIGNED, cur);
  } else if (IS_SAME(init, "_Bool")) {
    return add_token(TK_BOOL, cur);
  } else if (IS_SAME(init, "sizeof")) {
    return add_token(TK_SIZEOF, cur);
  } else if (IS_SAME(init, "switch")) {
    return add_token(TK_SWITCH, cur);
  } else if (IS_SAME(init, "goto")) {
    return add_token(TK_GOTO, cur);
  } else if (IS_SAME(init, "case")) {
    return add_token(TK_CASE, cur);
  } else if (IS_SAME(init, "default")) {
    return add_token(TK_DEFAULT, cur);
  } else if (IS_SAME(init, "struct")) {
    return add_token(TK_STRUCT, cur);
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

    if (memcmp(p, "//", 2) == 0) {
      p += 2;
      // newline is also consumed
      while (*(p++) != '\n')
        ;
      continue;
    }

    if (memcmp(p, "/*", 2) == 0) {
      p += 2;
      while (memcmp(p, "*/", 2) != 0) {
        p++;
      }
      p += 2;
      continue;
    }

    switch (*p) {
      case '.':
        cur = add_token(TK_DOT, cur);
        p++;
        continue;
      case '+':
        p++;
        switch (*p) {
          case '+':
            cur = add_token(TK_DOUBLE_PLUS, cur);
            p++;
            continue;
          case '=':
            cur = add_token(TK_PLUS_EQUAL, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_PLUS, cur);
            continue;
        }
      case '-':
        p++;
        switch (*p) {
          case '-':
            cur = add_token(TK_DOUBLE_MINUS, cur);
            p++;
            continue;
          case '>':
            cur = add_token(TK_ARROW, cur);
            p++;
            continue;
          case '=':
            cur = add_token(TK_MINUS_EQUAL, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_MINUS, cur);
            continue;
        }
      case '*':
        p++;
        switch (*p) {
          case '=':
            cur = add_token(TK_STAR_EQUAL, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_STAR, cur);
            continue;
        }
      case '/':
        p++;
        switch (*p) {
          case '=':
            cur = add_token(TK_SLASH_EQUAL, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_SLASH, cur);
            continue;
        }
      case '~':
        cur = add_token(TK_TILDE, cur);
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
      case '[':
        cur = add_token(TK_LBRACKET, cur);
        p++;
        continue;
      case ']':
        cur = add_token(TK_RBRACKET, cur);
        p++;
        continue;
      case ';':
        cur = add_token(TK_SEMICOLON, cur);
        p++;
        continue;
      case ',':
        cur = add_token(TK_COMMA, cur);
        p++;
        continue;
      case '"':
        p++;
        char* init = p;
        while (*p != '"')
          p++;
        cur              = add_token(TK_STRING, cur);
        cur->head.string = strndup(init, p - init);
        cur->head.length = p - init;
        p++;
        continue;
      case '^':
        p++;
        switch (*p) {
          case '=':
            cur = add_token(TK_HAT_EQUAL, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_HAT, cur);
            continue;
        }
      case '%':
        p++;
        switch (*p) {
          case '=':
            cur = add_token(TK_PERCENT_EQUAL, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_PERCENT, cur);
            continue;
        }
      case '&':
        p++;
        switch (*p) {
          case '&':
            cur = add_token(TK_DOUBLE_AND, cur);
            p++;
            continue;
          case '=':
            cur = add_token(TK_AND_EQUAL, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_AND, cur);
            continue;
        }
      case '|':
        p++;
        switch (*p) {
          case '|':
            cur = add_token(TK_DOUBLE_VERTICAL, cur);
            p++;
            continue;
          case '=':
            cur = add_token(TK_VERTICAL_EQUAL, cur);
            p++;
            continue;
          default:
            cur = add_token(TK_VERTICAL, cur);
            continue;
        }
      case ':':
        cur = add_token(TK_COLON, cur);
        p++;
        continue;
      case '?':
        cur = add_token(TK_QUESTION, cur);
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
          default:
            cur = add_token(TK_EXCL, cur);
            continue;
        }
      case '>':
        p++;
        switch (*p) {
          case '=':
            cur = add_token(TK_GE, cur);
            p++;
            continue;
          case '>':
            p++;
            switch (*p) {
              case '=':
                cur = add_token(TK_RIGHT_EQUAL, cur);
                p++;
                continue;
              default:
                cur = add_token(TK_RIGHT, cur);
                continue;
            }
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
          case '<':
            p++;
            switch (*p) {
              case '=':
                cur = add_token(TK_LEFT_EQUAL, cur);
                p++;
                continue;
              default:
                cur = add_token(TK_LEFT, cur);
                continue;
            }
          default:
            cur = add_token(TK_LT, cur);
            continue;
        }
      default:
        if (isdigit(*p)) {
          // call of `add_number` updates p
          cur = add_number(&p, cur);
          continue;
        } else if (is_ident_char(*p)) {
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
    case TK_DOUBLE_PLUS:
      fprintf(p, "(++)");
      break;
    case TK_DOUBLE_MINUS:
      fprintf(p, "(--)");
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
    case TK_LBRACKET:
      fprintf(p, "([)");
      break;
    case TK_RBRACKET:
      fprintf(p, "(])");
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
    case TK_EXCL:
      fprintf(p, "(!)");
      break;
    case TK_TILDE:
      fprintf(p, "(~)");
      break;
    case TK_SEMICOLON:
      fprintf(p, "(;)");
      break;
    case TK_COMMA:
      fprintf(p, "(,)");
      break;
    case TK_AND:
      fprintf(p, "(&)");
      break;
    case TK_VERTICAL:
      fprintf(p, "(|)");
      break;
    case TK_DOUBLE_AND:
      fprintf(p, "(&&)");
      break;
    case TK_DOUBLE_VERTICAL:
      fprintf(p, "(||)");
      break;
    case TK_HAT:
      fprintf(p, "(^)");
      break;
    case TK_LEFT:
      fprintf(p, "(<<)");
      break;
    case TK_RIGHT:
      fprintf(p, "(>>)");
      break;
    case TK_COLON:
      fprintf(p, "(:)");
      break;
    case TK_QUESTION:
      fprintf(p, "(?)");
      break;
    case TK_PERCENT:
      fprintf(p, "(%%)");
      break;
    case TK_STAR_EQUAL:
      fprintf(p, "(*=)");
      break;
    case TK_SLASH_EQUAL:
      fprintf(p, "(/=)");
      break;
    case TK_PERCENT_EQUAL:
      fprintf(p, "(%%=)");
      break;
    case TK_PLUS_EQUAL:
      fprintf(p, "(+=)");
      break;
    case TK_MINUS_EQUAL:
      fprintf(p, "(-=)");
      break;
    case TK_LEFT_EQUAL:
      fprintf(p, "(<<=)");
      break;
    case TK_RIGHT_EQUAL:
      fprintf(p, "(>>=)");
      break;
    case TK_AND_EQUAL:
      fprintf(p, "(&=)");
      break;
    case TK_HAT_EQUAL:
      fprintf(p, "(^=)");
      break;
    case TK_VERTICAL_EQUAL:
      fprintf(p, "(|=)");
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
    case TK_BREAK:
      fprintf(p, "(break)");
      break;
    case TK_CONTINUE:
      fprintf(p, "(CONTINUE)");
      break;
    case TK_INT:
      fprintf(p, "(INT)");
      break;
    case TK_CHAR:
      fprintf(p, "(CHAR)");
      break;
    case TK_LONG:
      fprintf(p, "(LONG)");
      break;
    case TK_SHORT:
      fprintf(p, "(SHORT)");
      break;
    case TK_VOID:
      fprintf(p, "(VOID)");
      break;
    case TK_SIGNED:
      fprintf(p, "(SIGNED)");
      break;
    case TK_UNSIGNED:
      fprintf(p, "(UNSIGNED)");
      break;
    case TK_BOOL:
      fprintf(p, "(BOOL)");
      break;
    case TK_SIZEOF:
      fprintf(p, "(SIZEOF)");
      break;
    case TK_STRUCT:
      fprintf(p, "(STRUCT)");
      break;
    case TK_SWITCH:
      fprintf(p, "(SWITCH)");
      break;
    case TK_GOTO:
      fprintf(p, "(GOTO)");
      break;
    case TK_CASE:
      fprintf(p, "(CASE)");
      break;
    case TK_DEFAULT:
      fprintf(p, "(DEFAULT)");
      break;
    case TK_DOT:
      fprintf(p, "(.)");
      break;
    case TK_ARROW:
      fprintf(p, "(->)");
      break;
    case TK_NUMBER:
      fprintf(p, "num(%d)", t.number);
      break;
    case TK_STRING:
      fprintf(p, "str(%s,%ld)", t.string, t.length);
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
