#include "sema.h"
#include "map.h"
#include "type.h"

DECLARE_MAP(Type*, TypeMap)
DEFINE_MAP(copy_Type, release_Type, Type*, TypeMap)

typedef struct {
  TypeMap* names;
  TypeMap* tagged_structs;
} GlobalEnv;

typedef struct {
  TypeMap* vars;
  Type* ret_ty;
  GlobalEnv* global;
  UIMap* named_labels;
  unsigned label_count;
  Statement* current_switch;
  bool is_global_only;
  TypeMap* tagged_structs;
} Env;

static GlobalEnv* init_GlobalEnv() {
  GlobalEnv* env      = calloc(1, sizeof(GlobalEnv));
  env->names          = new_TypeMap(64);
  env->tagged_structs = new_TypeMap(64);
  return env;
}

static Env* init_Env(GlobalEnv* global, Type* ret) {
  Env* env            = calloc(1, sizeof(Env));
  env->vars           = new_TypeMap(64);
  env->global         = global;
  env->ret_ty         = ret;
  env->named_labels   = new_UIMap(32);
  env->label_count    = 0;
  env->current_switch = NULL;
  env->is_global_only = false;
  env->tagged_structs = new_TypeMap(16);
  return env;
}

static Env* fake_env(GlobalEnv* global) {
  Env* env            = calloc(1, sizeof(Env));
  env->global         = global;
  env->is_global_only = true;
  return env;
}

static void release_Env(Env* env) {
  release_TypeMap(env->vars);
  release_TypeMap(env->tagged_structs);
  free(env);
}

static void release_GlobalEnv(GlobalEnv* env) {
  release_TypeMap(env->names);
  release_TypeMap(env->tagged_structs);
  free(env);
}

static void add_var(Env* env, const char* name, Type* ty) {
  insert_TypeMap(env->vars, name, ty);
}

static void add_global(GlobalEnv* env, const char* name, Type* ty) {
  insert_TypeMap(env->names, name, ty);
}

static void add_tagged_struct(Env* env, const char* name, Type* ty) {
  insert_TypeMap(env->tagged_structs, name, ty);
}

static void add_global_tagged_struct(GlobalEnv* env, const char* name, Type* ty) {
  insert_TypeMap(env->tagged_structs, name, ty);
}

static unsigned new_label_id(Env* env) {
  return env->label_count++;
}

static unsigned add_anon_label(Env* env) {
  return new_label_id(env);
}

static unsigned add_named_label(Env* env, const char* name) {
  if (lookup_UIMap(env->named_labels, name, NULL)) {
    error("redefinition of label \"%s\"", name);
  }

  unsigned id = new_label_id(env);
  insert_UIMap(env->named_labels, name, id);
  return id;
}

static Statement* start_switch(Env* env, Statement* s) {
  assert(s->cases == NULL);
  s->cases = new_StmtVec(8);

  Statement* old      = env->current_switch;
  env->current_switch = s;
  return old;
}

static void set_case(Env* env, Statement* s) {
  assert(s->kind == ST_CASE);
  if (env->current_switch == NULL) {
    error("case statement not in switch statement");
  }

  push_StmtVec(env->current_switch->cases, s);
}

static void set_default(Env* env, Statement* s) {
  assert(s->kind == ST_DEFAULT);
  if (env->current_switch == NULL) {
    error("default statement not in switch statement");
  }
  if (env->current_switch->default_ != NULL) {
    error("multiple default labels in one switch");
  }

  env->current_switch->default_ = s;
}

static Type* get_var(Env* env, const char* name) {
  Type* ty;
  if (env->is_global_only || !lookup_TypeMap(env->vars, name, &ty)) {
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

static void should_same(Type* expected, Type* ty) {
  if (!equal_to_Type(expected, ty)) {
    type_error(expected, ty);
  }
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

static void should_complete(Type* ty) {
  if (!is_complete_ty(ty)) {
    fputs("complete type is expected, but got ", stderr);
    print_Type(stderr, ty);
    fputs("\n", stderr);
    error("type error");
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

static Type* translate_base_type(BaseType t) {
#pragma GCC diagnostic ignored "-Wswitch"
  switch (t) {
    case BT_VOID:
      return void_ty();
    case BT_BOOL:
      return bool_ty();
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

static Type* translate_declaration_specifiers(Env* env, DeclarationSpecifiers* spec) {
  switch (spec->kind) {
    case DS_BASE:
      return translate_base_type(spec->base_type);
    case DS_STRUCT:
      error("unimplemented");
    default:
      CCC_UNREACHABLE;
  }
}

static void extract_direct_declarator(DirectDeclarator* decl,
                                      Type* base,
                                      char** name,
                                      Type** type) {
  switch (decl->kind) {
    case DE_DIRECT_ABSTRACT:
      assert(name == NULL);
      *type = copy_Type(base);
      return;
    case DE_DIRECT:
      *type = copy_Type(base);
      if (name != NULL) {
        *name = decl->name;
      }
      return;
    case DE_ARRAY: {
      Type* t;
      if (decl->length != NULL) {
        int length = eval_constant(decl->length);
        if (length <= 0) {
          error("invalid size of array: %d", length);
        }

        t         = array_ty(base, true);
        t->length = length;
      } else {
        t = array_ty(base, false);
      }

      extract_direct_declarator(decl->decl, t, name, type);
      return;
    }
    default:
      CCC_UNREACHABLE;
  }
}

// extract `Decalrator` and store the result to `name` and `type`
// if `name` is NULL, this accepts abstract declarator
static void extract_declarator(Declarator* decl, Type* base, char** name, Type** type) {
  extract_direct_declarator(decl->direct, ptrify(base, decl->num_ptrs), name, type);
}

static Type* translate_type_name(Env* env, TypeName* t) {
  Type* base_ty = translate_declaration_specifiers(env, t->spec);
  Type* res;
  extract_declarator(t->declarator, base_ty, NULL, &res);
  return res;
}

static bool is_null_pointer_constant(Expr* e) {
  return e->kind == ND_NUM && e->num == 0;
}

// `new_node_cast`, but supply targetted `Type*` directly
static Expr* new_cast_direct(Type* ty, Expr* opr) {
  Expr* e      = new_node_cast(NULL, opr);
  e->cast_type = ty;
  return e;
}

// cast for lvals
static Expr* new_lval_cast(Type* ty, Expr* opr) {
  Expr* ptr_opr_c = new_cast_direct(ptr_to_ty(ty), new_node_addr(opr));
  return new_node_deref(ptr_opr_c);
}

static Expr* build_conversion_to(Type* ty, Expr* opr, bool is_lval) {
  Expr* target;
  if (ty->kind == TY_BOOL) {
    target = new_node_binop(BINOP_NE, opr, new_node_num(0));
  } else {
    target = opr;
  }
  if (is_lval) {
    return new_lval_cast(ty, target);
  } else {
    return new_cast_direct(ty, target);
  }
}

// p + n (p: t*, s: sizeof(t)) -> (t*)((uint64_t)p + n * s)
// both `int_opr` and `ptr_opr` are consumed and new untyped node is returned
static Expr* build_pointer_arith(BinopKind op, Expr* ptr_opr, Expr* int_opr) {
  assert(is_pointer_ty(ptr_opr->type));
  assert(is_integer_ty(int_opr->type));

  Type* int_ty       = into_unsigned_ty(int_of_size_ty(sizeof_ty(ptr_opr->type)));
  unsigned elem_size = sizeof_ty(ptr_opr->type->ptr_to);

  Expr* ptr_opr_c = new_cast_direct(int_ty, ptr_opr);
  // TODO: Remove this explicit cast by arithmetic conversion
  Expr* int_opr_c = new_cast_direct(copy_Type(int_ty),
                                    new_node_binop(BINOP_MUL, int_opr, new_node_num(elem_size)));
  Expr* new_expr  = new_node_binop(op, ptr_opr_c, int_opr_c);

  return new_cast_direct(copy_Type(ptr_opr->type), new_expr);
}

// p += n (p: t*, s: sizeof(t)) -> (t*)(*((uint64_t*)&p) += n * s)
// both `int_opr` and `ptr_opr` are consumed and new untyped node is returned
static Expr* build_pointer_arith_assign(BinopKind op, Expr* ptr_opr, Expr* int_opr) {
  assert(is_pointer_ty(ptr_opr->type));
  assert(is_integer_ty(int_opr->type));

  Type* int_ty       = into_unsigned_ty(int_of_size_ty(sizeof_ty(ptr_opr->type)));
  unsigned elem_size = sizeof_ty(ptr_opr->type->ptr_to);

  Expr* lhs = new_lval_cast(int_ty, ptr_opr);
  // TODO: Remove this explicit cast by arithmetic conversion
  Expr* int_opr_c = new_cast_direct(copy_Type(int_ty),
                                    new_node_binop(BINOP_MUL, int_opr, new_node_num(elem_size)));
  Expr* new_expr  = new_node_compound_assign(op, lhs, int_opr_c);

  return new_cast_direct(copy_Type(ptr_opr->type), new_expr);
}

// p1 - p2 (p1, p2: t*, s: sizeof(t)) -> ((uint64_t)p1 - (uint64_t)p2) / s
// both `opr1` and `opr2` are consumed and new untyped node is returned
static Expr* build_pointer_diff(Expr* opr1, Expr* opr2) {
  assert(is_pointer_ty(opr1->type));
  assert(is_pointer_ty(opr2->type));
  assert(equal_to_Type(opr1->type->ptr_to, opr2->type->ptr_to));

  Type* int_ty = int_of_size_ty(sizeof_ty(opr1->type));

  Expr* opr1_c   = new_cast_direct(int_ty, opr1);
  Expr* opr2_c   = new_cast_direct(copy_Type(int_ty), opr2);
  Expr* new_expr = new_node_binop(BINOP_SUB, opr1_c, opr2_c);
  // TODO: Remove this explicit cast by arithmetic conversion
  Expr* num_c = new_cast_direct(copy_Type(int_ty), new_node_num(sizeof_ty(opr1->type->ptr_to)));

  return new_node_binop(BINOP_DIV, new_expr, num_c);
}

// convert array/function to pointer
// `opr` is consumed and new untyped node is returned
static Expr* build_decay(Expr* opr) {
  assert(is_array_ty(opr->type) || is_function_ty(opr->type));

  switch (opr->type->kind) {
    case TY_ARRAY:
      return new_node_addr_ary(opr);
    case TY_FUNC:
      return new_node_addr(opr);
    default:
      CCC_UNREACHABLE;
  }
}

// calculate the resulting type of integer promotion
// return NULL if no conversion is required
// caller has an ownership of returned `Type*`
static Type* promoted_type(Type* opr) {
  if (!is_integer_ty(opr)) {
    return NULL;
  }
  Type* int_ = int_ty();
  if (compare_rank_ty(opr, int_) <= 0) {
    if (is_representable_in_ty(opr, int_)) {
      return int_;
    } else {
      return into_unsigned_ty(int_);
    }
  }
  return NULL;
}

static Expr* indirect(Expr* e) {
  return shallow_copy_node(e);
}

static Type* sema_expr(Env* env, Expr* expr);

// perform an integer promotion, if required
// caller has an ownership of returned `Type*`
static Type* integer_promotion(Env* env, Expr* opr, bool is_lval) {
  Type* t = promoted_type(opr->type);
  if (t != NULL) {
    // TODO: shallow release rhs
    *opr = *build_conversion_to(t, indirect(opr), is_lval);
    sema_expr(env, opr);
    return copy_Type(t);
  } else {
    return copy_Type(opr->type);
  }
}

// perform an arithmetic conversion on two operands
// and return a "common real type""
// caller has an ownership of returned `Type*`
// TODO: shallow release (entire function)
// TODO: improve the control flow (e1_is_*)
static Type* arithmetic_conversion(Env* env, Expr* e1, Expr* e2, bool e1_is_lval) {
  Type* t1 = integer_promotion(env, e1, e1_is_lval);
  Type* t2 = integer_promotion(env, e2, false);

  // assign types for ease (we need exprs and types to be paired later)
  e1->type = t1;
  e2->type = t2;

  if (equal_to_Type(t1, t2)) {
    return copy_Type(t1);
  }

  if (t1->is_signed == t2->is_signed) {
    if (compare_rank_ty(t1, t2) > 0) {
      // t2 is lesser
      *e2 = *build_conversion_to(copy_Type(t1), indirect(e2), false);
      sema_expr(env, e2);
      return copy_Type(t1);
    } else {
      // t1 is lesser
      *e1 = *build_conversion_to(copy_Type(t2), indirect(e1), e1_is_lval);
      sema_expr(env, e1);
      return copy_Type(t2);
    }
  }

  Expr* signed_opr;
  Expr* unsigned_opr;
  bool e1_is_signed;
  if (t1->is_signed) {
    signed_opr   = e1;
    unsigned_opr = e2;
    e1_is_signed = true;
  } else {
    signed_opr   = e2;
    unsigned_opr = e1;
    e1_is_signed = false;
  }

  if (compare_rank_ty(unsigned_opr->type, signed_opr->type) >= 0) {
    *signed_opr = *build_conversion_to(copy_Type(unsigned_opr->type), indirect(signed_opr),
                                       e1_is_signed && e1_is_lval);
    sema_expr(env, signed_opr);
    return copy_Type(unsigned_opr->type);
  }

  if (is_representable_in_ty(unsigned_opr->type, signed_opr->type)) {
    *unsigned_opr = *build_conversion_to(copy_Type(signed_opr->type), indirect(unsigned_opr),
                                         !(e1_is_signed && e1_is_lval));
    sema_expr(env, unsigned_opr);
    return copy_Type(signed_opr->type);
  }

  Type* t = to_unsigned_ty(signed_opr->type);
  if (e1_is_lval) {
    *signed_opr   = *build_conversion_to(t, indirect(signed_opr), e1_is_signed);
    *unsigned_opr = *build_conversion_to(copy_Type(t), indirect(unsigned_opr), !e1_is_signed);
  } else {
    *signed_opr   = *build_conversion_to(t, indirect(signed_opr), false);
    *unsigned_opr = *build_conversion_to(copy_Type(t), indirect(unsigned_opr), false);
  }
  sema_expr(env, signed_opr);
  sema_expr(env, unsigned_opr);
  return copy_Type(t);
}

// conversion as if by assignment
// conversion is simplified to a cast expression
// mainly this performs the complex constraint check of assignment
static void assignment_conversion(Env* env, Type* lhs_ty, Expr* rhs) {
  // TODO: check qualifiers
  // TODO: check structs
  Type* rhs_ty = rhs->type;

  if (is_arithmetic_ty(lhs_ty) && is_arithmetic_ty(rhs_ty)) {
    goto convertible;
  }

  if (is_pointer_ty(lhs_ty) && is_pointer_ty(rhs_ty)) {
    if (is_compatible_ty(lhs_ty->ptr_to, rhs_ty->ptr_to)) {
      goto convertible;
    }

    if (lhs_ty->ptr_to->kind == TY_VOID || rhs_ty->ptr_to->kind == TY_VOID) {
      goto convertible;
    }
  }

  if (is_pointer_ty(lhs_ty) && is_null_pointer_constant(rhs)) {
    goto convertible;
  }

  if (lhs_ty->kind == TY_BOOL && is_pointer_ty(rhs_ty)) {
    goto convertible;
  }

  print_Type(stderr, rhs_ty);
  fprintf(stderr, " and ");
  print_Type(stderr, lhs_ty);
  error(" is not assignable");

convertible:
  // TODO: shallow release
  *rhs = *build_conversion_to(copy_Type(lhs_ty), indirect(rhs), false);
  sema_expr(env, rhs);
}

static Type* sema_binop_simple(BinopKind op, Expr* lhs, Expr* rhs) {
  // NOTE: arithmetic conversion should be performed before `sema_binop_simple`

  Type* lhs_ty = lhs->type;
  Type* rhs_ty = rhs->type;

  switch (op) {
    case BINOP_ADD:
    case BINOP_SUB:
    case BINOP_MUL:
    case BINOP_DIV:
    case BINOP_SHIFT_RIGHT:
    case BINOP_SHIFT_LEFT:
    case BINOP_AND:
    case BINOP_XOR:
    case BINOP_OR:
    case BINOP_REM:
      should_integer(lhs_ty);
      should_integer(rhs_ty);
      return copy_Type(lhs_ty);
    case BINOP_EQ:
    case BINOP_NE:
      if (is_arithmetic_ty(lhs_ty) && is_arithmetic_ty(rhs_ty)) {
        return copy_Type(lhs_ty);
      }
      if (is_pointer_ty(lhs_ty) && is_null_pointer_constant(rhs)) {
        *rhs = *build_conversion_to(copy_Type(lhs_ty), indirect(rhs), false);
        return int_ty();
      }
      if (is_pointer_ty(rhs_ty) && is_null_pointer_constant(lhs)) {
        *lhs = *build_conversion_to(copy_Type(rhs_ty), indirect(lhs), false);
        return int_ty();
      }
      should_pointer(lhs_ty);
      should_pointer(rhs_ty);
      should_compatible(lhs_ty->ptr_to, rhs_ty->ptr_to);
      return int_ty();
    case BINOP_GT:
    case BINOP_GE:
    case BINOP_LT:
    case BINOP_LE:
      if (is_real_ty(lhs_ty) && is_real_ty(rhs_ty)) {
        return copy_Type(lhs_ty);
      }
      should_pointer(lhs_ty);
      should_pointer(rhs_ty);
      should_compatible(lhs_ty->ptr_to, rhs_ty->ptr_to);
      return int_ty();
    default:
      CCC_UNREACHABLE;
  }
}

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
      // fallthrouth
    case BINOP_SUB:
      if (is_pointer_ty(lhs) && is_pointer_ty(rhs)) {
        should_compatible(lhs->ptr_to, rhs->ptr_to);

        // TODO: shallow release of rhs of this assignment
        *expr = *build_pointer_diff(expr->lhs, expr->rhs);

        Type* ty = ptrdiff_t_ty();
        sema_expr(env, expr);
        assert(equal_to_Type(expr->type, ty));

        return ty;
      }
      if (is_pointer_ty(lhs) && is_integer_ty(rhs)) {
        // TODO: shallow release of rhs of this assignment
        *expr = *build_pointer_arith(op, expr->lhs, expr->rhs);

        Type* ty = copy_Type(lhs);
        sema_expr(env, expr);
        assert(equal_to_Type(expr->type, ty));

        return ty;
      }
      // fallthrouth
    default:
      if (is_arithmetic_ty(lhs) && is_arithmetic_ty(rhs)) {
        arithmetic_conversion(env, expr->lhs, expr->rhs, false);
      }
      return sema_binop_simple(op, expr->lhs, expr->rhs);
  }
}

static Type* sema_expr_raw(Env* env, Expr* expr);

static Type* sema_unaop(Env* env, Expr* e) {
  Type* ty = sema_expr(env, e->expr);
  switch (e->unaop) {
    case UNAOP_POSITIVE:
    case UNAOP_INTEGER_NEG: {
      should_arithmetic(ty);
      return integer_promotion(env, e->expr, false);
    }
    case UNAOP_BITWISE_NEG: {
      should_integer(ty);
      return integer_promotion(env, e->expr, false);
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
      if (expr->cast_type == NULL) {
        expr->cast_type = translate_type_name(env, expr->cast_to);
      }
      // TODO: Check floating type
      should_scalar(ty);
      t = copy_Type(expr->cast_type);
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
      sema_expr(env, expr->rhs);
      assignment_conversion(env, lhs_ty, expr->rhs);
      t = copy_Type(lhs_ty);
      break;
    }
    case ND_COMMA: {
      sema_expr(env, expr->lhs);
      t = copy_Type(sema_expr(env, expr->rhs));
      break;
    }
    case ND_COMPOUND_ASSIGN: {
      Type* lhs_ty = sema_expr(env, expr->lhs);
      Type* rhs_ty = sema_expr(env, expr->rhs);
      switch (expr->binop) {
        case BINOP_ADD:
        case BINOP_SUB:
          if (is_pointer_ty(lhs_ty) && is_integer_ty(rhs_ty)) {
            // TODO: shallow release of rhs of this assignment
            *expr = *build_pointer_arith_assign(expr->binop, expr->lhs, expr->rhs);

            Type* ty = copy_Type(lhs_ty);
            sema_expr(env, expr);
            assert(equal_to_Type(expr->type, ty));

            t = ty;
            break;
          }
          // fallthrough
        default: {
          should_arithmetic(lhs_ty);
          should_arithmetic(rhs_ty);
          arithmetic_conversion(env, expr->lhs, expr->rhs, true);
          sema_binop_simple(expr->binop, expr->lhs, expr->rhs);
          assignment_conversion(env, lhs_ty, expr->rhs);
          t = copy_Type(lhs_ty);
          break;
        }
      }
      break;
    }
    case ND_ADDR: {
      Type* ty = sema_expr_raw(env, expr->expr);
      t        = ptr_to_ty(copy_Type(ty));
      break;
    }
    case ND_ADDR_ARY: {
      Type* ty = sema_expr_raw(env, expr->expr);
      assert(is_array_ty(ty));
      t = ptr_to_ty(copy_Type(ty->element));
      break;
    }
    case ND_DEREF: {
      Type* ty = sema_expr(env, expr->expr);
      should_pointer(ty);
      t = copy_Type(ty->ptr_to);
      break;
    }
    case ND_COND: {
      Type* cond_ty = sema_expr(env, expr->cond);
      Type* then_ty = sema_expr(env, expr->then_);
      Type* else_ty = sema_expr(env, expr->else_);
      should_scalar(cond_ty);
      if (is_arithmetic_ty(then_ty) && is_arithmetic_ty(else_ty)) {
        t = arithmetic_conversion(env, expr->then_, expr->else_, false);
        break;
      }
      if (is_pointer_ty(then_ty) && is_pointer_ty(else_ty)) {
        should_compatible(then_ty, else_ty);
        // TODO: composite type
        t = copy_Type(then_ty);
        break;
      }
      if (is_pointer_ty(then_ty) && is_null_pointer_constant(expr->else_)) {
        *expr->else_ = *build_conversion_to(copy_Type(then_ty), indirect(expr->else_), false);
        t            = copy_Type(then_ty);
        break;
      }
      if (is_pointer_ty(else_ty) && is_null_pointer_constant(expr->then_)) {
        *expr->then_ = *build_conversion_to(copy_Type(else_ty), indirect(expr->then_), false);
        t            = copy_Type(else_ty);
        break;
      }
      // TODO: handle void
      // TODO: check that operands has structure or union type
      should_same(then_ty, else_ty);
      t = copy_Type(then_ty);
      break;
    }
    case ND_VAR:
      t = get_var(env, expr->var);
      break;
    case ND_NUM:
      t = int_ty();
      break;
    case ND_STRING:
      t         = array_ty(char_ty(), true);
      t->length = expr->str_len + 1;
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
        Expr* a = get_ExprVec(expr->args, i);
        sema_expr(env, a);
        Type* p_ty = get_TypeVec(f->params, i);

        assignment_conversion(env, p_ty, a);
      }

      t = copy_Type(f->ret);
      break;
    }
    case ND_SIZEOF_TYPE: {
      Type* ty = translate_type_name(env, expr->sizeof_);

      // TODO: shallow release of rhs of this assignment
      *expr = *new_node_num(sizeof_ty(ty));
      // TODO: release previous content of `expr`

      t = size_t_ty();
      break;
    }
    case ND_SIZEOF_EXPR: {
      Type* ty = sema_expr_raw(env, expr->expr);
      should_complete(ty);

      // TODO: shallow release of rhs of this assignment
      *expr = *new_node_num(sizeof_ty(ty));
      // TODO: release previous content of `expr`

      t = size_t_ty();
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
      // TODO: shallow release of rhs of this assignment
      *e = *build_decay(indirect(e));
      return sema_expr(env, e);
    }
    default:
      return ty;
  }
}

// `type` will be consumed
static Initializer* build_empty_initializer(Type* type) {
  if (is_scalar_ty(type)) {
    Initializer* empty = new_Initializer(IN_EXPR);
    empty->expr        = new_cast_direct(type, new_node_num(0));
    return empty;
  } else {
    Initializer* empty = new_Initializer(IN_LIST);
    empty->list        = nil_InitializerList();
    return empty;
  }
}

static void sema_initializer(Env* env, Type* type, Initializer* init);

static void sema_aggr_initializer_list(Env* env, Type* type, InitializerList* l) {
  assert(is_array_ty(type));

  InitializerList* cur = l;

  for (unsigned i = 0; i < length_of_ty(type); i++) {
    if (is_nil_InitializerList(cur)) {
      Initializer* empty = build_empty_initializer(copy_Type(type->element));
      sema_initializer(env, type->element, empty);

      cur = snoc_InitializerList(empty, cur);
      cur = tail_InitializerList(cur);  // set to be NULL again
    } else {
      Initializer* head = head_InitializerList(cur);
      sema_initializer(env, type->element, head);

      cur = tail_InitializerList(cur);
    }
  }
}

static void sema_scalar_initializer_list(Env* env, Type* type, InitializerList* l) {
  if (is_nil_InitializerList(l)) {
    error("scalar initializer cannot be empty");
  }

  Initializer* init = head_InitializerList(l);
  if (init->kind != IN_EXPR) {
    error("too many braces around scalar initializer");
  }

  sema_initializer(env, type, init);

  if (!is_nil_InitializerList(tail_InitializerList(l))) {
    error("excess elements in scalar initializer");
  }
}

static Initializer* build_string_initializer(Env* env, Type* type, Expr* init) {
  assert(type->kind == TY_ARRAY);

  // string literal initializer
  if (init->str_len > length_of_ty(type)) {
    error("initializer-string for char array is too long");
  }

  // rewrite `= "hello"` to `= {'h', 'e', 'l', 'l', 'o', 0}`
  char* p               = init->string;
  InitializerList* list = nil_InitializerList();
  InitializerList* cur  = list;

  for (unsigned i = 0; i < length_of_ty(type); i++) {
    Initializer* c_init = new_Initializer(IN_EXPR);
    // TODO: remove this explicit cast by conversion
    c_init->expr = new_cast_direct(char_ty(), new_node_num(*p));
    cur          = snoc_InitializerList(c_init, cur);

    p++;
  }

  Initializer* new_init = new_Initializer(IN_LIST);
  new_init->list        = list;
  return new_init;
}

static bool is_char_array_ty(Type* type) {
  return type->kind == TY_ARRAY && is_character_ty(type->element);
}

static void sema_string_initializer(Env* env, Type* type, Initializer* init, Expr* expr) {
  // TODO: release rhs of the assignment
  *init = *build_string_initializer(env, type, expr);
  sema_initializer(env, type, init);
}

static void sema_initializer(Env* env, Type* type, Initializer* init) {
  switch (init->kind) {
    case IN_EXPR: {
      if (is_char_array_ty(type) && init->expr->kind == ND_STRING) {
        sema_string_initializer(env, type, init, init->expr);
        break;
      }

      sema_expr(env, init->expr);
      assignment_conversion(env, type, init->expr);
      break;
    }
    case IN_LIST: {
      // check string initializer
      if (!is_nil_InitializerList(init->list)) {
        Initializer* head = head_InitializerList(init->list);
        if (is_char_array_ty(type) && is_single_InitializerList(init->list) &&
            head->kind == IN_EXPR && head->expr->kind == ND_STRING) {
          sema_string_initializer(env, type, init, head->expr);
          break;
        }
      }

      if (is_scalar_ty(type)) {
        sema_scalar_initializer_list(env, type, init->list);
      } else {
        sema_aggr_initializer_list(env, type, init->list);
      }
      break;
    }
    default:
      CCC_UNREACHABLE;
  }
}

static void sema_init_declarator(Env* env, bool is_global, Type* base_ty, InitDeclarator* decl) {
  char* name;
  Type* ty;
  extract_declarator(decl->declarator, base_ty, &name, &ty);

  if (ty->kind == TY_ARRAY && !ty->is_length_known && decl->initializer != NULL) {
    // enable to guess the length of array!
    if (decl->initializer->kind != IN_LIST) {
      error("invalid initializer");
    }

    // TODO: it is expensive to calculate the length of list
    unsigned length = length_InitializerList(decl->initializer->list);
    set_length_ty(ty, length);
  }

  // TODO: check linkage
  should_complete(ty);
  decl->type = copy_Type(ty);

  if (is_global) {
    add_global(env->global, name, ty);
  } else {
    add_var(env, name, ty);
  }

  if (is_global && decl->initializer == NULL) {
    decl->initializer = build_empty_initializer(copy_Type(decl->type));
  }

  if (decl->initializer != NULL) {
    sema_initializer(env, decl->type, decl->initializer);
  }
}

static void sema_init_decl_list(Env* env, bool is_global, Type* base_ty, InitDeclaratorList* l) {
  if (is_nil_InitDeclaratorList(l)) {
    return;
  }
  sema_init_declarator(env, is_global, base_ty, head_InitDeclaratorList(l));
  sema_init_decl_list(env, is_global, base_ty, tail_InitDeclaratorList(l));
}

static void sema_decl(Env* env, Declaration* decl) {
  Type* base_ty = translate_declaration_specifiers(env, decl->spec);
  sema_init_decl_list(env, false, base_ty, decl->declarators);
}

static void sema_items(Env* env, BlockItemList* l);

static void sema_stmt(Env* env, Statement* stmt) {
  switch (stmt->kind) {
    case ST_COMPOUND: {
      // block
      TypeMap* save = env->vars;
      TypeMap* inst = shallow_copy_TypeMap(env->vars);

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
      if (stmt->expr == NULL) {
        should_compatible(env->ret_ty, void_ty());
      } else {
        sema_expr(env, stmt->expr);
        assignment_conversion(env, env->ret_ty, stmt->expr);
      }
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
    case ST_GOTO:
      break;
    case ST_LABEL:
      stmt->label_id = add_named_label(env, stmt->label_name);
      sema_stmt(env, stmt->body);
      break;
    case ST_SWITCH: {
      sema_expr(env, stmt->expr);
      Statement* old = start_switch(env, stmt);
      sema_stmt(env, stmt->body);
      if (old != NULL) {
        start_switch(env, old);
      }
      break;
    }
    case ST_CASE: {
      set_case(env, stmt);
      stmt->case_value = eval_constant(stmt->expr);
      stmt->label_id   = add_anon_label(env);
      sema_stmt(env, stmt->body);
      break;
    }
    case ST_DEFAULT: {
      set_default(env, stmt);
      stmt->label_id = add_anon_label(env);
      sema_stmt(env, stmt->body);
      break;
    }
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

// if `env` is NULL, this accepts abstract declarator
static TypeVec* param_types(Env* env, ParamList* cur) {
  TypeVec* params = new_TypeVec(2);
  while (!is_nil_ParamList(cur)) {
    ParameterDecl* d = head_ParamList(cur);
    Type* base_ty    = translate_declaration_specifiers(env, d->spec);
    if (base_ty->kind == TY_VOID) {
      if (length_TypeVec(params) != 0 || !is_nil_ParamList(tail_ParamList(cur))) {
        error("void must be the first and only parameter if specified");
      }
      if (!is_abstract_declarator(d->decl)) {
        error("argument may not have void type");
      }
      return params;
    }

    if (env == NULL) {
      Type* type;
      extract_declarator(d->decl, base_ty, NULL, &type);
      push_TypeVec(params, type);
    } else {
      // must be declarator
      if (is_abstract_declarator(d->decl)) {
        error("parameter name omitted");
      }
      Type* type;
      char* name;
      extract_declarator(d->decl, base_ty, &name, &type);
      push_TypeVec(params, type);
      add_var(env, name, copy_Type(type));
    }
    cur = tail_ParamList(cur);
  }
  return params;
}

static void sema_function(GlobalEnv* global, FunctionDef* f) {
  Type* base_ty = translate_declaration_specifiers(fake_env(global), f->spec);
  Type* ret;
  char* name;
  extract_declarator(f->decl, base_ty, &name, &ret);

  Env* env        = init_Env(global, ret);
  TypeVec* params = param_types(env, f->params);
  Type* ty        = func_ty(ret, params);
  f->type         = copy_Type(ty);

  add_global(global, name, ty);
  sema_items(env, f->items);

  f->named_labels = env->named_labels;
  f->label_count  = env->label_count;

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
      Type* base_ty   = translate_declaration_specifiers(fake_env(global), f->spec);
      Type* ret;
      char* name;
      extract_declarator(f->decl, base_ty, &name, &ret);
      TypeVec* params = param_types(NULL, f->params);
      Type* ty        = func_ty(ret, params);
      f->type         = copy_Type(ty);
      add_global(global, name, ty);
      break;
    }
    case EX_DECL: {
      Declaration* decl = d->decl;
      Type* base_ty     = translate_declaration_specifiers(fake_env(global), decl->spec);
      // TODO: check if the declaration is `extern`
      sema_init_decl_list(fake_env(global), true, base_ty, decl->declarators);
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
