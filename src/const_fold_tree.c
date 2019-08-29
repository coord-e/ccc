#include "const_fold_tree.h"

void const_fold_tree(AST* t) {
  return;
}

bool get_constant(Expr* e, long* t) {
  switch (e->kind) {
    case ND_NUM:
      *t = e->num;
      return true;
    default:
      return false;
  }
}

long perform_trunc(Type* t, long c) {
  // TODO: impl
  return c;
}

// TODO: release (emplace)
void fold_expr(Expr* e) {
  switch (e->kind) {
    case ND_BINOP: {
      fold_expr(e->lhs);
      fold_expr(e->rhs);
      long lhs_c, rhs_c;
      if (get_constant(e->lhs, &lhs_c) && get_constant(e->rhs, &rhs_c)) {
        *e = *new_node_num(eval_binop(e->binop, lhs_c, rhs_c));
      }
      return;
    }
    case ND_UNAOP: {
      fold_expr(e->expr);
      long constant;
      if (get_constant(e->expr, &constant)) {
        *e = *new_node_num(eval_unaop(e->unaop, constant));
      }
      return;
    }
    case ND_ADDR:
    case ND_DEREF:
    case ND_ADDR_ARY:
    case ND_MEMBER:
      fold_expr(e->expr);
      return;
    case ND_ASSIGN:
    case ND_COMPOUND_ASSIGN:
    case ND_COMMA:
      fold_expr(e->lhs);
      fold_expr(e->rhs);
      return;
    case ND_VAR:
    case ND_NUM:
    case ND_STRING:
      return;
    case ND_CAST: {
      fold_expr(e->expr);
      long constant;
      if (get_constant(e->expr, &constant)) {
        *e = *new_node_num(perform_trunc(e->cast_type, constant));
      }
      return;
    }
    case ND_COND: {
      fold_expr(e->cond);
      fold_expr(e->then_);
      fold_expr(e->else_);
      long cond_c;
      if (get_constant(e->cond, &cond_c)) {
        if (cond_c) {
          *e = *e->then_;
        } else {
          *e = *e->else_;
        }
      }
      return;
    }
    case ND_CALL:
      fold_expr(e->lhs);
      for (unsigned i = 0; i < length_ExprVec(e->args); i++) {
        fold_expr(get_ExprVec(e->args, i));
      }
      return;
    case ND_SIZEOF_EXPR:
    case ND_SIZEOF_TYPE:
      assert(false && "ND_SIZEOF_* must be removed in the previous pass");
    default:
      CCC_UNREACHABLE;
  }
  CCC_UNREACHABLE;
}
