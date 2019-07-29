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

typedef struct Declarator Declarator;

typedef struct {
  Declarator* decl;
} ParameterDeclaration;

DECLARE_LIST(ParameterDeclaration*, ParameterList)

typedef enum {
  DD_IDENT,
  DD_FUNCTION,
} DirectDeclKind;

typedef struct DirectDeclarator DirectDeclarator;

struct DirectDeclarator {
  DirectDeclKind kind;

  char* ident;  // for DD_IDENT

  DirectDeclarator* dd;   // for DD_FUNCTION
  ParameterList* params;  // for DD_FUNCTION
};

struct Declarator {
  DirectDeclarator* dd;
};

typedef struct {
  // TODO: Add declaration specifiers
  // TODO: Add initializer
  Declarator* decl;
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

typedef BlockItemList AST;

void print_AST(FILE*, AST*);

void release_AST(AST*);

#endif
