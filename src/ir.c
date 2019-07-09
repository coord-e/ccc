#include "ir.h"
#include "parser.h"
#include "error.h"

DEFINE_LIST(IRInst, IRInstList)

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
  // zero denotes unused here (=zero-initialized)
  // TODO: Use better representation of unused
  Reg r = { .virtual = (i + 1), .real = 0 };
  return r;
}

// TODO: Consider eliminating copies of `IRInst`

void add_inst(Env *env, IRInst inst) {
  env->cursor = snoc_IRInstList(inst, env->cursor);
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

IR* generate_IR(Node* node) {
  Env* env = new_env();

  Reg r = gen_ir(env, node);
  IRInst ret = { .kind = IR_RET, .ra = r };
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
  if (r.virtual == -1) {
    fprintf(p, "r%d", r.real);
  } else {
    fprintf(p, "v%d", r.virtual);
  }
}

void print_inst(FILE* p, IRInst i) {
  switch(i.kind) {
    case IR_IMM:
      fprintf(p, "IMM ");
      print_reg(p, i.rd);
      fprintf(p, " <- %d", i.imm);
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

DEFINE_LIST_PRINTER(print_inst, "\n", "\n", IRInstList)

void print_IR(FILE* p, IR* ir) { print_IRInstList(p, ir->insts); };
