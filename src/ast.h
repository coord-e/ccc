#ifndef CCC_AST_H
#define CCC_AST_H

#include <stdio.h>

#include "binop.h"
#include "list.h"
#include "vector.h"

typedef enum {
  ND_BINOP,
  ND_ASSIGN,
  ND_VAR,
  ND_NUM,
  ND_CALL,
} ExprKind;

typedef struct Expr Expr;

DECLARE_VECTOR(Expr*, ExprVec)

struct Expr {
  ExprKind kind;
  Expr* lhs;        // for ND_BINOP and ND_ASSIGN, owned
  Expr* rhs;        // ditto
  BinopKind binop;  // for ND_BINOP
  char* var;        // for ND_VAR, owned
  int num;          // for ND_NUM
  ExprVec* args;    // for ND_CALL, owned
};

typedef struct {
  unsigned num_ptrs;
  char* name;  // owned
} Declarator;

typedef struct {
  // TODO: Add declaration specifiers
  Declarator* declarator;  // owned
} Declaration;

typedef struct Statement Statement;

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
  ST_WHILE,
  ST_DO,
  ST_FOR,
  ST_BREAK,
  ST_CONTINUE,
  ST_NULL,
} StmtKind;

struct Statement {
  StmtKind kind;
  Expr* expr;  // for ST_EXPRESSION, ST_RETURN, ST_WHILE, ST_DO, and ST_IF, owned

  Statement* body;  // for ST_WHILE, ST_DO

  Statement* then_;  // for ST_IF
  Statement* else_;  // for ST_IF

  Expr* init;    // for ST_FOR, NULL if omitted
  Expr* before;  // for ST_FOR
  Expr* after;   // for ST_FOR, NULL if omitted

  BlockItemList* items;  // for ST_COMPOUND
};

DECLARE_LIST(Declarator*, ParamList)

typedef struct {
  char* name;         // owned
  ParamList* params;  // owned

  BlockItemList* items;  // owned
} FunctionDef;

DECLARE_LIST(FunctionDef*, TranslationUnit)

typedef TranslationUnit AST;

void print_AST(FILE*, AST*);

void release_AST(AST*);

#endif
