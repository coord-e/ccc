#ifndef CCC_AST_H
#define CCC_AST_H

#include <stdio.h>

#include "list.h"
#include "binop.h"

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
  char* declarator; // TODO: Add initializer
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
  ST_COMPOUND,
} StmtKind;

struct Statement {
  StmtKind kind;
  Expr* expr;            // for ST_EXPRESSION, owned
  BlockItemList* items;  // for ST_COMPOUND, owned
};

typedef BlockItemList AST;

void print_ast(FILE*, AST*);
void print_binop(FILE*, BinopKind kind);

void release_ast(AST*);

#endif
