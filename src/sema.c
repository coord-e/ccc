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

static Type* ptrify(Type* base, unsigned num) {
  if (num == 0) {
    return base;
  } else {
    return ptrify(ptr_to_ty(base), num - 1);
  }
}

static Type* sema_expr(Env* env, Expr* expr);
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
