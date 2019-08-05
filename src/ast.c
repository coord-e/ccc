#include "ast.h"

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
  for (unsigned i = 0; i < d->num_ptrs; i++) {
    fprintf(p, "*");
  }
  fprintf(p, "%s", d->name);
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
