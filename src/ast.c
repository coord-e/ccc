#include "ast.h"
#include "util.h"

// release functions
static void release_expr(Expr* e) {
  if (e == NULL) {
    return;
  }

  release_expr(e->lhs);
  release_expr(e->rhs);
  release_expr(e->expr);
  free(e->var);
  release_ExprVec(e->args);
  release_Type(e->cast_to);
  release_Type(e->type);

  free(e);
}

DEFINE_VECTOR(release_expr, Expr*, ExprVec)

static void release_Declarator(Declarator* d) {
  if (d == NULL) {
    return;
  }

  free(d->name);
  release_Declarator(d->decl);
  release_expr(d->length);
  free(d);
}

static void release_declaration(Declaration* d) {
  if (d == NULL) {
    return;
  }

  release_Declarator(d->declarator);
  free(d);
}

static void release_statement(Statement* stmt) {
  if (stmt == NULL) {
    return;
  }

  release_expr(stmt->expr);
  free(stmt);
}

static void release_BlockItem(BlockItem* item) {
  if (item == NULL) {
    return;
  }

  release_statement(item->stmt);
  release_declaration(item->decl);
  free(item);
}

DEFINE_LIST(release_BlockItem, BlockItem*, BlockItemList)

DEFINE_LIST(release_Declarator, Declarator*, ParamList)

static void release_FunctionDef(FunctionDef* def) {
  if (def == NULL) {
    return;
  }
  release_Declarator(def->decl);
  release_ParamList(def->params);
  release_BlockItemList(def->items);
  free(def);
}
static void release_FunctionDecl(FunctionDecl* decl) {
  if (decl == NULL) {
    return;
  }
  release_Declarator(decl->decl);
  release_ParamList(decl->params);
  free(decl);
}
static void release_ExternalDecl(ExternalDecl* edecl) {
  release_FunctionDef(edecl->func);
  release_FunctionDecl(edecl->func_decl);
  free(edecl);
}

DEFINE_LIST(release_ExternalDecl, ExternalDecl*, TranslationUnit)

void release_AST(AST* t) {
  release_TranslationUnit(t);
}

// printer functions
DECLARE_VECTOR_PRINTER(ExprVec)

static void print_expr(FILE* p, Expr* expr) {
  switch (expr->kind) {
    case ND_NUM:
      fprintf(p, "%d", expr->num);
      return;
    case ND_VAR:
      fprintf(p, "%s", expr->var);
      return;
    case ND_ASSIGN:
      fprintf(p, "(");
      print_expr(p, expr->lhs);
      fprintf(p, " = ");
      print_expr(p, expr->rhs);
      fprintf(p, ")");
      return;
    case ND_BINOP:
      fprintf(p, "(");
      print_binop(p, expr->binop);
      fprintf(p, " ");
      print_expr(p, expr->lhs);
      fprintf(p, " ");
      print_expr(p, expr->rhs);
      fprintf(p, ")");
      return;
    case ND_UNAOP:
      fprintf(p, "(");
      print_unaop(p, expr->unaop);
      fprintf(p, " ");
      print_expr(p, expr->expr);
      fprintf(p, ")");
      return;
    case ND_CALL:
      print_expr(p, expr->lhs);
      fprintf(p, "(");
      print_ExprVec(p, expr->args);
      fprintf(p, ")");
      return;
    case ND_CAST:
      fprintf(p, "(");
      print_Type(p, expr->cast_to);
      fprintf(p, ")");
      print_expr(p, expr->expr);
      return;
    default:
      CCC_UNREACHABLE;
  }
}
DEFINE_VECTOR_PRINTER(print_expr, ",", "", ExprVec)

static void print_Declarator(FILE* p, Declarator* d) {
  switch (d->kind) {
    case DE_DIRECT:
      for (unsigned i = 0; i < d->num_ptrs; i++) {
        fprintf(p, "*");
      }
      fprintf(p, "%s", d->name);
      break;
    case DE_ARRAY:
      print_Declarator(p, d->decl);
      fputs("[", p);
      print_expr(p, d->length);
      fputs("]", p);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static void print_declaration(FILE* p, Declaration* d) {
  fprintf(p, "int ");
  print_Declarator(p, d->declarator);
  fprintf(p, ";");
}

static void print_statement(FILE* p, Statement* stmt);

static void print_BlockItem(FILE* p, BlockItem* item) {
  switch (item->kind) {
    case BI_DECL:
      print_declaration(p, item->decl);
      break;
    case BI_STMT:
      print_statement(p, item->stmt);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

DECLARE_LIST_PRINTER(BlockItemList)
DEFINE_LIST_PRINTER(print_BlockItem, "\n", "\n", BlockItemList)

DECLARE_LIST_PRINTER(ParamList)
DEFINE_LIST_PRINTER(print_Declarator, ",", "", ParamList)

DECLARE_LIST_PRINTER(TranslationUnit)
static void print_FunctionDef(FILE* p, FunctionDef* def) {
  print_Declarator(p, def->decl);
  fprintf(p, " (");
  print_ParamList(p, def->params);
  fprintf(p, ") {\n");
  print_BlockItemList(p, def->items);
  fprintf(p, "}\n");
}
static void print_FunctionDecl(FILE* p, FunctionDecl* decl) {
  print_Declarator(p, decl->decl);
  fprintf(p, " (");
  print_ParamList(p, decl->params);
  fprintf(p, ");\n");
}
static void print_ExternalDecl(FILE* p, ExternalDecl* edecl) {
  switch (edecl->kind) {
    case EX_FUNC:
      print_FunctionDef(p, edecl->func);
      break;
    case EX_FUNC_DECL:
      print_FunctionDecl(p, edecl->func_decl);
      break;
    default:
      CCC_UNREACHABLE;
  }
}
DEFINE_LIST_PRINTER(print_ExternalDecl, "\n", "\n", TranslationUnit)

void print_statement(FILE* p, Statement* d) {
  switch (d->kind) {
    case ST_EXPRESSION:
      print_expr(p, d->expr);
      fputs(";", p);
      break;
    case ST_RETURN:
      fputs("return ", p);
      if (d->expr != NULL) {
        print_expr(p, d->expr);
      }
      fputs(";", p);
      break;
    case ST_IF:
      fputs("if (", p);
      print_expr(p, d->expr);
      fputs(") ", p);
      print_statement(p, d->then_);
      fputs(" else ", p);
      print_statement(p, d->else_);
      break;
    case ST_NULL:
      fputs(";", p);
      break;
    case ST_WHILE:
      fputs("while (", p);
      print_expr(p, d->expr);
      fputs(") ", p);
      print_statement(p, d->body);
      break;
    case ST_DO:
      fputs("do ", p);
      print_statement(p, d->body);
      fputs(" while (", p);
      print_expr(p, d->expr);
      fputs(");", p);
      break;
    case ST_FOR:
      fputs("for (", p);
      if (d->init != NULL) {
        print_expr(p, d->init);
      }
      fputs("; ", p);
      print_expr(p, d->before);
      fputs("; ", p);
      if (d->after != NULL) {
        print_expr(p, d->after);
      }
      fputs(") ", p);
      print_statement(p, d->body);
      break;
    case ST_BREAK:
      fputs("break;", p);
      break;
    case ST_CONTINUE:
      fputs("continue;", p);
      break;
    case ST_COMPOUND:
      fputs("{ ", p);
      print_BlockItemList(p, d->items);
      fputs(" }", p);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

void print_AST(FILE* p, AST* ast) {
  print_TranslationUnit(p, ast);
}

// constructors
Expr* new_node(ExprKind kind, Expr* lhs, Expr* rhs) {
  Expr* node    = calloc(1, sizeof(Expr));
  node->kind    = kind;
  node->lhs     = lhs;
  node->rhs     = rhs;
  node->expr    = NULL;
  node->var     = NULL;
  node->args    = NULL;
  node->cast_to = NULL;
  node->type    = NULL;
  return node;
}

Expr* new_node_num(int num) {
  Expr* node = new_node(ND_NUM, NULL, NULL);
  node->num  = num;
  return node;
}

Expr* new_node_var(char* ident) {
  Expr* node = new_node(ND_VAR, NULL, NULL);
  node->var  = strdup(ident);
  return node;
}

Expr* new_node_binop(BinopKind kind, Expr* lhs, Expr* rhs) {
  Expr* node  = new_node(ND_BINOP, lhs, rhs);
  node->binop = kind;
  return node;
}

Expr* new_node_unaop(UnaopKind kind, Expr* expr) {
  Expr* node  = new_node(ND_UNAOP, NULL, NULL);
  node->unaop = kind;
  node->expr  = expr;
  return node;
}

Expr* new_node_assign(Expr* lhs, Expr* rhs) {
  return new_node(ND_ASSIGN, lhs, rhs);
}

Expr* new_node_cast(Type* ty, Expr* opr) {
  Expr* node    = new_node(ND_CAST, NULL, NULL);
  node->cast_to = ty;
  node->expr    = opr;
  return node;
}

Declarator* new_Declarator(DeclaratorKind kind) {
  Declarator* d = calloc(1, sizeof(Declarator));
  d->kind       = kind;
  d->num_ptrs   = 0;
  d->name       = NULL;
  d->decl       = NULL;
  d->length     = NULL;
  return d;
}

Declaration* new_declaration(Declarator* s) {
  Declaration* d = calloc(1, sizeof(Declaration));
  d->declarator  = s;
  return d;
}

Statement* new_statement(StmtKind kind, Expr* expr) {
  Statement* s = calloc(1, sizeof(Statement));
  s->kind      = kind;
  s->expr      = expr;
  return s;
}

BlockItem* new_block_item(BlockItemKind kind, Statement* stmt, Declaration* decl) {
  BlockItem* item = calloc(1, sizeof(BlockItem));
  item->kind      = kind;
  item->stmt      = stmt;
  item->decl      = decl;
  return item;
}

FunctionDef* new_function_def() {
  FunctionDef* def = calloc(1, sizeof(FunctionDef));
  def->decl        = NULL;
  def->params      = NULL;
  def->items       = NULL;
  return def;
}

FunctionDecl* new_function_decl() {
  FunctionDecl* decl = calloc(1, sizeof(FunctionDecl));
  decl->decl         = NULL;
  decl->params       = NULL;
  return decl;
}

ExternalDecl* new_external_decl(ExtDeclKind kind) {
  ExternalDecl* edecl = calloc(1, sizeof(ExternalDecl));
  edecl->kind         = kind;
  edecl->func         = NULL;
  edecl->func_decl    = NULL;
  return edecl;
}
