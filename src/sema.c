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

static void should_compatible(Type* expected, Type* ty) {
  if (!is_compatible_ty(expected, ty)) {
    type_error(expected, ty);
  }
}

static void should_arithmetic(Type* ty) {
  if (!is_arithmetic_ty(ty)) {
    fputs("arithmetic type is expected, but got ", stderr);
    print_Type(stderr, ty);
    fputs("\n", stderr);
    error("type error");
  }
}

static void should_integer(Type* ty) {
  if (!is_integer_ty(ty)) {
    fputs("integer type is expected, but got ", stderr);
    print_Type(stderr, ty);
    fputs("\n", stderr);
    error("type error");
  }
}

static void should_pointer(Type* ty) {
  if (!is_pointer_ty(ty)) {
    fputs("pointer is expected, but got ", stderr);
    print_Type(stderr, ty);
    fputs("\n", stderr);
    error("type error");
  }
}

static Type* sema_binop(BinopKind op, Type* lhs, Type* rhs) {
  switch (op) {
    case BINOP_ADD:
      if (is_arithmetic_ty(lhs) && is_arithmetic_ty(rhs)) {
        // TODO: is this correct in current situation
        return copy_Type(lhs);
      }
      should_pointer(lhs);
      should_integer(rhs);
      return copy_Type(lhs);
    case BINOP_SUB:
      if (is_arithmetic_ty(lhs) && is_arithmetic_ty(rhs)) {
        // TODO: is this correct in current situation
        return copy_Type(lhs);
      }
      if (is_pointer_ty(lhs) && is_pointer_ty(rhs)) {
        should_compatible(lhs->ptr_to, rhs->ptr_to);
        // TODO: how to handle `ptrdiff_t`
        return int_ty();
      }
      should_pointer(lhs);
      should_integer(rhs);
      return copy_Type(lhs);
    case BINOP_MUL:
    case BINOP_DIV:
      should_arithmetic(lhs);
      should_arithmetic(rhs);
      return int_ty();
    case BINOP_EQ:
    case BINOP_NE:
      if (is_arithmetic_ty(lhs) && is_arithmetic_ty(rhs)) {
        return int_ty();
      }
      should_pointer(lhs);
      should_pointer(rhs);
      should_compatible(lhs->ptr_to, rhs->ptr_to);
      return int_ty();
    case BINOP_GT:
    case BINOP_GE:
    case BINOP_LT:
    case BINOP_LE:
      if (is_real_ty(lhs) && is_real_ty(rhs)) {
        return int_ty();
      }
      should_pointer(lhs);
      should_pointer(rhs);
      should_compatible(lhs->ptr_to, rhs->ptr_to);
      return int_ty();
    default:
      CCC_UNREACHABLE;
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
      should_compatible(lhs_ty, rhs_ty);
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

        should_compatible(a_ty, p_ty);
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

static void sema_decl(Env* env, Declaration* decl) {
  Declarator* d = decl->declarator;
  Type* ty      = ptrify(int_ty(), d->num_ptrs);
  add_var(env, d->name, ty);
}

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
