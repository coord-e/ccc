#include "ir.h"
#include "const_fold_tree.h"
#include "error.h"
#include "map.h"
#include "parser.h"

Reg* new_Reg(RegKind kind, DataSize size) {
  Reg* r  = calloc(1, sizeof(Reg));
  r->kind = kind;
  r->size = size;
  return r;
}

Reg* new_virtual_Reg(DataSize size, unsigned virtual) {
  Reg* r     = new_Reg(REG_VIRT, size);
  r->virtual = virtual;
  return r;
}

Reg* new_real_Reg(DataSize size, unsigned real) {
  Reg* r  = new_Reg(REG_REAL, size);
  r->real = real;
  return r;
}

Reg* new_fixed_Reg(DataSize size, unsigned virtual, unsigned real) {
  Reg* r     = new_Reg(REG_FIXED, size);
  r->virtual = virtual;
  r->real    = real;
  return r;
}

Reg* copy_Reg(Reg* r) {
  Reg* new = new_Reg(r->kind, r->size);
  *new     = *r;
  return new;
}

IRInst* new_inst(unsigned local, unsigned global, IRInstKind kind) {
  IRInst* i    = calloc(1, sizeof(IRInst));
  i->kind      = kind;
  i->local_id  = local;
  i->global_id = global;

  i->ras = new_RegVec(1);
  return i;
}

void release_Reg(Reg* r) {
  free(r);
}

void release_inst(IRInst* i) {
  release_RegVec(i->ras);
  release_Reg(i->rd);
  free(i->global_name);
  free(i);
}

DEFINE_LIST(release_inst, IRInst*, IRInstList)
DEFINE_VECTOR(release_inst, IRInst*, IRInstVec)

DEFINE_VECTOR(release_Reg, Reg*, RegVec)

void release_BasicBlock(BasicBlock* bb) {
  if (bb == NULL) {
    return;
  }

  release_IRInstList(bb->insts);

  release_BitSet(bb->live_gen);
  release_BitSet(bb->live_kill);
  release_BitSet(bb->live_in);
  release_BitSet(bb->live_out);
  release_BitSet(bb->reach_gen);
  release_BitSet(bb->reach_kill);
  release_BitSet(bb->reach_in);
  release_BitSet(bb->reach_out);

  release_BitSet(bb->should_preserve);

  free(bb);
}

DECLARE_MAP(BasicBlock*, BBMap)
DEFINE_DLIST(release_BasicBlock, BasicBlock*, BBList)
static void release_ref(void* p) {}
DEFINE_DLIST(release_ref, BasicBlock*, BBRefList)
DEFINE_VECTOR(release_BasicBlock, BasicBlock*, BBVec)
DEFINE_VECTOR(release_BitSet, BitSet*, BSVec)

static void release_Function(Function* f) {
  free(f->name);
  release_BBList(f->blocks);
  release_RegIntervals(f->intervals);
  release_BitSet(f->used_fixed_regs);
  release_BSVec(f->definitions);
  free(f);
}

DEFINE_LIST(release_Function, Function*, FunctionList)

static GlobalVar* new_GlobalVar(char* name, GlobalInitializer* init) {
  GlobalVar* v = calloc(1, sizeof(GlobalVar));
  v->name      = name;
  v->init      = init;
  return v;
}

static GlobalExpr* new_GlobalExpr(GlobalExprKind kind) {
  GlobalExpr* expr = calloc(1, sizeof(GlobalExpr));
  expr->kind       = kind;
  return expr;
}

static void release_GlobalExpr(GlobalExpr* expr) {
  if (expr == NULL) {
    return;
  }

  free(expr->lhs);
  free(expr->string);
  free(expr);
}

DEFINE_LIST(release_GlobalExpr, GlobalExpr*, GlobalInitializer)

static void release_GlobalVar(GlobalVar* v) {
  if (v == NULL) {
    return;
  }

  free(v->name);
  free(v);
}

DEFINE_VECTOR(release_GlobalVar, GlobalVar*, GlobalVarVec)

typedef struct {
  unsigned bb_count;
  unsigned inst_count;
  GlobalVarVec* globals;
} GlobalEnv;

static GlobalEnv* init_GlobalEnv() {
  GlobalEnv* env = calloc(1, sizeof(GlobalEnv));
  env->globals   = new_GlobalVarVec(16);
  return env;
}

static void release_GlobalEnv(GlobalEnv* env) {
  free(env);
}

static void add_normal_gvar(GlobalEnv* env, const char* name, GlobalInitializer* init) {
  GlobalVar* gv = new_GlobalVar(strdup(name), init);
  push_GlobalVarVec(env->globals, gv);
}

static void add_string_gvar(GlobalEnv* env, const char* name, const char* str) {
  GlobalExpr* expr = new_GlobalExpr(GE_STRING);
  expr->string     = strdup(str);
  add_normal_gvar(env, name, single_GlobalInitializer(expr));
}

typedef struct {
  unsigned reg_count;
  unsigned stack_count;
  unsigned bb_count;
  unsigned inst_count;

  unsigned call_count;

  UIMap* vars;
  BBList* blocks;
  BBVec* labels;
  UIMap* named_labels;

  BasicBlock* entry;
  BasicBlock* exit;

  BasicBlock* cur;
  IRInstList* inst_cur;

  BasicBlock* loop_break;
  BasicBlock* loop_continue;

  GlobalEnv* global_env;
} Env;

static IRInst* new_inst_(Env* env, IRInstKind kind) {
  return new_inst(env->inst_count++, env->global_env->inst_count++, kind);
}

static BasicBlock* new_bb(Env* env) {
  unsigned local_id  = env->bb_count++;
  unsigned global_id = env->global_env->bb_count++;

  IRInst* inst = new_inst_(env, IR_LABEL);

  BasicBlock* bb = calloc(1, sizeof(BasicBlock));
  inst->label    = bb;

  bb->local_id  = local_id;
  bb->global_id = global_id;
  bb->insts     = single_IRInstList(inst);
  bb->succs     = new_BBRefList();
  bb->preds     = new_BBRefList();

  push_back_BBList(env->blocks, bb);

  return bb;
}

static Env* new_env(GlobalEnv* genv, FunctionDef* f) {
  Env* env        = calloc(1, sizeof(Env));
  env->global_env = genv;
  env->vars       = new_UIMap(32);
  env->blocks     = new_BBList();

  env->exit = new_bb(env);

  env->named_labels = f->named_labels;
  env->labels       = new_BBVec(f->label_count);
  for (unsigned i = 0; i < f->label_count; i++) {
    push_BBVec(env->labels, new_bb(env));
  }

  return env;
}

static void connect_bb(BasicBlock* from, BasicBlock* to) {
  push_back_BBRefList(from->succs, to);
  push_back_BBRefList(to->preds, from);
}

static void start_bb(Env* env, BasicBlock* bb) {
  env->cur      = bb;
  env->inst_cur = env->cur->insts;
}

static BasicBlock* get_label(Env* env, unsigned id) {
  return get_BBVec(env->labels, id);
}

static BasicBlock* get_named_label(Env* env, const char* name) {
  unsigned id;
  if (!lookup_UIMap(env->named_labels, name, &id)) {
    error("use of undefined label \"%s\"", name);
  }

  return get_label(env, id);
}

static Reg* new_reg(Env* env, DataSize size) {
  return new_virtual_Reg(size, env->reg_count++);
}

static unsigned new_var(Env* env, char* name, unsigned size) {
  if (lookup_UIMap(env->vars, name, NULL)) {
    error("redeclaration of \"%s\"", name);
  }

  env->stack_count += size;
  unsigned i = env->stack_count;
  insert_UIMap(env->vars, name, i);
  return i;
}

static bool get_var(Env* env, char* name, unsigned* dest) {
  return lookup_UIMap(env->vars, name, dest);
}

static void add_inst(Env* env, IRInst* inst) {
  env->inst_cur = snoc_IRInstList(inst, env->inst_cur);
}

static Reg* new_binop(Env* env, BinopKind op, Reg* lhs, Reg* rhs) {
  assert(lhs->size == rhs->size);

  Reg* dest = new_reg(env, lhs->size);

  IRInst* i2 = new_inst_(env, IR_BIN);
  i2->binop  = op;
  i2->rd     = dest;
  push_RegVec(i2->ras, copy_Reg(lhs));
  push_RegVec(i2->ras, copy_Reg(rhs));
  add_inst(env, i2);

  return dest;
}

static Reg* new_unaop(Env* env, UnaopKind op, Reg* opr) {
  Reg* dest = new_reg(env, opr->size);

  IRInst* i2 = new_inst_(env, IR_UNA);
  i2->unaop  = op;
  i2->rd     = dest;
  push_RegVec(i2->ras, copy_Reg(opr));
  add_inst(env, i2);

  return dest;
}

static Reg* new_imm(Env* env, int num, DataSize size) {
  Reg* r    = new_reg(env, size);
  IRInst* i = new_inst_(env, IR_IMM);
  i->imm    = num;
  i->rd     = r;
  add_inst(env, i);
  return r;
}

static Reg* new_stack_addr(Env* env, unsigned s) {
  Reg* r       = new_reg(env, SIZE_QWORD);  // TODO: hardcoded pointer size
  IRInst* i    = new_inst_(env, IR_STACK_ADDR);
  i->stack_idx = s;
  i->rd        = r;
  add_inst(env, i);
  return r;
}

static Reg* new_load(Env* env, Reg* s, DataSize size) {
  Reg* r    = new_reg(env, size);
  IRInst* i = new_inst_(env, IR_LOAD);
  push_RegVec(i->ras, copy_Reg(s));
  i->rd        = r;
  i->data_size = size;
  add_inst(env, i);
  return r;
}

static void new_store(Env* env, Reg* s, Reg* r, DataSize size) {
  assert(r->size == size);

  IRInst* i = new_inst_(env, IR_STORE);
  push_RegVec(i->ras, copy_Reg(s));
  push_RegVec(i->ras, copy_Reg(r));
  i->data_size = size;
  add_inst(env, i);
}

static Reg* new_sext(Env* env, Reg* t, DataSize to) {
  assert(t->size < to);

  Reg* r    = new_reg(env, to);
  IRInst* i = new_inst_(env, IR_SEXT);
  push_RegVec(i->ras, copy_Reg(t));
  i->rd        = r;
  i->data_size = to;
  add_inst(env, i);
  return r;
}

static Reg* new_trunc(Env* env, Reg* t, DataSize to) {
  assert(t->size > to);

  Reg* r    = new_reg(env, to);
  IRInst* i = new_inst_(env, IR_TRUNC);
  push_RegVec(i->ras, copy_Reg(t));
  i->rd        = r;
  i->data_size = to;
  add_inst(env, i);
  return r;
}

static Reg* nth_arg(Env* env, unsigned nth, DataSize size) {
  Reg* r          = new_reg(env, size);
  IRInst* i       = new_inst_(env, IR_ARG);
  i->argument_idx = nth;
  i->rd           = r;
  add_inst(env, i);
  return r;
}

static void new_move(Env* env, Reg* d, Reg* s) {
  IRInst* i = new_inst_(env, IR_MOV);
  push_RegVec(i->ras, copy_Reg(s));
  i->rd = copy_Reg(d);
  add_inst(env, i);
}

static void new_jump(Env* env, BasicBlock* jump, BasicBlock* next);

static bool is_exit(IRInstKind k) {
  switch (k) {
    case IR_JUMP:
    case IR_BR:
    case IR_RET:
      return true;
    default:
      return false;
  }
}

static void create_or_start_bb(Env* env, BasicBlock* bb) {
  if (!bb) {
    bb = new_bb(env);
  }
  if (!is_exit(head_IRInstList(env->inst_cur)->kind)) {
    new_jump(env, bb, bb);
  } else {
    start_bb(env, bb);
  }
}

void new_jump(Env* env, BasicBlock* jump, BasicBlock* next) {
  IRInst* i = new_inst_(env, IR_JUMP);
  i->jump   = jump;
  add_inst(env, i);

  connect_bb(env->cur, jump);

  create_or_start_bb(env, next);
}

static void new_exit_ret(Env* env) {
  add_inst(env, new_inst_(env, IR_RET));
}

static void new_void_ret(Env* env, BasicBlock* next) {
  add_inst(env, new_inst_(env, IR_RET));

  connect_bb(env->cur, env->exit);
  create_or_start_bb(env, next);
}

static Reg* new_ret(Env* env, Reg* r, BasicBlock* next) {
  IRInst* i = new_inst_(env, IR_RET);
  push_RegVec(i->ras, copy_Reg(r));
  add_inst(env, i);

  connect_bb(env->cur, env->exit);
  create_or_start_bb(env, next);

  return r;
}

static void new_br(Env* env, Reg* r, BasicBlock* then_, BasicBlock* else_, BasicBlock* next) {
  IRInst* i = new_inst_(env, IR_BR);
  push_RegVec(i->ras, copy_Reg(r));
  i->then_ = then_;
  i->else_ = else_;
  add_inst(env, i);

  connect_bb(env->cur, then_);
  connect_bb(env->cur, else_);

  create_or_start_bb(env, next);
}

static Reg* new_global(Env* env, const char* name, GlobalNameKind kind) {
  Reg* r            = new_reg(env, SIZE_QWORD);  // TODO: hardcoded pointer size
  IRInst* inst      = new_inst_(env, IR_GLOBAL_ADDR);
  inst->rd          = r;
  inst->global_name = strdup(name);
  inst->global_kind = kind;
  add_inst(env, inst);
  return r;
}

static char* new_named_string(GlobalEnv* env, const char* str) {
  unsigned i = length_GlobalVarVec(env->globals);
  // TODO: allocate accurate length of string
  char name[10];
  sprintf(name, "_s_%d", i);
  add_string_gvar(env, name, str);
  return strdup(name);
}

static Reg* new_string(Env* env, const char* str) {
  char* name = new_named_string(env->global_env, str);
  Reg* r     = new_global(env, name, GN_DATA);
  free(name);
  return r;
}

static Reg* gen_expr(Env* env, Expr* node);

static Reg* gen_lhs(Env* env, Expr* node) {
  switch (node->kind) {
    case ND_VAR: {
      unsigned i;
      if (get_var(env, node->var, &i)) {
        return new_stack_addr(env, i);
      } else {
        if (node->type->kind == TY_FUNC) {
          return new_global(env, node->var, GN_FUNCTION);
        } else {
          return new_global(env, node->var, GN_DATA);
        }
      }
    }
    case ND_MEMBER: {
      assert(is_complete_ty(node->expr->type));
      Reg* r   = gen_lhs(env, node->expr);
      Field* f = get_FieldMap(node->expr->type->field_map, node->member);
      return new_binop(env, BINOP_ADD, r, new_imm(env, f->offset, r->size));
    }
    case ND_STRING:
      return new_string(env, node->string);
    case ND_DEREF:
      return gen_expr(env, node->expr);
    default:
      error("invaild lhs");
  }
}

static DataSize datasize_of_node(Expr* e) {
  return to_data_size(sizeof_ty(e->type));
}

Reg* gen_expr(Env* env, Expr* node) {
  switch (node->kind) {
    case ND_CAST: {
      // TODO: signedness?
      unsigned cast_size = sizeof_ty(node->cast_type);
      unsigned expr_size = sizeof_ty(node->expr->type);
      Reg* r             = gen_expr(env, node->expr);
      if (cast_size > expr_size) {
        return new_sext(env, r, cast_size);
      } else if (cast_size < expr_size) {
        return new_trunc(env, r, cast_size);
      } else {
        return r;
      }
    }
    case ND_NUM:
      return new_imm(env, node->num, datasize_of_node(node));
    case ND_BINOP: {
      Reg* lhs = gen_expr(env, node->lhs);
      Reg* rhs = gen_expr(env, node->rhs);
      return new_binop(env, node->binop, lhs, rhs);
    }
    case ND_UNAOP: {
      Reg* r = gen_expr(env, node->expr);
      return new_unaop(env, node->unaop, r);
    }
    case ND_ADDR:
    case ND_ADDR_ARY:
      return gen_lhs(env, node->expr);
    case ND_DEREF: {
      Reg* r = gen_expr(env, node->expr);
      if (node->type->kind == TY_STRUCT) {
        // NOTE: same representation (pointer to the head) is used for a struct value and lvalue
        return r;
      } else {
        return new_load(env, r, datasize_of_node(node));
      }
    }
    case ND_COMPOUND_ASSIGN: {
      Reg* addr = gen_lhs(env, node->lhs);
      Reg* rhs  = gen_expr(env, node->rhs);
      Reg* lhs  = new_load(env, addr, datasize_of_node(node->lhs));
      Reg* val  = new_binop(env, node->binop, lhs, rhs);
      assert(sizeof_ty(node->lhs->type) == val->size);
      new_store(env, addr, val, datasize_of_node(node));
      return val;
    }
    case ND_ASSIGN: {
      Reg* addr = gen_lhs(env, node->lhs);
      Reg* rhs  = gen_expr(env, node->rhs);
      assert(sizeof_ty(node->lhs->type) == sizeof_ty(node->rhs->type));
      new_store(env, addr, rhs, datasize_of_node(node));
      return rhs;
    }
    case ND_COMMA: {
      gen_expr(env, node->lhs);
      return gen_expr(env, node->rhs);
    }
    case ND_STRING:
      assert(is_array_ty(node->type));
      error("attempt to perform lvalue conversion on string constant");
    case ND_MEMBER:
    case ND_VAR: {
      // lvalue conversion is performed here
      if (is_array_ty(node->type)) {
        error("attempt to perform lvalue conversion on array value");
      }
      Reg* r = gen_lhs(env, node);
      if (node->type->kind == TY_STRUCT) {
        // NOTE: same representation (pointer to the head) is used for a struct value and lvalue
        return r;
      } else {
        return new_load(env, r, datasize_of_node(node));
      }
    }
    case ND_CALL: {
      BasicBlock* call_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      call_bb->is_call_bb = true;

      IRInst* inst = new_inst_(env, IR_CALL);
      Reg* f       = gen_expr(env, node->lhs);
      push_RegVec(inst->ras, copy_Reg(f));

      for (unsigned i = 0; i < length_ExprVec(node->args); i++) {
        Expr* e = get_ExprVec(node->args, i);
        Reg* r  = gen_expr(env, e);
        push_RegVec(inst->ras, copy_Reg(r));
      }

      Reg* r   = new_reg(env, datasize_of_node(node));
      inst->rd = r;

      assert(node->lhs->type->kind == TY_PTR && node->lhs->type->ptr_to->kind == TY_FUNC);
      inst->is_vararg = node->lhs->type->ptr_to->is_vararg;

      env->call_count++;

      new_jump(env, call_bb, call_bb);
      add_inst(env, inst);
      new_jump(env, next_bb, next_bb);
      return r;
    }
    case ND_COND: {
      Reg* r = new_reg(env, datasize_of_node(node));

      BasicBlock* then_bb = new_bb(env);
      BasicBlock* else_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      Reg* cond = gen_expr(env, node->cond);
      new_br(env, cond, then_bb, else_bb, then_bb);

      // then
      Reg* then_ = gen_expr(env, node->then_);
      new_move(env, r, then_);
      new_jump(env, next_bb, else_bb);

      // else
      Reg* else_ = gen_expr(env, node->else_);
      new_move(env, r, else_);
      new_jump(env, next_bb, next_bb);

      // TODO: `r` is leaked

      return r;
    }
    case ND_SIZEOF_TYPE:
    case ND_SIZEOF_EXPR:
    // `sizeof` must be processed `sema`
    default:
      CCC_UNREACHABLE;
  }
}

BasicBlock* set_break(Env* env, BasicBlock* break_) {
  BasicBlock* save = env->loop_break;
  env->loop_break  = break_;
  return save;
}

BasicBlock* set_continue(Env* env, BasicBlock* continue_) {
  BasicBlock* save   = env->loop_continue;
  env->loop_continue = continue_;
  return save;
}

void gen_block_item_list(Env* env, BlockItemList* ast);

static UIMap* start_scope(Env* env) {
  UIMap* save = env->vars;
  UIMap* inst = shallow_copy_UIMap(env->vars);

  env->vars = inst;

  return save;
}

static void end_scope(Env* env, UIMap* save) {
  release_UIMap(env->vars);
  env->vars = save;
}

static void gen_decl(Env* env, Declaration* decl);

static void gen_stmt(Env* env, Statement* stmt) {
  switch (stmt->kind) {
    case ST_EXPRESSION:
      gen_expr(env, stmt->expr);
      break;
    case ST_RETURN: {
      if (stmt->expr == NULL) {
        new_void_ret(env, NULL);
      } else {
        Reg* r = gen_expr(env, stmt->expr);
        new_ret(env, r, NULL);
      }
      break;
    }
    case ST_IF: {
      BasicBlock* then_bb = new_bb(env);
      BasicBlock* else_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      Reg* cond = gen_expr(env, stmt->expr);
      new_br(env, cond, then_bb, else_bb, then_bb);

      // then
      gen_stmt(env, stmt->then_);
      new_jump(env, next_bb, else_bb);

      // else
      gen_stmt(env, stmt->else_);
      new_jump(env, next_bb, next_bb);

      break;
    }
    case ST_WHILE: {
      BasicBlock* while_bb = new_bb(env);
      BasicBlock* body_bb  = new_bb(env);
      BasicBlock* next_bb  = new_bb(env);

      BasicBlock* old_break    = set_break(env, next_bb);
      BasicBlock* old_continue = set_continue(env, while_bb);

      create_or_start_bb(env, while_bb);
      Reg* cond = gen_expr(env, stmt->expr);
      new_br(env, cond, body_bb, next_bb, body_bb);

      // body
      gen_stmt(env, stmt->body);
      new_jump(env, while_bb, next_bb);

      set_break(env, old_break);
      set_continue(env, old_continue);

      break;
    }
    case ST_DO: {
      BasicBlock* body_bb = new_bb(env);
      BasicBlock* cont_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      BasicBlock* old_break    = set_break(env, next_bb);
      BasicBlock* old_continue = set_continue(env, cont_bb);

      create_or_start_bb(env, body_bb);
      gen_stmt(env, stmt->body);

      create_or_start_bb(env, cont_bb);
      Reg* cond = gen_expr(env, stmt->expr);
      new_br(env, cond, body_bb, next_bb, next_bb);

      set_break(env, old_break);
      set_continue(env, old_continue);

      break;
    }
    case ST_FOR: {
      BasicBlock* for_bb  = new_bb(env);
      BasicBlock* body_bb = new_bb(env);
      BasicBlock* cont_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      BasicBlock* old_break    = set_break(env, next_bb);
      BasicBlock* old_continue = set_continue(env, cont_bb);

      UIMap* save = start_scope(env);
      if (stmt->init_decl != NULL) {
        gen_decl(env, stmt->init_decl);
      } else if (stmt->init != NULL) {
        gen_expr(env, stmt->init);
      }
      create_or_start_bb(env, for_bb);
      Reg* cond = gen_expr(env, stmt->before);
      new_br(env, cond, body_bb, next_bb, body_bb);

      // body
      gen_stmt(env, stmt->body);
      create_or_start_bb(env, cont_bb);
      if (stmt->after != NULL) {
        gen_expr(env, stmt->after);
      }
      end_scope(env, save);
      new_jump(env, for_bb, next_bb);

      set_break(env, old_break);
      set_continue(env, old_continue);

      break;
    }
    case ST_BREAK: {
      if (env->loop_break == NULL) {
        error("invalid break statement");
      }

      new_jump(env, env->loop_break, NULL);

      break;
    }
    case ST_CONTINUE: {
      if (env->loop_continue == NULL) {
        error("invalid continue statement");
      }

      new_jump(env, env->loop_continue, NULL);

      break;
    }
    case ST_COMPOUND: {
      // compound statement is a block
      UIMap* save = start_scope(env);
      gen_block_item_list(env, stmt->items);
      end_scope(env, save);
      break;
    }
    case ST_LABEL:
    case ST_CASE:
    case ST_DEFAULT: {
      BasicBlock* next_bb = get_label(env, stmt->label_id);

      create_or_start_bb(env, next_bb);

      gen_stmt(env, stmt->body);

      break;
    }
    case ST_GOTO: {
      new_jump(env, get_named_label(env, stmt->label_name), NULL);
      break;
    }
    case ST_SWITCH: {
      Reg* r = gen_expr(env, stmt->expr);

      BasicBlock* next_bb = new_bb(env);

      BasicBlock* old_break = set_break(env, next_bb);

      for (unsigned i = 0; i < length_StmtVec(stmt->cases); i++) {
        Statement* case_    = get_StmtVec(stmt->cases, i);
        Reg* cond           = new_binop(env, BINOP_EQ, r, new_imm(env, case_->case_value, r->size));
        BasicBlock* fail_bb = new_bb(env);
        new_br(env, cond, get_label(env, case_->label_id), fail_bb, fail_bb);
      }

      if (stmt->default_ != NULL) {
        new_jump(env, get_label(env, stmt->default_->label_id), NULL);
      } else {
        new_jump(env, next_bb, NULL);
      }

      gen_stmt(env, stmt->body);

      new_jump(env, next_bb, next_bb);
      set_break(env, old_break);

      break;
    }
    case ST_NULL:
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static GlobalExpr* translate_initializer_lhs(GlobalEnv* env, Expr* expr) {
  switch (expr->kind) {
    case ND_VAR: {
      GlobalExpr* e = new_GlobalExpr(GE_NAME);
      e->name       = strdup(expr->var);
      return e;
    }
    case ND_STRING: {
      GlobalExpr* e = new_GlobalExpr(GE_NAME);
      e->name       = new_named_string(env, expr->string);
      return e;
    }
    default:
      error("global initializer element is not constant");
  }
}

static GlobalExpr* translate_initializer_expr(GlobalEnv* env, Expr* expr) {
  switch (expr->kind) {
    case ND_STRING: {
      GlobalExpr* e = new_GlobalExpr(GE_STRING);
      e->string     = strdup(expr->string);
      return e;
    }
    case ND_ADDR:
    case ND_ADDR_ARY:
      return translate_initializer_lhs(env, expr->expr);
    case ND_CAST: {
      // TODO: strictly consider types in constant evaluation
      GlobalExpr* e = translate_initializer_expr(env, expr->expr);
      e->size       = to_data_size(sizeof_ty(expr->cast_type));
      return e;
    }
    default: {
      unsigned size = to_data_size(sizeof_ty(expr->type));
      const_fold_expr(expr);

      long c;
      if (!get_constant(expr, &c)) {
        error("global initializer element is not constant");
      }
      GlobalExpr* e = new_GlobalExpr(GE_NUM);
      e->num        = c;
      e->size       = size;
      return e;
    }
  }
}

static GlobalInitializer* translate_initializer(GlobalEnv* env, Initializer* init) {
  switch (init->kind) {
    case IN_EXPR: {
      GlobalExpr* e = translate_initializer_expr(env, init->expr);
      return single_GlobalInitializer(e);
    }
    case IN_LIST: {
      GlobalInitializer* new_init = nil_GlobalInitializer();
      GlobalInitializer* gen_cur  = new_init;
      InitializerList* read_cur   = init->list;
      while (!is_nil_InitializerList(read_cur)) {
        Initializer* e = head_InitializerList(read_cur);

        GlobalInitializer* elem = translate_initializer(env, e);
        gen_cur                 = append_GlobalInitializer(gen_cur, elem);

        read_cur = tail_InitializerList(read_cur);
      }
      return new_init;
    }
    default:
      CCC_UNREACHABLE;
  }
}

static void gen_local_scalar_initializer(Env* env, Reg* target, Initializer* init, Type* type) {
  assert(is_scalar_ty(type));

  Expr* expr;
  switch (init->kind) {
    case IN_EXPR:
      expr = init->expr;
      break;
    case IN_LIST:
      // NOTE: this structure of `init` is ensured in `sema`
      expr = head_InitializerList(init->list)->expr;
      break;
    default:
      CCC_UNREACHABLE;
  }
  Reg* r = gen_expr(env, expr);
  new_store(env, target, r, datasize_of_node(expr));
}

static void gen_local_initializer(Env* env, Reg* target, Initializer* init, Type* type);

static void gen_local_array_initializer(Env* env, Reg* target, Initializer* init, Type* type) {
  if (init->kind != IN_LIST) {
    error("array initializer must be an initializer list");
  }

  InitializerList* cur = init->list;
  unsigned offset      = 0;
  // NOTE: `sema` ensured that the initializer list has the same length than the array
  while (!is_nil_InitializerList(cur)) {
    Initializer* ci = head_InitializerList(cur);
    Reg* r          = new_binop(env, BINOP_ADD, target, new_imm(env, offset, target->size));
    gen_local_initializer(env, r, ci, type->element);
    cur = tail_InitializerList(cur);
    offset += sizeof_ty(type->element);
  }
}

static void gen_local_initializer(Env* env, Reg* target, Initializer* init, Type* type) {
  if (is_scalar_ty(type)) {
    gen_local_scalar_initializer(env, target, init, type);
  } else if (is_array_ty(type)) {
    gen_local_array_initializer(env, target, init, type);
  } else {
    CCC_UNREACHABLE;
  }
}

static void gen_init_declarator(Env* env,
                                GlobalEnv* genv,
                                DeclarationSpecifiers* spec,
                                InitDeclarator* decl) {
  if (env != NULL) {
    unsigned var = new_var(env, decl->declarator->direct->name_ref, sizeof_ty(decl->type));
    Reg* r       = new_stack_addr(env, var);

    if (decl->initializer != NULL) {
      gen_local_initializer(env, r, decl->initializer, decl->type);
    }
  } else {
    assert(genv != NULL);

    if (!spec->is_extern) {
      // NOTE: `sema` ensured that all global declaration except `extern` has an initializer
      assert(decl->initializer != NULL);

      GlobalInitializer* gi = translate_initializer(genv, decl->initializer);
      add_normal_gvar(genv, decl->declarator->direct->name_ref, gi);
    }
  }
}

static void gen_init_decl_list(Env* env,
                               GlobalEnv* genv,
                               DeclarationSpecifiers* spec,
                               InitDeclaratorList* l) {
  if (is_nil_InitDeclaratorList(l)) {
    return;
  }
  gen_init_declarator(env, genv, spec, head_InitDeclaratorList(l));
  gen_init_decl_list(env, genv, spec, tail_InitDeclaratorList(l));
}

static void gen_decl(Env* env, Declaration* decl) {
  if (!decl->spec->is_typedef) {
    gen_init_decl_list(env, NULL, decl->spec, decl->declarators);
  }
}

void gen_block_item_list(Env* env, BlockItemList* ast) {
  if (is_nil_BlockItemList(ast)) {
    return;
  }

  BlockItem* item = head_BlockItemList(ast);
  switch (item->kind) {
    case BI_STMT:
      gen_stmt(env, item->stmt);
      break;
    case BI_DECL:
      gen_decl(env, item->decl);
      break;
    default:
      CCC_UNREACHABLE;
  }

  gen_block_item_list(env, tail_BlockItemList(ast));
}

static void gen_params(Env* env, FunctionDef* f, unsigned nth, ParamList* l) {
  if (is_nil_ParamList(l)) {
    return;
  }

  char* name    = head_ParamList(l)->decl->direct->name_ref;
  Type* ty      = get_TypeVec(f->type->params, nth);
  unsigned size = sizeof_ty(ty);
  new_var(env, name, size);

  unsigned addr;
  get_var(env, name, &addr);
  Reg* addr_reg = new_stack_addr(env, addr);

  Reg* rhs = nth_arg(env, nth, to_data_size(size));
  new_store(env, addr_reg, rhs, to_data_size(size));

  gen_params(env, f, nth + 1, tail_ParamList(l));
}

static Function* gen_function(GlobalEnv* genv, FunctionDef* ast) {
  Env* env = new_env(genv, ast);

  BasicBlock* entry = new_bb(env);
  start_bb(env, entry);
  // if the parameter is `void`, the lengths of `ast->params` and `ast->type->params` differs
  if (length_TypeVec(ast->type->params) != 0) {
    gen_params(env, ast, 0, ast->params);
  }
  gen_block_item_list(env, ast->items);
  create_or_start_bb(env, env->exit);
  new_exit_ret(env);

  Function* ir    = calloc(1, sizeof(Function));
  ir->name        = strdup(ast->decl->direct->name_ref);
  ir->entry       = entry;
  ir->exit        = env->cur;
  ir->bb_count    = env->bb_count;
  ir->reg_count   = env->reg_count;
  ir->stack_count = env->stack_count;
  ir->inst_count  = env->inst_count;
  ir->call_count  = env->call_count;
  ir->blocks      = env->blocks;

  // TODO: shallow release of containers
  free(env);
  return ir;
}

static FunctionList* gen_TranslationUnit(GlobalEnv* genv, FunctionList* acc, TranslationUnit* l) {
  if (is_nil_TranslationUnit(l)) {
    return acc;
  }

  ExternalDecl* d       = head_TranslationUnit(l);
  TranslationUnit* tail = tail_TranslationUnit(l);
  switch (d->kind) {
    case EX_FUNC: {
      Function* f = gen_function(genv, d->func);
      return gen_TranslationUnit(genv, cons_FunctionList(f, acc), tail);
    }
    case EX_FUNC_DECL:
      return gen_TranslationUnit(genv, acc, tail);
    case EX_DECL:
      if (!d->decl->spec->is_typedef) {
        gen_init_decl_list(NULL, genv, d->decl->spec, d->decl->declarators);
      }
      return gen_TranslationUnit(genv, acc, tail);
    default:
      CCC_UNREACHABLE;
  }
}

IR* generate_IR(AST* ast) {
  GlobalEnv* genv = init_GlobalEnv();

  IR* ir         = calloc(1, sizeof(IR));
  ir->functions  = gen_TranslationUnit(genv, nil_FunctionList(), ast);
  ir->inst_count = genv->inst_count;
  ir->bb_count   = genv->bb_count;
  ir->globals    = genv->globals;

  release_GlobalEnv(genv);

  return ir;
}

void release_IR(IR* ir) {
  release_FunctionList(ir->functions);
  release_GlobalVarVec(ir->globals);
}

void detach_BasicBlock(Function* f, BasicBlock* b) {
  // detach a block from IR and release it safely.
  // - check entry/exit
  // - remove from succs/preds
  // - remove from blocks

  assert(f->sorted_blocks == NULL);
  assert(f->entry != b);
  assert(f->exit != b);

  {
    BBRefListIterator* it = front_BBRefList(b->succs);
    while (!is_nil_BBRefListIterator(it)) {
      BasicBlock* suc = data_BBRefListIterator(it);
      erase_one_BBRefList(suc->preds, b);
      it = next_BBRefListIterator(it);
    }
  }
  {
    BBRefListIterator* it = front_BBRefList(b->preds);
    while (!is_nil_BBRefListIterator(it)) {
      BasicBlock* pre = data_BBRefListIterator(it);
      erase_one_BBRefList(pre->succs, b);
      it = next_BBRefListIterator(it);
    }
  }

  erase_one_BBList(f->blocks, b);
}

static void print_reg(FILE* p, Reg* r) {
  switch (r->kind) {
    case REG_VIRT:
      fprintf(p, "v%d", r->virtual);
      break;
    case REG_REAL:
      fprintf(p, "r%d", r->real);
      break;
    case REG_FIXED:
      fprintf(p, "f(v%d:r%d)", r->virtual, r->real);
      break;
    default:
      CCC_UNREACHABLE;
  }
  if (r->irreplaceable) {
    fputs("!", p);
  }
}

DEFINE_VECTOR_PRINTER(print_reg, ", ", "", RegVec)

void print_escaped_binop(FILE* p, BinopKind kind) {
  switch (kind) {
    case BINOP_GT:
    case BINOP_GE:
    case BINOP_LT:
    case BINOP_LE:
    case BINOP_OR:
      fputs("\\", p);
      print_binop(p, kind);
      return;
    case BINOP_SHIFT_RIGHT:
      fprintf(p, "\\>\\>");
      return;
    case BINOP_SHIFT_LEFT:
      fprintf(p, "\\<\\<");
      return;
    default:
      print_binop(p, kind);
      return;
  }
}

static void print_inst(FILE* p, IRInst* i) {
  if (i->rd != NULL) {
    print_reg(p, i->rd);
    fprintf(p, " = ");
  }
  switch (i->kind) {
    case IR_ARG:
      fprintf(p, "ARG %d", i->argument_idx);
      break;
    case IR_NOP:
      fprintf(p, "NOP");
      break;
    case IR_IMM:
      fprintf(p, "IMM %d", i->imm);
      break;
    case IR_RET:
      fprintf(p, "RET ");
      break;
    case IR_MOV:
      fprintf(p, "MOV ");
      break;
    case IR_SEXT:
      fprintf(p, "SEXT ");
      break;
    case IR_TRUNC:
      fprintf(p, "TRUNC ");
      break;
    case IR_CALL:
      fprintf(p, "CALL ");
      break;
    case IR_BIN:
      fprintf(p, "BIN ");
      print_escaped_binop(p, i->binop);
      fprintf(p, " ");
      break;
    case IR_UNA:
      fprintf(p, "UNA ");
      print_unaop(p, i->unaop);
      fprintf(p, " ");
      break;
    case IR_STACK_ADDR:
      fprintf(p, "STACK_ADDR %d %d ", i->stack_idx, i->data_size);
      break;
    case IR_STACK_LOAD:
      fprintf(p, "STACK_LOAD %d %d ", i->stack_idx, i->data_size);
      break;
    case IR_STACK_STORE:
      fprintf(p, "STACK_STORE %d %d ", i->stack_idx, i->data_size);
      break;
    case IR_LOAD:
      fprintf(p, "LOAD %d ", i->data_size);
      break;
    case IR_STORE:
      fprintf(p, "STORE %d ", i->data_size);
      break;
    case IR_BR:
      fprintf(p, "BR %d %d ", i->then_->local_id, i->else_->local_id);
      break;
    case IR_JUMP:
      fprintf(p, "JUMP %d", i->jump->local_id);
      break;
    case IR_LABEL:
      fprintf(p, "LABEL %d", i->label->local_id);
      break;
    case IR_GLOBAL_ADDR:
      fprintf(p, "GLOBAL %s", i->global_name);
      break;
    default:
      CCC_UNREACHABLE;
  }
  if (i->ras != NULL) {
    print_RegVec(p, i->ras);
  }
}

// NOTE: printers below are to print CFG in dot language
static unsigned print_graph_insts(FILE* p, IRInstList* l) {
  IRInst* i1    = head_IRInstList(l);
  IRInstList* t = tail_IRInstList(l);

  fprintf(p, "inst_%d [shape=record,fontname=monospace,label=\"%d|", i1->global_id, i1->global_id);
  print_inst(p, i1);
  fputs("\"];\n", p);

  if (is_nil_IRInstList(t)) {
    return i1->global_id;
  }

  IRInst* i2 = head_IRInstList(t);
  fprintf(p, "inst_%d -> inst_%d;\n", i1->global_id, i2->global_id);
  return print_graph_insts(p, t);
}

static void print_graph_bb(FILE* p, BasicBlock* bb);

static void print_graph_succs(FILE* p, unsigned id, BBRefListIterator* it) {
  if (is_nil_BBRefListIterator(it)) {
    return;
  }
  BasicBlock* head = data_BBRefListIterator(it);
  if (is_nil_IRInstList(head->insts)) {
    error("unexpected empty basic block %d", head->global_id);
  }

  fprintf(p, "inst_%d->inst_%d;\n", id, head_IRInstList(head->insts)->global_id);
  print_graph_succs(p, id, next_BBRefListIterator(it));
}

static void print_graph_bb(FILE* p, BasicBlock* bb) {
  fprintf(p, "subgraph cluster_%d {\n", bb->global_id);
  fprintf(p, "label = \"BasicBlock %d", bb->local_id);

  if (bb->live_gen != NULL) {
    fprintf(p, "\\ngen: ");
    print_BitSet(p, bb->live_gen);
  }
  if (bb->live_kill != NULL) {
    fprintf(p, " kill: ");
    print_BitSet(p, bb->live_kill);
  }
  if (bb->live_in != NULL) {
    fprintf(p, "\\nin: ");
    print_BitSet(p, bb->live_in);
  }
  if (bb->live_out != NULL) {
    fprintf(p, " out: ");
    print_BitSet(p, bb->live_out);
  }
  if (bb->should_preserve != NULL) {
    fprintf(p, "\\npreserve: ");
    print_BitSet(p, bb->should_preserve);
  }

  fprintf(p, "\";\n");

  if (is_nil_IRInstList(bb->insts)) {
    error("unexpected empty basic block %d", bb->global_id);
  }
  unsigned last_id = print_graph_insts(p, bb->insts);

  fputs("}\n", p);
  print_graph_succs(p, last_id, front_BBRefList(bb->succs));
}

static void print_graph_blocks(FILE* p, BBListIterator* it) {
  if (is_nil_BBListIterator(it)) {
    return;
  }
  print_graph_bb(p, data_BBListIterator(it));
  print_graph_blocks(p, next_BBListIterator(it));
}

static void print_Function(FILE* p, Function* f) {
  fprintf(p, "subgraph cluster_%s {\n", f->name);
  fprintf(p, "label = %s;\n", f->name);
  print_graph_blocks(p, front_BBList(f->blocks));
  fprintf(p, "}\n");
}

DECLARE_LIST_PRINTER(FunctionList)
DEFINE_LIST_PRINTER(print_Function, "\n", "\n", FunctionList)

void print_IR(FILE* p, IR* ir) {
  fprintf(p, "digraph CFG {\n");
  print_FunctionList(p, ir->functions);
  fprintf(p, "}\n");
  // TODO: print `ir->globals`
}

static void release_Interval(Interval* iv) {
  free(iv);
}
DEFINE_VECTOR(release_Interval, Interval*, RegIntervals)

static void print_Interval(FILE* p, Interval* iv) {
  fprintf(p, "[%d, %d]", iv->from, iv->to);
}

void print_Intervals(FILE* p, RegIntervals* v) {
  for (unsigned i = 0; i < length_RegIntervals(v); i++) {
    Interval* iv = get_RegIntervals(v, i);
    fprintf(p, "%d: ", i);
    print_Interval(p, iv);
    fputs("\n", p);
  }
}
