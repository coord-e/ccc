#ifndef CCC_AST_H
#define CCC_AST_H

#include <stdio.h>

#include "binop.h"
#include "list.h"

typedef enum {
  ND_BINOP,
  ND_ASSIGN,
  ND_VAR,
  ND_NUM,
} ExprKind;

typedef struct Expr Expr;

struct Expr {
  ExprKind kind;
  Expr* lhs;        // for ND_BINOP and ND_ASSIGN, owned
  Expr* rhs;        // ditto
  BinopKind binop;  // for ND_BINOP
  char* var;        // for ND_VAR, owned
  int num;          // for ND_NUM
};

typedef struct Statement Statement;

typedef struct {
  // TODO: Add declaration specifiers
  char* declarator;  // owned TODO: Add initializer
} Declaration;

typedef enum {
  BI_STMT,
  BI_DECL,
} BlockItemKind;

typedef struct {
  BlockItemKind kind;
  Statement* stmt;    // for BI_STMT, owned
  Declaration* decl;  // for BI_DECL, owned
} BlockItem;

DECLARE_LIST(BlockItem*, BlockItemList)

typedef enum {
  ST_EXPRESSION,
  ST_RETURN,
  ST_IF,
  ST_COMPOUND,
} StmtKind;

struct Statement {
  StmtKind kind;
  Expr* expr;  // for ST_EXPRESSION, ST_RETURN, and ST_IF, owned

  Statement* then_;  // for ST_IF
  Statement* else_;  // for ST_IF

  BlockItemList* items;  // for ST_COMPOUND
};

typedef BlockItemList AST;

void print_AST(FILE*, AST*);

void release_AST(AST*);

#endif
