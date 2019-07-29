#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "parser.h"
#include "util.h"

// utilities to build AST
static Expr* new_node(ExprKind kind, Expr* lhs, Expr* rhs) {
  Expr* node = calloc(1, sizeof(Expr));
  node->kind = kind;
  node->lhs  = lhs;
  node->rhs  = rhs;
  node->var  = NULL;
  return node;
}

static Expr* new_node_num(int num) {
  Expr* node = new_node(ND_NUM, NULL, NULL);
  node->num  = num;
  return node;
}

static Expr* new_node_var(char* ident) {
  Expr* node = new_node(ND_VAR, NULL, NULL);
  node->var  = strdup(ident);
  return node;
}

static Expr* new_node_binop(BinopKind kind, Expr* lhs, Expr* rhs) {
  Expr* node  = new_node(ND_BINOP, lhs, rhs);
  node->binop = kind;
  return node;
}

static Expr* new_node_assign(Expr* lhs, Expr* rhs) {
  return new_node(ND_ASSIGN, lhs, rhs);
}

static Declaration* new_declaration(char* s) {
  Declaration* d = calloc(1, sizeof(Declaration));
  d->declarator  = strdup(s);
  return d;
}

static Statement* new_statement(StmtKind kind, Expr* expr) {
  Statement* s = calloc(1, sizeof(Statement));
  s->kind      = kind;
  s->expr      = expr;
  return s;
}

static BlockItem* new_block_item(BlockItemKind kind, Statement* stmt, Declaration* decl) {
  BlockItem* item = calloc(1, sizeof(BlockItem));
  item->kind      = kind;
  item->stmt      = stmt;
  item->decl      = decl;
  return item;
}

static void consume(TokenList** t) {
  *t = tail_TokenList(*t);
}

static Token consuming(TokenList** t) {
  TokenList* p = *t;
  consume(t);
  return head_TokenList(p);
}

static void expect(TokenList** t, TokenKind k) {
  if (consuming(t).kind != k) {
    error("unexpected token");
  }
}

static TokenKind head_of(TokenList** t) {
  return head_TokenList(*t).kind;
}

static Expr* expr(TokenList** t);

static Expr* term(TokenList** t) {
  if (head_of(t) == TK_LPAREN) {
    consume(t);
    Expr* node = expr(t);
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

static Expr* unary(TokenList** t) {
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

static Expr* mul(TokenList** t) {
  Expr* node = unary(t);

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

static Expr* add(TokenList** t) {
  Expr* node = mul(t);

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

static Expr* relational(TokenList** t) {
  Expr* node = add(t);

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

static Expr* equality(TokenList** t) {
  Expr* node = relational(t);

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

static Expr* assign(TokenList** t) {
  Expr* node = equality(t);

  // `=` has right associativity
  switch (head_of(t)) {
    case TK_EQUAL:
      consume(t);
      return new_node_assign(node, assign(t));
    default:
      return node;
  }
}

static Expr* expr(TokenList** t) {
  return assign(t);
}

static Declaration* try_declaration(TokenList** t) {
  Token t1 = head_TokenList(*t);

  if (t1.kind != TK_IDENT) {
    return NULL;
  }

  // TODO: Parse type specifier
  if (strcmp(t1.ident, "decl") != 0) {
    return NULL;
  }

  consume(t);

  if (head_of(t) != TK_IDENT) {
    return NULL;
  }

  Declaration* d = new_declaration(consuming(t).ident);

  if (consuming(t).kind != TK_SEMICOLON) {
    return NULL;
  }

  return d;
}

static BlockItemList* block_item_list(TokenList** t);

static Statement* statement(TokenList** t) {
  switch (head_of(t)) {
    case TK_RETURN: {
      consume(t);
      Expr* e = expr(t);
      expect(t, TK_SEMICOLON);
      return new_statement(ST_RETURN, e);
    }
    case TK_IF: {
      consume(t);
      expect(t, TK_LPAREN);
      Expr* c = expr(t);
      expect(t, TK_RPAREN);
      Statement* then_ = statement(t);
      Statement* else_;
      if (head_of(t) == TK_ELSE) {
        consume(t);
        else_ = statement(t);
      } else {
        else_ = new_statement(ST_NULL, NULL);
      }
      Statement* s = new_statement(ST_IF, c);
      s->then_     = then_;
      s->else_     = else_;
      return s;
    }
    case TK_WHILE: {
      consume(t);
      expect(t, TK_LPAREN);
      Expr* c = expr(t);
      expect(t, TK_RPAREN);
      Statement* body = statement(t);
      Statement* s    = new_statement(ST_WHILE, c);
      s->body         = body;
      return s;
    }
    case TK_DO: {
      consume(t);
      Statement* body = statement(t);
      expect(t, TK_WHILE);
      expect(t, TK_LPAREN);
      Expr* c = expr(t);
      expect(t, TK_RPAREN);
      expect(t, TK_SEMICOLON);
      Statement* s = new_statement(ST_DO, c);
      s->body      = body;
      return s;
    }
    case TK_FOR: {
      consume(t);
      expect(t, TK_LPAREN);

      Expr* init;
      if (head_of(t) == TK_SEMICOLON) {
        init = NULL;
      } else {
        init = expr(t);
      }

      expect(t, TK_SEMICOLON);

      Expr* before;
      if (head_of(t) == TK_SEMICOLON) {
        before = new_node_num(1);
      } else {
        before = expr(t);
      }

      expect(t, TK_SEMICOLON);

      Expr* after;
      if (head_of(t) == TK_RPAREN) {
        after = NULL;
      } else {
        after = expr(t);
      }

      expect(t, TK_RPAREN);
      Statement* body = statement(t);

      Statement* s = new_statement(ST_FOR, NULL);
      s->init      = init;
      s->before    = before;
      s->after     = after;
      s->body      = body;
      return s;
    }
    case TK_BREAK: {
      consume(t);
      expect(t, TK_SEMICOLON);
      return new_statement(ST_BREAK, NULL);
    }
    case TK_CONTINUE: {
      consume(t);
      expect(t, TK_SEMICOLON);
      return new_statement(ST_CONTINUE, NULL);
    }
    case TK_LBRACE: {
      consume(t);
      Statement* s = new_statement(ST_COMPOUND, NULL);
      s->items     = block_item_list(t);
      expect(t, TK_RBRACE);
      return s;
    }
    case TK_SEMICOLON: {
      consume(t);
      return new_statement(ST_NULL, NULL);
    }
    default: {
      Expr* e = expr(t);
      expect(t, TK_SEMICOLON);
      return new_statement(ST_EXPRESSION, e);
    }
  }
}

static BlockItem* block_item(TokenList** t) {
  TokenList* save = *t;
  Declaration* d  = try_declaration(t);
  if (d) {
    return new_block_item(BI_DECL, NULL, d);
  }

  *t = save;
  return new_block_item(BI_STMT, statement(t), NULL);
}

BlockItemList* block_item_list(TokenList** t) {
  BlockItemList* cur  = nil_BlockItemList();
  BlockItemList* list = cur;

  while (head_of(t) != TK_END && head_of(t) != TK_RBRACE) {
    cur = snoc_BlockItemList(block_item(t), cur);
  }
  return list;
}

// parse tokens into AST
AST* parse(TokenList* t) {
  return block_item_list(&t);
}
