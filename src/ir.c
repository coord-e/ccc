#include "ir.h"
#include "error.h"
#include "parser.h"

IRInst* new_inst(IRInstKind kind) {
  IRInst* i = calloc(1, sizeof(IRInst));
  i->kind   = kind;
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

typedef struct {
  unsigned reg_count;
  IRInstList* insts;
  IRInstList* cursor;
} Env;

static Env* new_env() {
  Env* env       = calloc(1, sizeof(Env));
  env->reg_count = 0;
  env->insts     = nil_IRInstList();
  env->cursor    = env->insts;
  return env;
}

static Reg new_reg(Env* env) {
  unsigned i = env->reg_count++;
  Reg r      = {.kind = REG_VIRT, .virtual = i, .real = 0, .is_used = true};
  return r;
}

static void add_inst(Env* env, IRInst* inst) {
  env->cursor = snoc_IRInstList(inst, env->cursor);
}

static Reg new_binop(Env* env, BinopKind op, Reg lhs, Reg rhs) {
  Reg dest = new_reg(env);

  IRInst* i1 = new_inst(IR_MOV);
  i1->rd     = dest;
  push_RegVec(i1->ras, lhs);
  add_inst(env, i1);

  IRInst* i2 = new_inst(IR_BIN);
  i2->binop  = op;
  i2->rd     = dest;
  push_RegVec(i2->ras, dest);
  push_RegVec(i2->ras, rhs);
  add_inst(env, i2);

  return dest;
}

static Reg new_imm(Env* env, int num) {
  Reg r     = new_reg(env);
  IRInst* i = new_inst(IR_IMM);
  i->imm    = num;
  i->rd     = r;
  add_inst(env, i);
  return r;
}

static Reg gen_ir(Env* env, Node* node) {
  switch (node->kind) {
    case ND_NUM:
      return new_imm(env, node->num);
    case ND_BINOP: {
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

  Reg r       = gen_ir(env, node);
  IRInst* ret = new_inst(IR_RET);
  push_RegVec(ret->ras, r);
  add_inst(env, ret);

  IRInstList* insts  = env->insts;
  unsigned reg_count = env->reg_count;
  free(env);

  IR* ir        = calloc(1, sizeof(IR));
  ir->insts     = insts;
  ir->reg_count = reg_count;
  return ir;
}

void release_IR(IR* ir) {
  release_IRInstList(ir->insts);
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
    default:
      CCC_UNREACHABLE;
  }
  if (i->ras != NULL) {
    print_RegVec(p, i->ras);
  }
}

DEFINE_LIST_PRINTER(print_inst, "\n", "\n", IRInstList)

void print_IR(FILE* p, IR* ir) {
  print_IRInstList(p, ir->insts);
}
