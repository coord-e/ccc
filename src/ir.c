#include "ir.h"
#include "error.h"
#include "liveness.h"
#include "map.h"
#include "parser.h"

IRInst* new_inst(unsigned local, unsigned global, IRInstKind kind) {
  IRInst* i    = calloc(1, sizeof(IRInst));
  i->kind      = kind;
  i->local_id  = local;
  i->global_id = global;

  i->ras         = new_RegVec(1);
  i->global_name = NULL;
  return i;
}

void release_inst(IRInst* i) {
  assert(i->ras != NULL);
  release_RegVec(i->ras);
  free(i->global_name);
  free(i);
}

DEFINE_LIST(release_inst, IRInst*, IRInstList)
DEFINE_VECTOR(release_inst, IRInst*, IRInstVec)

static void release_reg(Reg r) {}
DEFINE_VECTOR(release_reg, Reg, RegVec)

static void release_BasicBlock(BasicBlock* bb) {
  if (bb == NULL) {
    return;
  }

  release_IRInstList(bb->insts);

  release_BitSet(bb->live_gen);
  release_BitSet(bb->live_kill);
  release_BitSet(bb->live_in);
  release_BitSet(bb->live_out);

  free(bb);
}

DECLARE_MAP(BasicBlock*, BBMap)
DEFINE_LIST(release_BasicBlock, BasicBlock*, BBList)
DEFINE_VECTOR(release_BasicBlock, BasicBlock*, BBVec)

static void release_Function(Function* f) {
  free(f->name);
  release_BBList(f->blocks);
  release_RegIntervals(f->intervals);
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
  GlobalEnv* env  = calloc(1, sizeof(GlobalEnv));
  env->bb_count   = 0;
  env->inst_count = 0;
  env->globals    = new_GlobalVarVec(16);
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
  bb->succs     = nil_BBList();
  bb->preds     = nil_BBList();
  bb->dead      = false;

  bb->live_gen     = NULL;
  bb->live_kill    = NULL;
  bb->live_in      = NULL;
  bb->live_out     = NULL;
  bb->sorted_insts = NULL;

  env->blocks = cons_BBList(bb, env->blocks);

  return bb;
}

static Env* new_env(GlobalEnv* genv, FunctionDef* f) {
  Env* env         = calloc(1, sizeof(Env));
  env->global_env  = genv;
  env->reg_count   = 0;
  env->stack_count = 0;
  env->bb_count    = 0;
  env->inst_count  = 0;
  env->vars        = new_UIMap(32);
  env->blocks      = nil_BBList();

  env->exit = new_bb(env);

  env->loop_break    = NULL;
  env->loop_continue = NULL;

  env->named_labels = f->named_labels;
  env->labels       = new_BBVec(f->label_count);
  for (unsigned i = 0; i < f->label_count; i++) {
    push_BBVec(env->labels, new_bb(env));
  }

  return env;
}

static void connect_bb(BasicBlock* from, BasicBlock* to) {
  from->succs = cons_BBList(to, from->succs);
  to->preds   = cons_BBList(from, to->preds);
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

static Reg new_reg(Env* env, DataSize size) {
  unsigned i = env->reg_count++;
  Reg r      = {.kind = REG_VIRT, .virtual = i, .real = 0, .size = size, .is_used = true};
  return r;
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

static Reg new_binop(Env* env, BinopKind op, Reg lhs, Reg rhs) {
  assert(lhs.size == rhs.size);

  Reg dest = new_reg(env, lhs.size);

  // TODO: emit this move in `arch` pass
  IRInst* i1 = new_inst_(env, IR_MOV);
  i1->rd     = dest;
  push_RegVec(i1->ras, lhs);
  add_inst(env, i1);

  IRInst* i2 = new_inst_(env, IR_BIN);
  i2->binop  = op;
  i2->rd     = dest;
  push_RegVec(i2->ras, dest);
  push_RegVec(i2->ras, rhs);
  add_inst(env, i2);

  return dest;
}

static Reg new_unaop(Env* env, UnaopKind op, Reg opr) {
  Reg dest = new_reg(env, opr.size);

  // TODO: emit this move in `arch` pass
  IRInst* i1 = new_inst_(env, IR_MOV);
  i1->rd     = dest;
  push_RegVec(i1->ras, opr);
  add_inst(env, i1);

  IRInst* i2 = new_inst_(env, IR_UNA);
  i2->unaop  = op;
  i2->rd     = dest;
  push_RegVec(i2->ras, dest);
  add_inst(env, i2);

  return dest;
}

static Reg new_imm(Env* env, int num, DataSize size) {
  Reg r     = new_reg(env, size);
  IRInst* i = new_inst_(env, IR_IMM);
  i->imm    = num;
  i->rd     = r;
  add_inst(env, i);
  return r;
}

static Reg new_stack_load(Env* env, unsigned s, DataSize size) {
  Reg r        = new_reg(env, size);
  IRInst* i    = new_inst_(env, IR_STACK_LOAD);
  i->stack_idx = s;
  i->rd        = r;
  i->data_size = size;
  add_inst(env, i);
  return r;
}

static Reg new_stack_store(Env* env, unsigned s, Reg r, DataSize size) {
  assert(r.size == size);

  IRInst* i    = new_inst_(env, IR_STACK_STORE);
  i->stack_idx = s;
  push_RegVec(i->ras, r);
  i->data_size = size;
  add_inst(env, i);
  return r;
}

static Reg new_stack_addr(Env* env, unsigned s) {
  Reg r        = new_reg(env, SIZE_QWORD);  // TODO: hardcoded pointer size
  IRInst* i    = new_inst_(env, IR_STACK_ADDR);
  i->stack_idx = s;
  i->rd        = r;
  add_inst(env, i);
  return r;
}

static Reg new_load(Env* env, Reg s, DataSize size) {
  Reg r     = new_reg(env, size);
  IRInst* i = new_inst_(env, IR_LOAD);
  push_RegVec(i->ras, s);
  i->rd        = r;
  i->data_size = size;
  add_inst(env, i);
  return r;
}

static void new_store(Env* env, Reg s, Reg r, DataSize size) {
  assert(r.size == size);

  IRInst* i = new_inst_(env, IR_STORE);
  push_RegVec(i->ras, s);
  push_RegVec(i->ras, r);
  i->data_size = size;
  add_inst(env, i);
}

static Reg new_sext(Env* env, Reg t, DataSize to) {
  assert(t.size < to);

  Reg r     = new_reg(env, to);
  IRInst* i = new_inst_(env, IR_SEXT);
  push_RegVec(i->ras, t);
  i->rd        = r;
  i->data_size = to;
  add_inst(env, i);
  return r;
}

static Reg new_trunc(Env* env, Reg t, DataSize to) {
  assert(t.size > to);

  Reg r     = new_reg(env, to);
  IRInst* i = new_inst_(env, IR_TRUNC);
  push_RegVec(i->ras, t);
  i->rd        = r;
  i->data_size = to;
  add_inst(env, i);
  return r;
}

static Reg nth_arg(Env* env, unsigned nth, DataSize size) {
  Reg r           = new_reg(env, size);
  IRInst* i       = new_inst_(env, IR_ARG);
  i->argument_idx = nth;
  i->rd           = r;
  add_inst(env, i);
  return r;
}

static void new_move(Env* env, Reg d, Reg s) {
  IRInst* i = new_inst_(env, IR_MOV);
  push_RegVec(i->ras, s);
  i->rd = d;
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

static Reg new_ret(Env* env, Reg r, BasicBlock* next) {
  IRInst* i = new_inst_(env, IR_RET);
  push_RegVec(i->ras, r);
  add_inst(env, i);

  connect_bb(env->cur, env->exit);
  create_or_start_bb(env, next);

  return r;
}

static void new_br(Env* env, Reg r, BasicBlock* then_, BasicBlock* else_, BasicBlock* next) {
  IRInst* i = new_inst_(env, IR_BR);
  push_RegVec(i->ras, r);
  i->then_ = then_;
  i->else_ = else_;
  add_inst(env, i);

  connect_bb(env->cur, then_);
  connect_bb(env->cur, else_);

  create_or_start_bb(env, next);
}

static Reg new_global(Env* env, const char* name) {
  Reg r             = new_reg(env, SIZE_QWORD);  // TODO: hardcoded pointer size
  IRInst* inst      = new_inst_(env, IR_GLOBAL_ADDR);
  inst->rd          = r;
  inst->global_name = strdup(name);
  add_inst(env, inst);
  return r;
}

static Reg new_string(Env* env, const char* str) {
  unsigned i = length_GlobalVarVec(env->global_env->globals);
  // TODO: allocate accurate length of string
  char name[10];
  sprintf(name, "_s_%d", i);
  add_string_gvar(env->global_env, name, str);
  return new_global(env, name);
}

static Reg gen_expr(Env* env, Expr* node);

static Reg gen_lhs(Env* env, Expr* node) {
  switch (node->kind) {
    case ND_VAR: {
      unsigned i;
      if (get_var(env, node->var, &i)) {
        return new_stack_addr(env, i);
      } else {
        return new_global(env, node->var);
      }
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

Reg gen_expr(Env* env, Expr* node) {
  switch (node->kind) {
    case ND_CAST: {
      // TODO: signedness?
      unsigned cast_size = sizeof_ty(node->cast_type);
      unsigned expr_size = sizeof_ty(node->expr->type);
      Reg r              = gen_expr(env, node->expr);
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
      Reg lhs = gen_expr(env, node->lhs);
      Reg rhs = gen_expr(env, node->rhs);
      return new_binop(env, node->binop, lhs, rhs);
    }
    case ND_UNAOP: {
      Reg r = gen_expr(env, node->expr);
      return new_unaop(env, node->unaop, r);
    }
    case ND_ADDR:
    case ND_ADDR_ARY:
      return gen_lhs(env, node->expr);
    case ND_DEREF: {
      Reg r = gen_expr(env, node->expr);
      return new_load(env, r, datasize_of_node(node));
    }
    case ND_COMPOUND_ASSIGN: {
      Reg addr = gen_lhs(env, node->lhs);
      Reg rhs  = gen_expr(env, node->rhs);
      Reg lhs  = new_load(env, addr, datasize_of_node(node->lhs));
      Reg val  = new_binop(env, node->binop, lhs, rhs);
      assert(sizeof_ty(node->lhs->type) == val.size);
      new_store(env, addr, val, datasize_of_node(node));
      return val;
    }
    case ND_ASSIGN: {
      Reg addr = gen_lhs(env, node->lhs);
      Reg rhs  = gen_expr(env, node->rhs);
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
    case ND_VAR: {
      // lvalue conversion is performed here
      if (is_array_ty(node->type)) {
        error("attempt to perform lvalue conversion on array value");
      }
      Reg r = gen_lhs(env, node);
      return new_load(env, r, datasize_of_node(node));
    }
    case ND_CALL: {
      IRInst* inst = new_inst_(env, IR_CALL);
      Reg f        = gen_expr(env, node->lhs);
      push_RegVec(inst->ras, f);

      for (unsigned i = 0; i < length_ExprVec(node->args); i++) {
        Expr* e = get_ExprVec(node->args, i);
        push_RegVec(inst->ras, gen_expr(env, e));
      }

      Reg r    = new_reg(env, datasize_of_node(node));
      inst->rd = r;

      add_inst(env, inst);
      return r;
    }
    case ND_COND: {
      Reg r = new_reg(env, datasize_of_node(node));

      BasicBlock* then_bb = new_bb(env);
      BasicBlock* else_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      Reg cond = gen_expr(env, node->cond);
      new_br(env, cond, then_bb, else_bb, then_bb);

      // then
      Reg then_ = gen_expr(env, node->then_);
      new_move(env, r, then_);
      new_jump(env, next_bb, else_bb);

      // else
      Reg else_ = gen_expr(env, node->else_);
      new_move(env, r, else_);
      new_jump(env, next_bb, next_bb);

      return r;
    }
    case ND_SIZEOF_TYPE:
    case ND_SIZEOF_EXPR:
    // `sizeof` must be processed `sema`
    default:
      CCC_UNREACHABLE;
  }
}

void set_loop(Env* env, BasicBlock* next, BasicBlock* cont) {
  env->loop_break    = next;
  env->loop_continue = cont;
}

void reset_loop(Env* env) {
  env->loop_break    = NULL;
  env->loop_continue = NULL;
}

void gen_block_item_list(Env* env, BlockItemList* ast);

static void gen_stmt(Env* env, Statement* stmt) {
  switch (stmt->kind) {
    case ST_EXPRESSION:
      gen_expr(env, stmt->expr);
      break;
    case ST_RETURN: {
      if (stmt->expr == NULL) {
        new_void_ret(env, NULL);
      } else {
        Reg r = gen_expr(env, stmt->expr);
        new_ret(env, r, NULL);
      }
      break;
    }
    case ST_IF: {
      BasicBlock* then_bb = new_bb(env);
      BasicBlock* else_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      Reg cond = gen_expr(env, stmt->expr);
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

      set_loop(env, next_bb, while_bb);

      create_or_start_bb(env, while_bb);
      Reg cond = gen_expr(env, stmt->expr);
      new_br(env, cond, body_bb, next_bb, body_bb);

      // body
      gen_stmt(env, stmt->body);
      new_jump(env, while_bb, next_bb);

      reset_loop(env);

      break;
    }
    case ST_DO: {
      BasicBlock* body_bb = new_bb(env);
      BasicBlock* cont_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      set_loop(env, next_bb, cont_bb);

      create_or_start_bb(env, body_bb);
      gen_stmt(env, stmt->body);

      create_or_start_bb(env, cont_bb);
      Reg cond = gen_expr(env, stmt->expr);
      new_br(env, cond, body_bb, next_bb, next_bb);

      reset_loop(env);

      break;
    }
    case ST_FOR: {
      BasicBlock* for_bb  = new_bb(env);
      BasicBlock* body_bb = new_bb(env);
      BasicBlock* cont_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      set_loop(env, next_bb, cont_bb);

      if (stmt->init != NULL) {
        gen_expr(env, stmt->init);
      }
      create_or_start_bb(env, for_bb);
      Reg cond = gen_expr(env, stmt->before);
      new_br(env, cond, body_bb, next_bb, body_bb);

      // body
      gen_stmt(env, stmt->body);
      create_or_start_bb(env, cont_bb);
      if (stmt->after != NULL) {
        gen_expr(env, stmt->after);
      }
      new_jump(env, for_bb, next_bb);

      reset_loop(env);

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
      UIMap* save = env->vars;
      UIMap* inst = copy_UIMap(env->vars);

      env->vars = inst;
      gen_block_item_list(env, stmt->items);
      env->vars = save;

      release_UIMap(inst);
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
      Reg r = gen_expr(env, stmt->expr);

      BasicBlock* next_bb = new_bb(env);

      BasicBlock* old_break = env->loop_break;
      env->loop_break       = next_bb;

      for (unsigned i = 0; i < length_StmtVec(stmt->cases); i++) {
        Statement* case_    = get_StmtVec(stmt->cases, i);
        Reg cond            = new_binop(env, BINOP_EQ, r, new_imm(env, case_->case_value, r.size));
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
      env->loop_break = old_break;

      break;
    }
    case ST_NULL:
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static GlobalExpr* translate_initializer_expr(Expr* expr) {
  switch (expr->kind) {
    case ND_NUM: {
      GlobalExpr* e = new_GlobalExpr(GE_NUM);
      e->num        = expr->num;
      e->size       = to_data_size(sizeof_ty(expr->type));
      return e;
    }
    case ND_STRING: {
      GlobalExpr* e = new_GlobalExpr(GE_STRING);
      e->string     = strdup(expr->string);
      return e;
    }
    case ND_CAST: {
      // TODO: strictly consider types in constant evaluation
      GlobalExpr* e = translate_initializer_expr(expr->expr);
      e->size       = to_data_size(sizeof_ty(expr->cast_type));
      return e;
    }
    default:
      CCC_UNREACHABLE;
  }
}

static GlobalInitializer* translate_initializer(Initializer* init) {
  switch (init->kind) {
    case IN_EXPR: {
      GlobalExpr* e = translate_initializer_expr(init->expr);
      return single_GlobalInitializer(e);
    }
    case IN_LIST: {
      GlobalInitializer* new_init = nil_GlobalInitializer();
      GlobalInitializer* gen_cur  = new_init;
      InitializerList* read_cur   = init->list;
      while (!is_nil_InitializerList(read_cur)) {
        Initializer* e = head_InitializerList(read_cur);

        GlobalInitializer* elem = translate_initializer(e);
        gen_cur                 = append_GlobalInitializer(gen_cur, elem);

        read_cur = tail_InitializerList(read_cur);
      }
      return new_init;
    }
    default:
      CCC_UNREACHABLE;
  }
}

static void gen_local_scalar_initializer(Env* env, Reg target, Initializer* init, Type* type) {
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
  Reg r = gen_expr(env, expr);
  new_store(env, target, r, datasize_of_node(expr));
}

static void gen_local_initializer(Env* env, Reg target, Initializer* init, Type* type);

static void gen_local_array_initializer(Env* env, Reg target, Initializer* init, Type* type) {
  if (init->kind != IN_LIST) {
    error("array initializer must be an initializer list");
  }

  InitializerList* cur = init->list;
  unsigned offset      = 0;
  // NOTE: `sema` ensured that the initializer list has the same length than the array
  while (!is_nil_InitializerList(cur)) {
    Initializer* ci = head_InitializerList(cur);
    Reg r           = new_binop(env, BINOP_ADD, target, new_imm(env, offset, target.size));
    gen_local_initializer(env, r, ci, type->element);
    cur = tail_InitializerList(cur);
    offset += sizeof_ty(type->element);
  }
}

static void gen_local_initializer(Env* env, Reg target, Initializer* init, Type* type) {
  if (is_scalar_ty(type)) {
    gen_local_scalar_initializer(env, target, init, type);
  } else if (is_array_ty(type)) {
    gen_local_array_initializer(env, target, init, type);
  } else {
    CCC_UNREACHABLE;
  }
}

static void gen_init_declarator(Env* env, GlobalEnv* genv, InitDeclarator* decl) {
  if (env != NULL) {
    unsigned var = new_var(env, decl->declarator->name_ref, sizeof_ty(decl->type));
    Reg r        = new_stack_addr(env, var);

    if (decl->initializer != NULL) {
      gen_local_initializer(env, r, decl->initializer, decl->type);
    }
  } else {
    assert(genv != NULL);

    // TODO: check `extern` definitions
    // NOTE: `sema` ensured that all global declaration except `extern` has an initializer
    assert(decl->initializer != NULL);

    GlobalInitializer* gi = translate_initializer(decl->initializer);
    add_normal_gvar(genv, decl->declarator->name_ref, gi);
  }
}

static void gen_init_decl_list(Env* env, GlobalEnv* genv, InitDeclaratorList* l) {
  if (is_nil_InitDeclaratorList(l)) {
    return;
  }
  gen_init_declarator(env, genv, head_InitDeclaratorList(l));
  gen_init_decl_list(env, genv, tail_InitDeclaratorList(l));
}

static void gen_decl(Env* env, Declaration* decl) {
  gen_init_decl_list(env, NULL, decl->declarators);
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

  char* name    = head_ParamList(l)->decl->name_ref;
  Type* ty      = get_TypeVec(f->type->params, nth);
  unsigned size = sizeof_ty(ty);
  new_var(env, name, size);

  unsigned addr;
  get_var(env, name, &addr);

  Reg rhs = nth_arg(env, nth, to_data_size(size));
  new_stack_store(env, addr, rhs, to_data_size(size));

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

  Function* ir      = calloc(1, sizeof(Function));
  ir->name          = strdup(ast->decl->name_ref);
  ir->entry         = entry;
  ir->exit          = env->cur;
  ir->bb_count      = env->bb_count;
  ir->reg_count     = env->reg_count;
  ir->stack_count   = env->stack_count;
  ir->inst_count    = env->inst_count;
  ir->blocks        = env->blocks;
  ir->sorted_blocks = NULL;
  ir->intervals     = NULL;
  ir->used_regs     = NULL;

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
      gen_init_decl_list(NULL, genv, d->decl->declarators);
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

static void print_reg(FILE* p, Reg r) {
  switch (r.kind) {
    case REG_VIRT:
      fprintf(p, "v%d", r.virtual);
      break;
    case REG_REAL:
      fprintf(p, "r%d", r.real);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

DEFINE_VECTOR_PRINTER(print_reg, ", ", "", RegVec)

static void print_inst(FILE* p, IRInst* i) {
  if (i->rd.is_used) {
    print_reg(p, i->rd);
    fprintf(p, " = ");
  }
  switch (i->kind) {
    case IR_ARG:
      fprintf(p, "ARG %d", i->argument_idx);
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
      print_binop(p, i->binop);
      fprintf(p, " ");
      break;
    case IR_UNA:
      fprintf(p, "UNA ");
      print_binop(p, i->unaop);
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

static void print_graph_succs(FILE* p, unsigned id, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }
  BasicBlock* head = head_BBList(l);
  if (is_nil_IRInstList(head->insts)) {
    error("unexpected empty basic block %d", head->global_id);
  }

  fprintf(p, "inst_%d->inst_%d;\n", id, head_IRInstList(head->insts)->global_id);
  print_graph_succs(p, id, l->tail);
}

static void print_graph_bb(FILE* p, BasicBlock* bb) {
  if (bb->dead) {
    return;
  }

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

  fprintf(p, "\";\n");

  if (is_nil_IRInstList(bb->insts)) {
    error("unexpected empty basic block %d", bb->global_id);
  }
  unsigned last_id = print_graph_insts(p, bb->insts);

  fputs("}\n", p);
  print_graph_succs(p, last_id, bb->succs);
}

static void print_graph_blocks(FILE* p, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }
  print_graph_bb(p, head_BBList(l));
  print_graph_blocks(p, tail_BBList(l));
}

static void print_Function(FILE* p, Function* f) {
  fprintf(p, "subgraph cluster_%s {\n", f->name);
  fprintf(p, "label = %s;\n", f->name);
  print_graph_blocks(p, f->blocks);
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
