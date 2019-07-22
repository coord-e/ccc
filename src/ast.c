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

void release_AST(AST* t) {
  release_BlockItemList(t);
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
  fprintf(p, "decl %s", d->declarator);
}

static void print_statement(FILE* p, Statement* d) {
  switch(d->kind) {
    case ST_EXPRESSION:
      print_expr(p, d->expr);
      fputs(";", p);
    default:
      CCC_UNREACHABLE;
  }
}

static void print_BlockItem(FILE* p, BlockItem* item) {
  switch(item->kind) {
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

void print_AST(FILE* p, AST* ast) {
  print_BlockItemList(p, ast);
}
