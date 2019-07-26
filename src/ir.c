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

static void release_reg(Reg r) {}
DEFINE_VECTOR(release_reg, Reg, RegVec)

static void release_BasicBlock(BasicBlock* bb) {
  if (bb == NULL || bb->released) {
    return;
  }

  bb->released = true;  // prevent from double-free (graph can be cyclic)

  release_IRInstList(bb->insts);
  release_BBList(bb->succs);
  release_BBList(bb->preds);

  free(bb);
}

DEFINE_LIST(release_BasicBlock, BasicBlock*, BBList)

typedef struct {
  unsigned reg_count;
  unsigned stack_count;
  unsigned bb_count;
  unsigned inst_count;

  UIMap* vars;

  BasicBlock* entry;

  BasicBlock* cur;
  IRInstList* inst_cur;
} Env;

static Env* new_env() {
  Env* env         = calloc(1, sizeof(Env));
  env->reg_count   = 0;
  env->stack_count = 0;
  env->bb_count    = 0;
  env->inst_count  = 0;
  env->vars        = new_UIMap(32);

  return env;
}

static IRInst* new_inst_(Env* env, IRInstKind kind) {
  return new_inst(env->inst_count++, kind);
}

static BasicBlock* new_bb(Env* env) {
  unsigned i     = env->bb_count++;
  BasicBlock* bb = calloc(1, sizeof(BasicBlock));
  bb->id         = i;
  bb->insts      = nil_IRInstList();
  bb->succs      = nil_BBList();
  bb->preds      = nil_BBList();
  return bb;
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

static Reg new_ret(Env* env, Reg r) {
  IRInst* i = new_inst_(env, IR_RET);
  push_RegVec(i->ras, r);
  add_inst(env, i);
  return r;
}

static void new_br(Env* env, Reg r, BasicBlock* then_, BasicBlock* else_) {
  IRInst* i = new_inst_(env, IR_BR);
  push_RegVec(i->ras, r);
  i->then_ = then_;
  i->else_ = else_;
  add_inst(env, i);
}

static void new_jump(Env* env, BasicBlock* jump) {
  IRInst* i = new_inst_(env, IR_JUMP);
  i->jump   = jump;
  add_inst(env, i);
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

static void gen_stmt(Env* env, Statement* stmt) {
  switch (stmt->kind) {
    case ST_EXPRESSION:
      gen_expr(env, stmt->expr);
      break;
    case ST_RETURN: {
      Reg r = gen_expr(env, stmt->expr);
      new_ret(env, r);
      break;
    }
    case ST_IF: {
      BasicBlock* cur = env->cur;

      BasicBlock* then_bb = new_bb(env);
      BasicBlock* else_bb = new_bb(env);
      BasicBlock* next_bb = new_bb(env);

      Reg cond = gen_expr(env, stmt->expr);
      new_br(env, cond, then_bb, else_bb);

      // then
      start_bb(env, then_bb);
      gen_stmt(env, stmt->then_);
      new_jump(env, next_bb);

      // else
      start_bb(env, else_bb);
      gen_stmt(env, stmt->else_);
      new_jump(env, next_bb);

      connect_bb(cur, then_bb);
      connect_bb(cur, else_bb);
      connect_bb(then_bb, next_bb);
      connect_bb(else_bb, next_bb);

      start_bb(env, next_bb);
      break;
    }
    default:
      CCC_UNREACHABLE;
  }
}

static void gen_decl(Env* env, Declaration* decl) {
  new_var(env, decl->declarator);
}

static void gen_ir(Env* env, AST* ast) {
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

  gen_ir(env, tail_BlockItemList(ast));
}

IR* generate_IR(AST* ast) {
  Env* env = new_env();

  BasicBlock* entry    = new_bb(env);
  start_bb(env, entry);
  gen_ir(env, ast);

  BasicBlock* exit     = env->cur;
  unsigned reg_count   = env->reg_count;
  unsigned stack_count = env->stack_count;
  free(env);

  IR* ir          = calloc(1, sizeof(IR));
  ir->entry       = entry;
  ir->exit        = exit;
  ir->reg_count   = reg_count;
  ir->stack_count = stack_count;
  return ir;
}

void release_IR(IR* ir) {
  release_BasicBlock(ir->entry);
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
      fprintf(p, "BR (%d | %d) ", i->then_->id, i->else_->id);
      break;
    case IR_JUMP:
      fprintf(p, "JUMP %d", i->jump->id);
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

  fprintf(p, "inst_%d [label=\"", i1->id);
  print_inst(p, i1);
  fputs("\"];\n", p);

  if (is_nil_IRInstList(t)) {
    return i1->id;
  }

  IRInst* i2 = head_IRInstList(t);
  fprintf(p, "inst_%d -> inst_%d;\n", i1->id, i2->id);
  return print_graph_insts(p, t);
}

static void print_graph_succs(FILE* p, unsigned id, BBList* l) {
  if (l->is_nil) {
    return;
  }
  IRInstList* insts = head_BBList(l)->insts;
  fprintf(p, "inst_%d->inst_%d", id, head_IRInstList(insts)->id);
  print_graph_succs(p, id, l->tail);
}

static void print_graph_bb(FILE* p, BasicBlock* bb) {
  fprintf(p, "subgraph bb_%d {\n", bb->id);
  unsigned last_id = print_graph_insts(p, bb->insts);
  fputs("}\n", p);
  print_graph_succs(p, last_id, bb->succs);
}

void print_IR(FILE* p, IR* ir) {
  print_graph_bb(p, ir->entry);
}
