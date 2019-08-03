#ifndef CCC_AST_H
#define CCC_AST_H

#include <stdio.h>

#include "list.h"
#include "ops.h"
#include "type.h"
#include "vector.h"

typedef enum {
  ND_BINOP,
  ND_UNAOP,
  ND_ASSIGN,
  ND_VAR,
  ND_NUM,
  ND_CALL,
} ExprKind;

typedef struct Expr Expr;

DECLARE_VECTOR(Expr*, ExprVec)

struct Expr {
  ExprKind kind;

  Expr* lhs;  // for ND_BINOP and ND_ASSIGN, owned
  Expr* rhs;  // ditto

  Expr* expr;  // for ND_UNAOP, owned

  BinopKind binop;  // for ND_BINOP
  UnaopKind unaop;  // for ND_UNAOP
  char* var;        // for ND_VAR, owned
  int num;          // for ND_NUM
  ExprVec* args;    // for ND_CALL, owned

  // will filled in `sema`
  Type* type;  // owned
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

  Statement* body;  // for ST_WHILE, ST_DO, ST_BODY

  Statement* then_;  // for ST_IF
  Statement* else_;  // for ST_IF

  Expr* init;    // for ST_FOR, NULL if omitted
  Expr* before;  // for ST_FOR
  Expr* after;   // for ST_FOR, NULL if omitted

  BlockItemList* items;  // for ST_COMPOUND
};

DECLARE_LIST(Declarator*, ParamList)

typedef struct {
  Declarator* decl;   // owned
  ParamList* params;  // owned

  BlockItemList* items;  // owned
} FunctionDef;

// Separate function declaration and other declarations to simplify implementation,
// while function declaration is expressed as a normal declaration in C spec.
typedef struct {
  Declarator* decl;   // owned
  ParamList* params;  // owned
} FunctionDecl;

typedef enum {
  EX_FUNC,
  EX_FUNC_DECL,
} ExtDeclKind;

typedef struct {
  ExtDeclKind kind;
  FunctionDef* func;        // for EX_FUNC, owned
  FunctionDecl* func_decl;  // for EX_FUNC_DECL, owned
} ExternalDecl;

DECLARE_LIST(ExternalDecl*, TranslationUnit)

typedef TranslationUnit AST;

void print_AST(FILE*, AST*);

void release_AST(AST*);

#endif
