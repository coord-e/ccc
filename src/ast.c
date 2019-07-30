#include "ast.h"

// release functions
static void release_expr(Expr* e) {
  if (e == NULL) {
    return;
  }

  release_expr(e->lhs);
  release_expr(e->rhs);
  free(e->var);

  free(e);
}

static void release_declaration(Declaration* d) {
  if (d == NULL) {
    return;
  }

  free(d->declarator);
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

static void release_string(char* s) {
  free(s);
}
DEFINE_LIST(release_string, char*, StringList)

static void release_FunctionDef(FunctionDef* def) {
  free(def->name);
  release_StringList(def->params);
  release_BlockItemList(def->items);
  free(def);
}
DEFINE_LIST(release_FunctionDef, FunctionDef*, TranslationUnit)

void release_AST(AST* t) {
  release_TranslationUnit(t);
}

// printer functions
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
    default:
      CCC_UNREACHABLE;
  }
}

static void print_declaration(FILE* p, Declaration* d) {
  fprintf(p, "decl %s;", d->declarator);
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

DECLARE_LIST_PRINTER(StringList)
static void print_string(FILE* p, char* s) {
  fprintf(p, "%s", s);
}
DEFINE_LIST_PRINTER(print_string, ",", "", StringList)

DECLARE_LIST_PRINTER(TranslationUnit)
static void print_FunctionDef(FILE* p, FunctionDef* def) {
  fprintf(p, "%s (", def->name);
  print_StringList(p, def->params);
  fprintf(p, ") {\n");
  print_BlockItemList(p, def->items);
  fprintf(p, "}\n");
}
DEFINE_LIST_PRINTER(print_FunctionDef, "\n", "\n", TranslationUnit)

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
