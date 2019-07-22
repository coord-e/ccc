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
  release_BlockItemList(stmt->items);
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

void release_AST(AST* t) {
  release_BlockItemList(t);
}

// printer functions
DECLARE_LIST_PRINTER(BlockItemList)

static void print_expr_(FILE* p, Expr* expr) {
  switch (expr->kind) {
  case ND_NUM:
    fprintf(p, "%d", expr->num);
    return;
  case ND_VAR:
    fprintf(p, "%s", expr->var);
    return;
  case ND_ASSIGN:
    fprintf(p, "(");
    print_expr_(p, expr->lhs);
    fprintf(p, " = ");
    print_expr_(p, expr->rhs);
    fprintf(p, ")");
    return;
  case ND_BINOP:
    fprintf(p, "(");
    print_binop(p, expr->binop);
    fprintf(p, " ");
    print_expr_(p, expr->lhs);
    fprintf(p, " ");
    print_expr_(p, expr->rhs);
    fprintf(p, ")");
    return;
  default:
    CCC_UNREACHABLE;
  }
}

static void print_expr(FILE* p, Expr* expr) {
  print_expr_(p, expr);
  fprintf(p, "\n");
}

static void print_declaration(FILE* p, Declaration* d) {
  fprintf(p, "decl %s", d->declarator);
}
