#include "ir.h"
#include "error.h"
#include "liveness.h"
#include "map.h"
#include "parser.h"

DataSize to_data_size(unsigned i) {
  switch (i) {
    case 1:
      return SIZE_BYTE;
    case 2:
      return SIZE_WORD;
    case 4:
      return SIZE_DWORD;
    case 8:
      return SIZE_QWORD;
    default:
      error("invalid data size %d", i);
  }
}

// Unsigned Integer Map
DECLARE_MAP(unsigned, UIMap)
static void release_unsigned(unsigned i) {}
DEFINE_MAP(release_unsigned, unsigned, UIMap)

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

DEFINE_LIST(release_BasicBlock, BasicBlock*, BBList)
DEFINE_VECTOR(release_BasicBlock, BasicBlock*, BBVec)

static void release_Function(Function* f) {
  free(f->name);
  release_BBList(f->blocks);
  release_RegIntervals(f->intervals);
  free(f);
}

DEFINE_LIST(release_Function, Function*, FunctionList)

typedef struct {
  unsigned bb_count;
  unsigned inst_count;
} GlobalEnv;

static GlobalEnv* init_GlobalEnv() {
  GlobalEnv* env  = calloc(1, sizeof(GlobalEnv));
  env->bb_count   = 0;
  env->inst_count = 0;
  return env;
}

static void release_GlobalEnv(GlobalEnv* env) {
  free(env);
}

typedef struct {
  unsigned reg_count;
  unsigned stack_count;
  unsigned bb_count;
  unsigned inst_count;

  UIMap* vars;
  BBList* blocks;

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

static Env* new_env(GlobalEnv* genv) {
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

static Reg nth_arg(Env* env, unsigned nth, DataSize size) {
  Reg r           = new_reg(env, size);
  IRInst* i       = new_inst_(env, IR_ARG);
  i->argument_idx = nth;
  i->rd           = r;
  add_inst(env, i);
  return r;
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

static Reg gen_expr(Env* env, Expr* node);

static Reg gen_lhs(Env* env, Expr* node) {
  switch (node->kind) {
    case ND_VAR: {
      unsigned i;
      if (get_var(env, node->var, &i)) {
        return new_stack_addr(env, i);
      } else {
        error("undeclared name \"%s\"", node->var);
      }
    }
    case ND_UNAOP:
      if (node->unaop == UNAOP_DEREF) {
        return gen_expr(env, node->expr);
      }
      // fallthrough
    default:
      error("invaild lhs");
  }
}

static DataSize datasize_of_node(Expr* e) {
  return to_data_size(stored_size_ty(e->type));
}

static Reg gen_unaop(Env* env, UnaopKind op, Expr* opr) {
  switch (op) {
    case UNAOP_ADDR:
      return gen_lhs(env, opr->expr);
    case UNAOP_DEREF: {
      Reg r = gen_expr(env, opr->expr);
      return new_load(env, r, datasize_of_node(opr));
    }
    default:
      CCC_UNREACHABLE;
  }
}

Reg gen_expr(Env* env, Expr* node) {
  switch (node->kind) {
    case ND_CAST:
      // TODO: trunc and extend
      assert(stored_size_ty(node->cast_to) == stored_size_ty(node->expr->type));
      return gen_expr(env, node->expr);
    case ND_NUM:
      return new_imm(env, node->num, datasize_of_node(node));
    case ND_BINOP: {
      Reg lhs = gen_expr(env, node->lhs);
      Reg rhs = gen_expr(env, node->rhs);
      return new_binop(env, node->binop, lhs, rhs);
    }
    case ND_UNAOP:
      return gen_unaop(env, node->unaop, node);
    case ND_ASSIGN: {
      Reg addr = gen_lhs(env, node->lhs);
      Reg rhs  = gen_expr(env, node->rhs);
      assert(stored_size_ty(node->lhs->type) == stored_size_ty(node->rhs->type));
      new_store(env, addr, rhs, datasize_of_node(node));
      return rhs;
    }
    case ND_VAR: {
      unsigned i;
      if (get_var(env, node->var, &i)) {
        return new_stack_load(env, i, datasize_of_node(node));
      } else {
        return new_global(env, node->var);
      }
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
    case ST_NULL:
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static void gen_decl(Env* env, Declaration* decl) {
  new_var(env, decl->declarator->name, stored_size_ty(decl->type));
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

  char* name    = head_ParamList(l)->name;
  Type* ty      = get_TypeVec(f->type->params, nth);
  unsigned size = stored_size_ty(ty);
  new_var(env, name, size);

  unsigned addr;
  get_var(env, name, &addr);

  Reg rhs = nth_arg(env, nth, to_data_size(size));
  new_stack_store(env, addr, rhs, to_data_size(size));

  gen_params(env, f, nth + 1, tail_ParamList(l));
}

static Function* gen_function(GlobalEnv* genv, FunctionDef* ast) {
  Env* env = new_env(genv);

  BasicBlock* entry = new_bb(env);
  start_bb(env, entry);
  gen_params(env, ast, 0, ast->params);
  gen_block_item_list(env, ast->items);
  create_or_start_bb(env, env->exit);
  new_exit_ret(env);

  Function* ir      = calloc(1, sizeof(Function));
  ir->name          = strdup(ast->decl->name);
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

  release_GlobalEnv(genv);

  return ir;
}

void release_IR(IR* ir) {
  release_FunctionList(ir->functions);
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
    case IR_CALL:
      fprintf(p, "CALL ");
      break;
    case IR_BIN:
      fprintf(p, "BIN ");
      print_binop(p, i->binop);
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
}
