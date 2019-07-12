#include "ir.h"
#include "parser.h"
#include "error.h"

IRInst* new_inst(IRInstKind kind) {
  IRInst* i = calloc(1, sizeof(IRInst));
  i->kind = kind;
  return i;
}
void release_inst(IRInst* i) {
  if (i->ras != NULL) {
    release_RegVec(i->ras);
  }
  free(i);
}

DEFINE_LIST(release_inst, IRInst*, IRInstList)

static void release_reg(Reg r) {}
DEFINE_VECTOR(release_reg, Reg, RegVec)

typedef struct {
  unsigned reg_count;
  IRInstList* insts;
  IRInstList* cursor;
} Env;

Env* new_env() {
  Env* env = calloc(1, sizeof(Env));
  env->reg_count = 0;
  env->insts = nil_IRInstList();
  env->cursor = env->insts;
  return env;
}

Reg new_reg(Env* env) {
  unsigned i = env->reg_count++;
  Reg r = { .kind = REG_VIRT, .virtual = (i + 1), .real = 0, .is_used = true };
  return r;
}

void add_inst(Env *env, IRInst* inst) {
  env->cursor = snoc_IRInstList(inst, env->cursor);
}

RegVec* single_regvec(Reg r) {
  RegVec* v = new_RegVec(1);
  push_RegVec(v, r);
  return v;
}

Reg new_binop(Env *env, BinopKind op, Reg lhs, Reg rhs) {
  IRInst* i = new_inst(IR_BIN);
  Reg dest = new_reg(env);
  i->binop = op;
  i->rd = dest;
  i->ras = new_RegVec(2);
  push_RegVec(i->ras, lhs);
  push_RegVec(i->ras, rhs);
  add_inst(env, i);
  return dest;
}

Reg new_imm(Env* env, int num) {
  Reg r = new_reg(env);
  IRInst* i = new_inst(IR_IMM);
  i->imm = num;
  i->rd = r;
  add_inst(env, i);
  return r;
}

Reg gen_ir(Env* env, Node* node) {
  switch(node->kind) {
    case ND_NUM:
      return new_imm(env, node->num);
    case ND_BINOP:
    {
      Reg lhs = gen_ir(env, node->lhs);
      Reg rhs = gen_ir(env, node->rhs);
      return new_binop(env, node->binop, lhs, rhs);
    }
    default:
      CCC_UNREACHABLE;
  }
}

IR* generate_IR(Node* node) {
  Env* env = new_env();

  Reg r = gen_ir(env, node);
  IRInst* ret = new_inst(IR_RET);
  ret->ras = single_regvec(r);
  add_inst(env, ret);

  IRInstList* insts = env->insts;
  unsigned reg_count = env->reg_count;
  free(env);

  IR* ir = calloc(1, sizeof(IR));
  ir->insts = insts;
  ir->reg_count = reg_count;
  return ir;
}

void release_IR(IR* ir) {
  release_IRInstList(ir->insts);
  free(ir);
}

void print_reg(FILE* p, Reg r) {
  switch(r.kind) {
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

void print_inst(FILE* p, IRInst* i) {
  if (i->rd.is_used) {
    print_reg(p, i->rd);
    fprintf(p, " = ");
  }
  switch(i->kind) {
    case IR_IMM:
      fprintf(p, "IMM %d", i->imm);
      break;
    case IR_RET:
      fprintf(p, "RET ");
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
    default:
      CCC_UNREACHABLE;
  }
  if (i->ras != NULL) {
    print_RegVec(p, i->ras);
  }
}

DEFINE_LIST_PRINTER(print_inst, "\n", "\n", IRInstList)

void print_IR(FILE* p, IR* ir) { print_IRInstList(p, ir->insts); };
