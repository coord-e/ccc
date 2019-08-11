#include "sema.h"
#include "map.h"
#include "type.h"

DECLARE_MAP(Type*, TypeMap)
DEFINE_MAP(release_Type, Type*, TypeMap)

typedef struct {
  TypeMap* names;
} GlobalEnv;

typedef struct {
  TypeMap* vars;
  Type* ret_ty;
  GlobalEnv* global;
} Env;

static GlobalEnv* init_GlobalEnv() {
  GlobalEnv* env = calloc(1, sizeof(GlobalEnv));
  env->names     = new_TypeMap(64);
  return env;
}

static Env* init_Env(GlobalEnv* global, Type* ret) {
  Env* env    = calloc(1, sizeof(Env));
  env->vars   = new_TypeMap(64);
  env->global = global;
  env->ret_ty = ret;
  return env;
}

static void release_Env(Env* env) {
  release_TypeMap(env->vars);
  free(env);
}

static void release_GlobalEnv(GlobalEnv* env) {
  release_TypeMap(env->names);
  free(env);
}

static void add_var(Env* env, const char* name, Type* ty) {
  insert_TypeMap(env->vars, name, ty);
}

static void add_global(GlobalEnv* env, const char* name, Type* ty) {
  insert_TypeMap(env->names, name, ty);
}

static Type* get_var(Env* env, const char* name) {
  Type* ty;
  if (!lookup_TypeMap(env->vars, name, &ty)) {
    if (!lookup_TypeMap(env->global->names, name, &ty)) {
      error("undeclared identifier \"%s\"", name);
    }
  }
  return copy_Type(ty);
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
  fputs(" but got: ", stderr);
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

static void should_scalar(Type* ty) {
  if (!is_scalar_ty(ty)) {
    fputs("scalar type is expected, but got ", stderr);
    print_Type(stderr, ty);
    fputs("\n", stderr);
    error("type error");
  }
}

// p + n (p: t*, s: sizeof(t)) -> (t*)((uint64_t)p + n * s)
// both `int_opr` and `ptr_opr` are consumed and new untyped node is returned
static Expr* build_pointer_arith(BinopKind op, Expr* ptr_opr, Expr* int_opr) {
  assert(is_pointer_ty(ptr_opr->type));
  assert(is_integer_ty(int_opr->type));

  Type* int_ty       = into_unsigned_ty(int_of_size_ty(sizeof_ty(ptr_opr->type)));
  unsigned elem_size = sizeof_ty(ptr_opr->type->ptr_to);

  Expr* ptr_opr_c = new_node_cast(int_ty, ptr_opr);
  // TODO: Remove this explicit cast by arithmetic conversion
  Expr* int_opr_c =
      new_node_cast(copy_Type(int_ty), new_node_binop(BINOP_MUL, int_opr, new_node_num(elem_size)));
  Expr* new_expr = new_node_binop(op, ptr_opr_c, int_opr_c);

  return new_node_cast(copy_Type(ptr_opr->type), new_expr);
}

// p1 - p2 (p1, p2: t*, s: sizeof(t)) -> ((uint64_t)p1 - (uint64_t)p2) / s
// both `opr1` and `opr2` are consumed and new untyped node is returned
static Expr* build_pointer_diff(Expr* opr1, Expr* opr2) {
  assert(is_pointer_ty(opr1->type));
  assert(is_pointer_ty(opr2->type));
  assert(equal_to_Type(opr1->type->ptr_to, opr2->type->ptr_to));

  Type* int_ty = int_of_size_ty(sizeof_ty(opr1->type));

  Expr* opr1_c   = new_node_cast(int_ty, opr1);
  Expr* opr2_c   = new_node_cast(copy_Type(int_ty), opr2);
  Expr* new_expr = new_node_binop(BINOP_SUB, opr1_c, opr2_c);
  // TODO: Remove this explicit cast by arithmetic conversion
  Expr* num_c = new_node_cast(copy_Type(int_ty), new_node_num(sizeof_ty(opr1->type->ptr_to)));

  return new_node_binop(BINOP_DIV, new_expr, num_c);
}

// convert array/function to pointer
// `opr` is consumed and new untyped node is returned
static Expr* build_decay(Expr* opr) {
  assert(is_array_ty(opr->type) || is_function_ty(opr->type));

  switch (opr->type->kind) {
    case TY_ARRAY:
      return new_node_unaop(UNAOP_ADDR_ARY, opr);
    case TY_FUNC:
      return new_node_unaop(UNAOP_ADDR, opr);
    default:
      CCC_UNREACHABLE;
  }
}

static Type* sema_expr(Env* env, Expr* expr);

static Type* sema_binop(Env* env, Expr* expr) {
  BinopKind op = expr->binop;
  Type* lhs    = sema_expr(env, expr->lhs);
  Type* rhs    = sema_expr(env, expr->rhs);

  switch (op) {
    case BINOP_ADD:
      if (is_pointer_ty(lhs)) {
        should_integer(rhs);

        // TODO: shallow release of rhs of this assignment
        *expr = *build_pointer_arith(op, expr->lhs, expr->rhs);

        Type* ty = copy_Type(lhs);
        sema_expr(env, expr);
        assert(equal_to_Type(expr->type, ty));

        return ty;
      }
      if (is_pointer_ty(rhs)) {
        should_integer(lhs);

        // TODO: shallow release of rhs of this assignment
        *expr = *build_pointer_arith(op, expr->rhs, expr->lhs);

        Type* ty = copy_Type(rhs);
        sema_expr(env, expr);
        assert(equal_to_Type(expr->type, ty));

        return ty;
      }
      should_arithmetic(lhs);
      should_arithmetic(rhs);
      // TODO: arithmetic conversion
      return copy_Type(lhs);
    case BINOP_SUB:
      if (is_arithmetic_ty(lhs) && is_arithmetic_ty(rhs)) {
        // TODO: arithmetic conversion
        return copy_Type(lhs);
      }
      if (is_pointer_ty(lhs) && is_pointer_ty(rhs)) {
        should_compatible(lhs->ptr_to, rhs->ptr_to);

        // TODO: shallow release of rhs of this assignment
        *expr = *build_pointer_diff(expr->lhs, expr->rhs);

        // TODO: how to handle `ptrdiff_t`
        Type* ty = int_ty();
        sema_expr(env, expr);
        assert(equal_to_Type(expr->type, ty));

        return ty;
      }
      should_pointer(lhs);
      should_integer(rhs);

      // TODO: shallow release of rhs of this assignment
      *expr = *build_pointer_arith(op, expr->lhs, expr->rhs);

      Type* ty = copy_Type(lhs);
      sema_expr(env, expr);
      assert(equal_to_Type(expr->type, ty));

      return ty;
    case BINOP_MUL:
    case BINOP_DIV:
      should_arithmetic(lhs);
      should_arithmetic(rhs);
      // TODO: arithmetic conversion
      return copy_Type(lhs);
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

static Type* sema_expr_raw(Env* env, Expr* expr);

static Type* sema_unaop(Env* env, Expr* e) {
  Expr* opr = e->expr;
  switch (e->unaop) {
    case UNAOP_ADDR: {
      Type* ty = sema_expr_raw(env, opr);
      return ptr_to_ty(copy_Type(ty));
    }
    case UNAOP_ADDR_ARY: {
      Type* ty = sema_expr_raw(env, opr);
      assert(is_array_ty(ty));
      return ptr_to_ty(copy_Type(ty->element));
    }
    case UNAOP_DEREF: {
      Type* ty = sema_expr(env, opr);
      should_pointer(ty);
      return copy_Type(ty->ptr_to);
    }
    default:
      CCC_UNREACHABLE;
  }
}

// returned `Type*` is reference to a data is owned by `expr`
Type* sema_expr_raw(Env* env, Expr* expr) {
  Type* t;
  switch (expr->kind) {
    case ND_CAST: {
      Type* ty = sema_expr(env, expr->expr);
      // TODO: Check floating type
      // TODO: arithmetic conversions
      should_scalar(ty);
      t = copy_Type(expr->cast_to);
      break;
    }
    case ND_BINOP: {
      t = sema_binop(env, expr);
      break;
    }
    case ND_UNAOP: {
      t = sema_unaop(env, expr);
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

  if (expr->type != NULL) {
    release_Type(expr->type);
  }
  expr->type = t;
  return t;
}

static Type* sema_expr(Env* env, Expr* e) {
  Type* ty = sema_expr_raw(env, e);
  switch (ty->kind) {
    case TY_FUNC:
    case TY_ARRAY: {
      Expr* copy = shallow_copy_node(e);
      // TODO: shallow release of rhs of this assignment
      *e = *build_decay(copy);
      return sema_expr(env, e);
    }
    default:
      return ty;
  }
}

static int eval_constant(Expr* e) {
  // TODO: Support more nodes
  switch (e->kind) {
    case ND_NUM:
      return e->num;
    default:
      error("invalid constant expression");
  }
}

static Type* translate_type(BaseType t) {
#pragma GCC diagnostic ignored "-Wswitch"
  switch (t) {
    case BT_SIGNED + BT_CHAR:
      return into_signed_ty(char_ty());
    case BT_CHAR:
    case BT_UNSIGNED + BT_CHAR:
      return char_ty();
    case BT_INT:
    case BT_SIGNED:
    case BT_SIGNED + BT_INT:
      return int_ty();
    case BT_UNSIGNED:
    case BT_UNSIGNED + BT_INT:
      return into_unsigned_ty(int_ty());
    case BT_LONG:
    case BT_LONG + BT_INT:
    case BT_SIGNED + BT_LONG:
    case BT_SIGNED + BT_LONG + BT_INT:
      return long_ty();
    case BT_UNSIGNED + BT_LONG:
    case BT_UNSIGNED + BT_LONG + BT_INT:
      return into_unsigned_ty(long_ty());
    case BT_SHORT:
    case BT_SHORT + BT_INT:
    case BT_SIGNED + BT_SHORT:
    case BT_SIGNED + BT_SHORT + BT_INT:
      return short_ty();
    case BT_UNSIGNED + BT_SHORT:
    case BT_UNSIGNED + BT_SHORT + BT_INT:
      return into_unsigned_ty(short_ty());
    default:
      error("invalid type");
  }
#pragma GCC diagnostic warning "-Wswitch"
}

static void extract_declarator(Declarator* decl, Type* base, char** name, Type** type) {
  switch (decl->kind) {
    case DE_DIRECT:
      *type = ptrify(base, decl->num_ptrs);
      *name = decl->name;
      return;
    case DE_ARRAY: {
      Type* ty;
      extract_declarator(decl->decl, base, name, &ty);
      int length = eval_constant(decl->length);
      if (length <= 0) {
        error("invalid size of array: %d", length);
      }

      *type = array_ty(ty, length);
      return;
    }
    default:
      CCC_UNREACHABLE;
  }
}

static void sema_decl(Env* env, Declaration* decl) {
  Type* base_ty = translate_type(decl->spec->type->base);
  char* name;
  Type* ty;
  extract_declarator(decl->declarator, base_ty, &name, &ty);
  decl->type = copy_Type(ty);
  add_var(env, name, ty);
}

static void sema_items(Env* env, BlockItemList* l);

static void sema_stmt(Env* env, Statement* stmt) {
  switch (stmt->kind) {
    case ST_COMPOUND: {
      // block
      TypeMap* save = env->vars;
      TypeMap* inst = copy_TypeMap(env->vars);

      env->vars = inst;
      sema_items(env, stmt->items);
      env->vars = save;

      // TODO: shallow release
      /* release_TypeMap(inst); */
      break;
    }
    case ST_EXPRESSION:
      sema_expr(env, stmt->expr);
      break;
    case ST_RETURN: {
      Type* t = sema_expr(env, stmt->expr);
      should_compatible(env->ret_ty, t);
      break;
    }
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

static TypeVec* param_types(Env* env, ParamList* cur) {
  TypeVec* params = new_TypeVec(2);
  while (!is_nil_ParamList(cur)) {
    ParameterDecl* d = head_ParamList(cur);
    Type* base_ty    = translate_type(d->spec->type->base);
    Type* type;
    char* name;
    extract_declarator(d->decl, base_ty, &name, &type);
    push_TypeVec(params, type);
    if (env != NULL) {
      add_var(env, name, copy_Type(type));
    }
    cur = tail_ParamList(cur);
  }
  return params;
}

static void sema_function(GlobalEnv* global, FunctionDef* f) {
  Type* base_ty = translate_type(f->spec->type->base);
  Type* ret;
  char* name;
  extract_declarator(f->decl, base_ty, &name, &ret);

  Env* env        = init_Env(global, ret);
  TypeVec* params = param_types(env, f->params);
  Type* ty        = func_ty(ret, params);
  f->type         = copy_Type(ty);

  add_global(global, name, ty);
  sema_items(env, f->items);

  release_Env(env);
}

static void sema_translation_unit(GlobalEnv* global, TranslationUnit* l) {
  if (is_nil_TranslationUnit(l)) {
    return;
  }

  ExternalDecl* d = head_TranslationUnit(l);
  switch (d->kind) {
    case EX_FUNC:
      sema_function(global, d->func);
      break;
    case EX_FUNC_DECL: {
      FunctionDecl* f = d->func_decl;
      Type* base_ty   = translate_type(f->spec->type->base);
      Type* ret;
      char* name;
      extract_declarator(f->decl, base_ty, &name, &ret);
      TypeVec* params = param_types(NULL, f->params);
      Type* ty        = func_ty(ret, params);
      f->type         = copy_Type(ty);
      add_global(global, name, ty);
      break;
    }
    default:
      CCC_UNREACHABLE;
  }
  sema_translation_unit(global, tail_TranslationUnit(l));
}

void sema(AST* ast) {
  GlobalEnv* env = init_GlobalEnv();
  sema_translation_unit(env, ast);
  release_GlobalEnv(env);
}
