#include "ir.h"
#include "parser.h"
#include "error.h"

DEFINE_LIST(IRInst, IR)

typedef struct {
  int reg_count;
  IR* ir;
  IR* ir_cursor;
} Env;

Env* new_env() {
  Env* env = calloc(1, sizeof(Env));
  env->reg_count = 0;
  env->ir = nil_IR();
  env->ir_cursor = env->ir;
  return env;
}

// release `Env`, but not containing `ir`
// extract `ir` and return it
IR* release_env(Env* env) {
  IR* out = env->ir;
  free(env);
  return out;
}

Reg new_reg(Env* env) {
  int i = env->reg_count++;
  Reg r = { .virtual = i, .real = -1 };
  return r;
}

// TODO: Consider eliminating copies of `IRInst`

void add_inst(Env *env, IRInst inst) {
  env->ir_cursor = snoc_IR(inst, env->ir_cursor);
}

Reg new_binop(Env *env, BinopKind op, Reg lhs, Reg rhs) {
  IRInst i = {
    .kind = IR_BIN,
    .binop = op,
    .rd = lhs,
    .ra = rhs,
  };
  add_inst(env, i);
  return lhs;
}

Reg new_imm(Env* env, int num) {
  Reg r = new_reg(env);
  IRInst i = {
    .kind = IR_IMM,
    .imm = num,
    .rd = r,
  };
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

IR* generate_ir(Node* node) {
  Env* env = new_env();
  Reg r = gen_ir(env, node);
  IRInst ret = { .kind = IR_RET, .ra = r };
  add_inst(env, ret);
  return release_env(env);
}

void print_reg(FILE* p, Reg r) {
  if (r.virtual == -1) {
    fprintf(p, "r%d", r.real);
  } else {
    fprintf(p, "v%d", r.virtual);
  }
}

void print_inst(FILE* p, IRInst i) {
  switch(i.kind) {
    case IR_IMM:
      fprintf(p, "IMM %d", i.imm);
      break;
    case IR_RET:
      fprintf(p, "RET ");
      print_reg(p, i.ra);
      break;
    case IR_BIN:
      fprintf(p, "BIN ");
      print_reg(p, i.rd);
      fprintf(p, " ");
      print_binop(p, i.binop);
      fprintf(p, " ");
      print_reg(p, i.ra);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

DEFINE_LIST_PRINTER(print_inst, "\n", "\n", IR)
