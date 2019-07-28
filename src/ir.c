#include "ir.h"
#include "error.h"
#include "map.h"
#include "parser.h"

// Unsigned Integer Map
DECLARE_MAP(unsigned, UIMap)
static void release_unsigned(unsigned i) {}
DEFINE_MAP(release_unsigned, unsigned, UIMap)

IRInst* new_inst(unsigned id, IRInstKind kind) {
  IRInst* i = calloc(1, sizeof(IRInst));
  i->kind   = kind;
  i->id     = id;
  i->ras    = new_RegVec(1);
  return i;
}

void release_inst(IRInst* i) {
  assert(i->ras != NULL);
  release_RegVec(i->ras);
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
} Env;

static IRInst* new_inst_(Env* env, IRInstKind kind) {
  return new_inst(env->inst_count++, kind);
}

static BasicBlock* new_bb(Env* env) {
  unsigned i = env->bb_count++;

  IRInst* inst = new_inst_(env, IR_LABEL);

  BasicBlock* bb = calloc(1, sizeof(BasicBlock));
  inst->label    = bb;

  bb->id    = i;
  bb->insts = single_IRInstList(inst);
  bb->succs = nil_BBList();
  bb->preds = nil_BBList();
  bb->dead  = false;

  bb->live_gen     = NULL;
  bb->live_kill    = NULL;
  bb->live_in      = NULL;
  bb->live_out     = NULL;
  bb->sorted_insts = NULL;

  env->blocks = cons_BBList(bb, env->blocks);

  return bb;
}

static Env* new_env() {
  Env* env         = calloc(1, sizeof(Env));
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

static Reg new_reg(Env* env) {
  unsigned i = env->reg_count++;
  Reg r      = {.kind = REG_VIRT, .virtual = i, .real = 0, .is_used = true};
  return r;
}

static unsigned new_var(Env* env, char* name) {
  if (lookup_UIMap(env->vars, name, NULL)) {
    error("redeclaration of \"%s\"", name);
  }

  unsigned i = env->stack_count++;
  insert_UIMap(env->vars, name, i);
  return i;
}

static unsigned get_var(Env* env, char* name) {
  unsigned i;
  if (!lookup_UIMap(env->vars, name, &i)) {
    error("variable \"%s\" is not declared", name);
  }
  return i;
}

static void add_inst(Env* env, IRInst* inst) {
  env->inst_cur = snoc_IRInstList(inst, env->inst_cur);
}

static Reg new_binop(Env* env, BinopKind op, Reg lhs, Reg rhs) {
  Reg dest = new_reg(env);

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

static Reg new_imm(Env* env, int num) {
  Reg r     = new_reg(env);
  IRInst* i = new_inst_(env, IR_IMM);
  i->imm    = num;
  i->rd     = r;
  add_inst(env, i);
  return r;
}

static Reg new_load(Env* env, unsigned s) {
  Reg r        = new_reg(env);
  IRInst* i    = new_inst_(env, IR_LOAD);
  i->stack_idx = s;
  i->rd        = r;
  add_inst(env, i);
  return r;
}

static Reg new_store(Env* env, unsigned s, Reg r) {
  IRInst* i    = new_inst_(env, IR_STORE);
  i->stack_idx = s;
  push_RegVec(i->ras, r);
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
  Reg r     = new_imm(env, 0);
  IRInst* i = new_inst_(env, IR_RET);
  push_RegVec(i->ras, r);
  add_inst(env, i);
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

static unsigned gen_lhs(Env* env, Expr* node) {
  switch (node->kind) {
    case ND_VAR:
      return get_var(env, node->var);
    default:
      error("invaild lhs");
  }
}

static Reg gen_expr(Env* env, Expr* node) {
  switch (node->kind) {
    case ND_NUM:
      return new_imm(env, node->num);
    case ND_BINOP: {
      Reg lhs = gen_expr(env, node->lhs);
      Reg rhs = gen_expr(env, node->rhs);
      return new_binop(env, node->binop, lhs, rhs);
    }
    case ND_ASSIGN: {
      unsigned addr = gen_lhs(env, node->lhs);
      Reg rhs       = gen_expr(env, node->rhs);
      new_store(env, addr, rhs);
      return rhs;
    }
    case ND_VAR: {
      unsigned i = get_var(env, node->var);
      return new_load(env, i);
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
      Reg r = gen_expr(env, stmt->expr);
      new_ret(env, r, NULL);
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

      gen_expr(env, stmt->init);
      create_or_start_bb(env, for_bb);
      Reg cond = gen_expr(env, stmt->before);
      new_br(env, cond, body_bb, next_bb, body_bb);

      // body
      gen_stmt(env, stmt->body);
      create_or_start_bb(env, cont_bb);
      gen_expr(env, stmt->after);
      new_jump(env, for_bb, next_bb);

      reset_loop(env);

      break;
    }
    case ST_COMPOUND: {
      // compound statement is a block
      UIMap* save = copy_UIMap(env->vars);
      gen_block_item_list(env, stmt->items);
      release_UIMap(env->vars);
      env->vars = save;
      break;
    }
    default:
      CCC_UNREACHABLE;
  }
}

static void gen_decl(Env* env, Declaration* decl) {
  new_var(env, decl->declarator);
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

static void gen_ir(Env* env, AST* ast) {
  gen_block_item_list(env, ast);
}

IR* generate_IR(AST* ast) {
  Env* env = new_env();

  BasicBlock* entry = new_bb(env);
  start_bb(env, entry);
  gen_ir(env, ast);
  create_or_start_bb(env, env->exit);
  new_exit_ret(env);

  IR* ir            = calloc(1, sizeof(IR));
  ir->entry         = entry;
  ir->exit          = env->cur;
  ir->bb_count      = env->bb_count;
  ir->reg_count     = env->reg_count;
  ir->stack_count   = env->stack_count;
  ir->inst_count    = env->inst_count;
  ir->blocks        = env->blocks;
  ir->sorted_blocks = NULL;

  free(env);
  return ir;
}

void release_IR(IR* ir) {
  release_BBList(ir->blocks);
  free(ir);
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
    case IR_IMM:
      fprintf(p, "IMM %d", i->imm);
      break;
    case IR_RET:
      fprintf(p, "RET ");
      break;
    case IR_MOV:
      fprintf(p, "MOV ");
      break;
    case IR_BIN:
      fprintf(p, "BIN ");
      print_binop(p, i->binop);
      fprintf(p, " ");
      break;
    case IR_LOAD:
      fprintf(p, "LOAD %d", i->stack_idx);
      break;
    case IR_STORE:
      fprintf(p, "STORE %d ", i->stack_idx);
      break;
    case IR_SUBS:
      fprintf(p, "SUBS %d", i->stack_idx);
      break;
    case IR_BR:
      fprintf(p, "BR %d %d ", i->then_->id, i->else_->id);
      break;
    case IR_JUMP:
      fprintf(p, "JUMP %d", i->jump->id);
      break;
    case IR_LABEL:
      fprintf(p, "LABEL %d", i->label->id);
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

  fprintf(p, "inst_%d [shape=record,fontname=monospace,label=\"%d|", i1->id, i1->id);
  print_inst(p, i1);
  fputs("\"];\n", p);

  if (is_nil_IRInstList(t)) {
    return i1->id;
  }

  IRInst* i2 = head_IRInstList(t);
  fprintf(p, "inst_%d -> inst_%d;\n", i1->id, i2->id);
  return print_graph_insts(p, t);
}

static void print_graph_bb(FILE* p, BasicBlock* bb);

static void print_graph_succs(FILE* p, unsigned id, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }
  BasicBlock* head = head_BBList(l);
  if (is_nil_IRInstList(head->insts)) {
    error("unexpected empty basic block %d", head->id);
  }

  fprintf(p, "inst_%d->inst_%d;\n", id, head_IRInstList(head->insts)->id);
  print_graph_succs(p, id, l->tail);
}

static void print_graph_bb(FILE* p, BasicBlock* bb) {
  if (bb->dead) {
    return;
  }

  fprintf(p, "subgraph cluster_%d {\n", bb->id);
  fprintf(p, "label = \"BasicBlock %d", bb->id);

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
    error("unexpected empty basic block %d", bb->id);
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

void print_IR(FILE* p, IR* ir) {
  fprintf(p, "digraph CFG {\n");
  print_graph_blocks(p, ir->blocks);
  fprintf(p, "}\n");
}
