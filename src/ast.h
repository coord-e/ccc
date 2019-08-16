#ifndef CCC_AST_H
#define CCC_AST_H

#include <stdio.h>

#include "list.h"
#include "map.h"
#include "ops.h"
#include "type.h"
#include "vector.h"

typedef struct Expr Expr;

typedef struct {
  char* name;
  Expr* value;  // nullable
} Enumerator;

Enumerator* new_Enumerator(char* name, Expr* value);

DECLARE_LIST(Enumerator*, EnumeratorList)

typedef enum {
  ES_DECL,  // with enumerator-list
  ES_NAME,  // otherwise
} EnumSpecKind;

typedef struct {
  EnumSpecKind kind;

  char* tag;              // owned, nullable in SS_DECL
  EnumeratorList* enums;  // for ES_DECL, owned
} EnumSpecifier;

EnumSpecifier* new_EnumSpecifier(EnumSpecKind, char* tag);

typedef struct DeclarationSpecifiers DeclarationSpecifiers;
typedef struct Declarator Declarator;

DECLARE_LIST(Declarator*, DeclaratorList)

typedef struct {
  DeclarationSpecifiers* spec;  // owned
  DeclaratorList* declarators;  // owned
} StructDeclaration;

StructDeclaration* new_StructDeclaration(DeclarationSpecifiers*, DeclaratorList*);

DECLARE_LIST(StructDeclaration*, StructDeclarationList)

typedef enum {
  SS_DECL,  // with struct-declaration-list
  SS_NAME,  // otherwise
} StructSpecKind;

typedef struct {
  StructSpecKind kind;

  char* tag;                            // owned, nullable in SS_DECL
  StructDeclarationList* declarations;  // for SS_DECL, owned
} StructSpecifier;

StructSpecifier* new_StructSpecifier(StructSpecKind, char*);

// use bit flags to express the combination of names
// this idea is from `cdecl.c` by Rui Ueyama
// Copyright (C) 2019 Rui Ueyama
typedef enum {
  BT_SIGNED   = 1,
  BT_UNSIGNED = 1 << 2,
  BT_VOID     = 1 << 4,
  BT_BOOL     = 1 << 6,
  BT_CHAR     = 1 << 8,
  BT_SHORT    = 1 << 10,
  BT_INT      = 1 << 12,
  BT_LONG     = 1 << 14,
} BaseType;

typedef enum {
  DS_BASE,
  DS_STRUCT,
  DS_ENUM,
  DS_TYPEDEF_NAME,
} DeclarationSpecKind;

struct DeclarationSpecifiers {
  DeclarationSpecKind kind;

  BaseType base_type;        // for DS_BASE
  StructSpecifier* struct_;  // for DS_STRUCT, owned
  EnumSpecifier* enum_;      // for DS_ENUM, owned

  bool is_typedef;
  char* typedef_name;
  bool is_extern;
  bool is_static;
  bool is_const;
};

DeclarationSpecifiers* new_DeclarationSpecifiers(DeclarationSpecKind);

typedef enum {
  DE_DIRECT_ABSTRACT,
  DE_DIRECT,
  DE_ARRAY,
} DirectDeclKind;

typedef struct DirectDeclarator DirectDeclarator;

struct DirectDeclarator {
  DirectDeclKind kind;
  char* name_ref;  // not owned, NULL if this is abstract declarator

  char* name;  // for DE_DIRECT, owned

  DirectDeclarator* decl;  // for DE_ARRAY, owned
  Expr* length;            // for DE_ARRAY, owned, NULL if omitted
};

DirectDeclarator* new_DirectDeclarator(DirectDeclKind);
bool is_abstract_direct_declarator(DirectDeclarator*);

struct Declarator {
  DirectDeclarator* direct;
  unsigned num_ptrs;
};

Declarator* new_Declarator(DirectDeclarator* direct, unsigned num_ptrs);
bool is_abstract_declarator(Declarator*);

typedef struct Initializer Initializer;

DECLARE_LIST(Initializer*, InitializerList)

typedef enum {
  IN_EXPR,
  IN_LIST,
} InitializerKind;

struct Initializer {
  InitializerKind kind;
  Expr* expr;             // for IN_EXPR, owned
  InitializerList* list;  // for IN_LIST, owned
};

Initializer* new_Initializer(InitializerKind);

typedef struct {
  Declarator* declarator;    // owned
  Initializer* initializer;  // owned, nullable

  // will filled in `sema`
  // NOTE: computed from declarator and declaration-specifier from declaration
  Type* type;  // owned
} InitDeclarator;

DECLARE_LIST(InitDeclarator*, InitDeclaratorList)

// NOTE: the second parameter can be NULL
InitDeclarator* new_InitDeclarator(Declarator*, Initializer*);

typedef struct {
  DeclarationSpecifiers* spec;      // owned
  InitDeclaratorList* declarators;  // owned
} Declaration;

Declaration* new_declaration(DeclarationSpecifiers* spec, InitDeclaratorList* list);

typedef struct {
  DeclarationSpecifiers* spec;  // owned
  Declarator* declarator;       // owned

  // will filled in `sema`
  Type* type;  // owned
} TypeName;

TypeName* new_TypeName(DeclarationSpecifiers* spec, Declarator* s);

typedef enum {
  ND_BINOP,
  ND_UNAOP,
  ND_ADDR,
  ND_DEREF,
  ND_ADDR_ARY,
  ND_ASSIGN,
  ND_COMPOUND_ASSIGN,
  ND_VAR,
  ND_NUM,
  ND_STRING,
  ND_CALL,
  ND_CAST,
  ND_COND,
  ND_COMMA,
  ND_SIZEOF_EXPR,
  ND_SIZEOF_TYPE,
  ND_MEMBER,
} ExprKind;

DECLARE_VECTOR(Expr*, ExprVec)

struct Expr {
  ExprKind kind;

  Expr* lhs;  // for ND_BINOP, ND_COMMA, ND_COMPOUND_ASSIGN and ND_ASSIGN, owned
  Expr* rhs;  // ditto

  // for ND_ADDR, ND_DEREF, ND_ADDR_ARY, ND_UNAOP, ND_SIZEOF_EXPR, ND_MEMBER and ND_CAST, owned
  Expr* expr;

  TypeName* cast_to;  // for ND_CAST, owned, nullable if `cast_type` is not NULL

  TypeName* sizeof_;  // for ND_SIZEOF_TYPE, owned

  BinopKind binop;  // for ND_BINOP, ND_COMPOUND_ASSIGN
  UnaopKind unaop;  // for ND_UNAOP
  char* var;        // for ND_VAR, owned
  int num;          // for ND_NUM
  char* string;     // for ND_STRING, owned
  size_t str_len;   // for ND_STRING
  ExprVec* args;    // for ND_CALL, owned
  char* member;     // for ND_MEMBER, owned

  Expr* cond;   // for ND_COND, owned
  Expr* then_;  // for ND_COND, owned
  Expr* else_;  // for ND_COND, owned

  // will filled in `sema`
  Type* type;       // owned
  Type* cast_type;  // for ND_CAST, owned
};

Expr* new_node(ExprKind kind, Expr* lhs, Expr* rhs);
Expr* new_node_num(int num);
Expr* new_node_var(const char* ident);
Expr* new_node_string(const char* s, size_t len);
Expr* new_node_binop(BinopKind kind, Expr* lhs, Expr* rhs);
Expr* new_node_unaop(UnaopKind kind, Expr* expr);
Expr* new_node_addr(Expr* expr);
Expr* new_node_addr_ary(Expr* expr);
Expr* new_node_deref(Expr* expr);
Expr* new_node_assign(Expr* lhs, Expr* rhs);
Expr* new_node_comma(Expr* lhs, Expr* rhs);
Expr* new_node_compound_assign(BinopKind, Expr* lhs, Expr* rhs);
Expr* new_node_cast(TypeName* ty, Expr* opr);
Expr* new_node_cond(Expr* cond, Expr* then_, Expr* else_);
Expr* new_node_sizeof_type(TypeName*);
Expr* new_node_sizeof_expr(Expr*);
Expr* new_node_member(Expr*, const char*);
Expr* shallow_copy_node(Expr*);

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

BlockItem* new_block_item(BlockItemKind kind, Statement* stmt, Declaration* decl);

DECLARE_LIST(BlockItem*, BlockItemList)

DECLARE_VECTOR(Statement*, StmtVec)

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
  ST_LABEL,
  ST_CASE,
  ST_DEFAULT,
  ST_GOTO,
  ST_SWITCH,
} StmtKind;

struct Statement {
  StmtKind kind;
  // for ST_EXPRESSION, ST_RETURN, ST_WHILE, ST_DO, ST_SWITCH, ST_CASE, and ST_IF, owned
  Expr* expr;

  Statement* body;  // for ST_WHILE, ST_DO, ST_FOR, ST_LABEL, ST_CASE, ST_DEFAULT, ST_SWITCH

  Statement* then_;  // for ST_IF
  Statement* else_;  // for ST_IF

  Expr* init;    // for ST_FOR, NULL if omitted
  Expr* before;  // for ST_FOR
  Expr* after;   // for ST_FOR, NULL if omitted

  BlockItemList* items;  // for ST_COMPOUND

  // for ST_LABEL and ST_GOTO, owned
  char* label_name;

  // will filled in `sema`
  unsigned label_id;    // for ST_LABEL, ST_CASE, ST_DEFAULT
  long case_value;      // for ST_CASE
  StmtVec* cases;       // for ST_SWITCH, not owned
  Statement* default_;  // for ST_SWITCH, not owned
};

Statement* new_statement(StmtKind kind, Expr* expr);

typedef struct {
  DeclarationSpecifiers* spec;
  Declarator* decl;
} ParameterDecl;

ParameterDecl* new_ParameterDecl(DeclarationSpecifiers*, Declarator*);

DECLARE_LIST(ParameterDecl*, ParamList)
DECLARE_MAP(unsigned, UIMap)

typedef struct {
  DeclarationSpecifiers* spec;  // owned
  Declarator* decl;             // owned
  ParamList* params;            // owned

  BlockItemList* items;  // owned

  // will filled in `sema`
  Type* type;           // owned
  UIMap* named_labels;  // owned
  unsigned label_count;
} FunctionDef;

FunctionDef* new_function_def();

// Separate function declaration and other declarations to simplify implementation,
// while function declaration is expressed as a normal declaration in C spec.
typedef struct {
  DeclarationSpecifiers* spec;  // owned
  Declarator* decl;             // owned
  ParamList* params;            // owned

  // will filled in `sema`
  Type* type;  // owned
} FunctionDecl;

FunctionDecl* new_function_decl();

typedef enum {
  EX_FUNC,
  EX_FUNC_DECL,
  EX_DECL,
} ExtDeclKind;

typedef struct {
  ExtDeclKind kind;
  FunctionDef* func;        // for EX_FUNC, owned
  FunctionDecl* func_decl;  // for EX_FUNC_DECL, owned
  Declaration* decl;        // for EX_DECL, owned
} ExternalDecl;

ExternalDecl* new_external_decl(ExtDeclKind kind);

DECLARE_LIST(ExternalDecl*, TranslationUnit)

typedef TranslationUnit AST;

void print_AST(FILE*, AST*);

void release_AST(AST*);

#endif
