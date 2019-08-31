#include "const_fold_tree.h"

void const_fold_initializer(Initializer* init) {
  switch (init->kind) {
    case IN_EXPR:
      const_fold_expr(init->expr);
      break;
    case IN_LIST: {
      InitializerList* l = init->list;
      while (!is_nil_InitializerList(l)) {
        const_fold_initializer(head_InitializerList(l));
        l = tail_InitializerList(l);
      }
      break;
    }
    default:
      CCC_UNREACHABLE;
  }
}

void const_fold_decl(Declaration* decl) {
  InitDeclaratorList* l = decl->declarators;
  while (!is_nil_InitDeclaratorList(l)) {
    InitDeclarator* id = head_InitDeclaratorList(l);
    if (id->initializer != NULL) {
      const_fold_initializer(id->initializer);
    }
    l = tail_InitDeclaratorList(l);
  }
}

void const_fold_items(BlockItemList* items);

// TODO: release (emplace)
void const_fold_stmt(Statement* stmt) {
  switch (stmt->kind) {
    case ST_EXPRESSION:
      const_fold_expr(stmt->expr);
      break;
    case ST_RETURN:
      if (stmt->expr != NULL) {
        const_fold_expr(stmt->expr);
      }
      break;
    case ST_COMPOUND:
      const_fold_items(stmt->items);
      break;
    case ST_IF: {
      const_fold_expr(stmt->expr);
      long cond_c;
      if (get_constant(stmt->expr, &cond_c)) {
        if (cond_c) {
          *stmt = *stmt->then_;
        } else {
          *stmt = *stmt->else_;
        }
      }
      break;
    }
    case ST_WHILE: {
      const_fold_expr(stmt->expr);
      const_fold_stmt(stmt->body);
      long cond_c;
      if (get_constant(stmt->expr, &cond_c)) {
        if (!cond_c) {
          *stmt = *new_statement(ST_NULL, NULL);
        }
      }
      break;
    }
    case ST_DO: {
      const_fold_expr(stmt->expr);
      const_fold_stmt(stmt->body);
      break;
    }
    case ST_FOR: {
      if (stmt->init_decl != NULL) {
        const_fold_decl(stmt->init_decl);
      } else if (stmt->init != NULL) {
        const_fold_expr(stmt->init);
      }
      const_fold_expr(stmt->before);
      if (stmt->after != NULL) {
        const_fold_expr(stmt->after);
      }
      const_fold_stmt(stmt->body);
      long cond_c;
      if (get_constant(stmt->before, &cond_c)) {
        if (!cond_c) {
          *stmt = *new_statement(ST_NULL, NULL);
        }
      }
      break;
    }
    case ST_BREAK:
    case ST_CONTINUE:
    case ST_NULL:
    case ST_GOTO:
      break;
    case ST_LABEL:
    case ST_CASE:
    case ST_DEFAULT:
      const_fold_stmt(stmt->body);
      break;
    case ST_SWITCH:
      // TODO: optimize
      const_fold_expr(stmt->expr);
      const_fold_stmt(stmt->body);
      break;
    default:
      CCC_UNREACHABLE;
  }
}
void const_fold_items(BlockItemList* items) {
  while (!is_nil_BlockItemList(items)) {
    BlockItem* item = head_BlockItemList(items);
    switch (item->kind) {
      case BI_DECL:
        const_fold_decl(item->decl);
        break;
      case BI_STMT:
        const_fold_stmt(item->stmt);
        break;
      default:
        CCC_UNREACHABLE;
    }
    items = tail_BlockItemList(items);
  }
}

void const_fold_tree(AST* ast) {
  TranslationUnit* l = ast;
  while (!is_nil_TranslationUnit(l)) {
    ExternalDecl* d = head_TranslationUnit(l);
    switch (d->kind) {
      case EX_FUNC:
        const_fold_items(d->func->items);
        break;
      case EX_FUNC_DECL:
        break;
      case EX_DECL:
        const_fold_decl(d->decl);
        break;
      default:
        CCC_UNREACHABLE;
    }
    l = tail_TranslationUnit(l);
  }
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
void const_fold_expr(Expr* e) {
  switch (e->kind) {
    case ND_BINOP: {
      const_fold_expr(e->lhs);
      const_fold_expr(e->rhs);
      long lhs_c, rhs_c;
      if (get_constant(e->lhs, &lhs_c) && get_constant(e->rhs, &rhs_c)) {
        *e = *new_node_num(eval_binop(e->binop, lhs_c, rhs_c));
      }
      return;
    }
    case ND_UNAOP: {
      const_fold_expr(e->expr);
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
      const_fold_expr(e->expr);
      return;
    case ND_ASSIGN:
    case ND_COMPOUND_ASSIGN:
    case ND_COMMA:
      const_fold_expr(e->lhs);
      const_fold_expr(e->rhs);
      return;
    case ND_VAR:
    case ND_NUM:
    case ND_STRING:
      return;
    case ND_CAST: {
      const_fold_expr(e->expr);
      long constant;
      if (get_constant(e->expr, &constant)) {
        *e = *new_node_num(perform_trunc(e->cast_type, constant));
      }
      return;
    }
    case ND_COND: {
      const_fold_expr(e->cond);
      const_fold_expr(e->then_);
      const_fold_expr(e->else_);
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
      const_fold_expr(e->lhs);
      for (unsigned i = 0; i < length_ExprVec(e->args); i++) {
        const_fold_expr(get_ExprVec(e->args, i));
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
