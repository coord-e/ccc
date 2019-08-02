#include "sema.h"
#include "map.h"
#include "type.h"

DECLARE_MAP(Type*, TypeMap)
DEFINE_MAP(release_Type, Type*, TypeMap)

typedef struct {
  TypeMap* vars;
} Env;

static Env* init_Env() {
  Env* env  = calloc(1, sizeof(Env));
  env->vars = new_TypeMap(64);
  return env;
}

static void release_Env(Env* env) {
  release_TypeMap(env->vars);
  free(env);
}

static void add_var(Env* env, const char* name, Type* ty) {
  insert_TypeMap(env->vars, name, ty);
}

static Type* get_var(Env* env, const char* name) {
  Type* ty;
  if (!lookup_TypeMap(env->vars, name, &ty)) {
    error("undeclared identifier \"%s\"", name);
  }
  return ty;
}

static Type* ptrify(Type* base, unsigned num) {
  if (num == 0) {
    return base;
  } else {
    return ptrify(ptr_to_ty(base), num - 1);
  }
}

static noreturn void type_error(Type* expected, Type* got) {
  fputs("expected: ", stderr);
  print_Type(stderr, expected);
  fputs("but got: ", stderr);
  print_Type(stderr, got);
  fputs("\n", stderr);
  error("type mismatch");
}

static void should_equal(Type* expected, Type* ty) {
  if (!equal_to_Type(expected, ty)) {
    type_error(expected, ty);
  }
}

// returned `Type*` is reference to a data is owned by `expr`
static Type* sema_expr(Env* env, Expr* expr) {
  Type* t;
  switch (expr->kind) {
    case ND_BINOP: {
      Type* lhs_ty = sema_expr(env, expr->lhs);
      Type* rhs_ty = sema_expr(env, expr->rhs);
      t            = sema_binop(expr->binop, lhs_ty, rhs_ty);
      break;
    }
    case ND_ASSIGN: {
      Type* lhs_ty = sema_expr(env, expr->lhs);
      Type* rhs_ty = sema_expr(env, expr->rhs);
      // TODO: type conversion
      should_equal(lhs_ty, rhs_ty);
      t = copy_Type(lhs_ty);
      break;
    }
    case ND_VAR:
      t = get_var(env, expr->var);
      break;
    case ND_NUM:
      t = int_ty();
      break;
    case ND_CALL: {
      Type* lhs_ty = sema_expr(env, expr->lhs);
      if (lhs_ty->kind != TY_PTR || lhs_ty->ptr_to->kind != TY_FUNC) {
        fputs("attempt to call a value with type ", stderr);
        print_Type(stderr, lhs_ty);
        fputs("\n", stderr);
        error("could not call a value other than function pointers");
      }

      Type* f             = lhs_ty->ptr_to;
      unsigned num_args   = length_ExprVec(expr->args);
      unsigned num_params = length_TypeVec(f->params);
      if (num_args != num_params) {
        error("too many / too few arguments to function");
      }

      for (unsigned i = 0; i < num_args; i++) {
        Expr* a    = get_ExprVec(expr->args, i);
        Type* a_ty = sema_expr(env, a);
        Type* p_ty = get_TypeVec(f->params, i);

        if (!equal_to_Type(a_ty, p_ty)) {
          type_error(a_ty, p_ty);
        }
      }

      t = copy_Type(f->ret);
      break;
    }
    default:
      CCC_UNREACHABLE;
  }
  expr->type = t;
  return t;
}

static void sema_decl(Env* env, Declaration* decl);

static void sema_items(Env* env, BlockItemList* l);

static void sema_stmt(Env* env, Statement* stmt) {
  switch (stmt->kind) {
    case ST_COMPOUND:
      sema_items(env, stmt->items);
      break;
    case ST_EXPRESSION:
    case ST_RETURN:
      sema_expr(env, stmt->expr);
      break;
    case ST_IF:
      sema_expr(env, stmt->expr);
      sema_stmt(env, stmt->then_);
      sema_stmt(env, stmt->else_);
      break;
    case ST_WHILE:
    case ST_DO:
      sema_expr(env, stmt->expr);
      sema_stmt(env, stmt->body);
      break;
    case ST_FOR:
      if (stmt->init != NULL) {
        sema_expr(env, stmt->init);
      }
      sema_expr(env, stmt->before);
      if (stmt->after != NULL) {
        sema_expr(env, stmt->after);
      }
      sema_stmt(env, stmt->body);
      break;
    case ST_BREAK:
    case ST_CONTINUE:
    case ST_NULL:
      break;
    default:
      CCC_UNREACHABLE;
  }
}

void sema_items(Env* env, BlockItemList* l) {
  if (is_nil_BlockItemList(l)) {
    return;
  }

  BlockItem* item = head_BlockItemList(l);
  switch (item->kind) {
    case BI_STMT:
      sema_stmt(env, item->stmt);
      break;
    case BI_DECL:
      sema_decl(env, item->decl);
      break;
    default:
      CCC_UNREACHABLE;
  }

  sema_items(env, tail_BlockItemList(l));
}

static void sema_functions(Env* env, TranslationUnit* l) {
  if (is_nil_TranslationUnit(l)) {
    return;
  }

  FunctionDef* f = head_TranslationUnit(l);
  sema_items(env, f->items);

  sema_functions(env, tail_TranslationUnit(l));
}

void sema(AST* ast) {
  Env* env = init_Env();
  sema_functions(env, ast);
  release_Env(env);
}
