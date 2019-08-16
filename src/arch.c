#include "arch.h"

typedef struct {
  unsigned global_inst_count;
  unsigned inst_count;
} Env;

static Env* init_Env(unsigned global_inst_count, Function* f) {
  Env* env               = calloc(1, sizeof(Env));
  env->global_inst_count = global_inst_count;
  env->inst_count        = f->inst_count;
  return env;
}

static void finish_Env(Env* env, unsigned* inst_count, Function* f) {
  *inst_count   = env->global_inst_count;
  f->inst_count = env->inst_count;
  free(env);
}

static IRInst* new_move(Env* env, Reg rd, Reg ra) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_MOV);
  inst->rd     = rd;
  push_RegVec(inst->ras, ra);
  return inst;
}

static IRInst* new_binop(Env* env, BinopKind kind, Reg rd, Reg lhs, Reg rhs) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_BIN);
  inst->binop  = kind;
  inst->rd     = rd;
  push_RegVec(inst->ras, lhs);
  push_RegVec(inst->ras, rhs);
  return inst;
}

static IRInst* new_unaop(Env* env, UnaopKind kind, Reg rd, Reg opr) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_UNA);
  inst->unaop  = kind;
  inst->rd     = rd;
  push_RegVec(inst->ras, opr);
  return inst;
}

static void walk_insts(Env* env, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }

  IRInst* inst     = head_IRInstList(l);
  IRInstList* tail = tail_IRInstList(l);
  switch (inst->kind) {
    case IR_BIN: {
      Reg rd     = inst->rd;
      Reg lhs    = get_RegVec(inst->ras, 0);
      Reg rhs    = get_RegVec(inst->ras, 1);
      IRInst* i1 = new_move(env, rd, lhs);
      IRInst* i2 = new_binop(env, inst->binop, rd, rd, rhs);

      remove_IRInstList(l);
      insert_IRInstList(i2, l);
      insert_IRInstList(i1, l);
      break;
    }
    case IR_UNA: {
      Reg rd     = inst->rd;
      Reg opr    = get_RegVec(inst->ras, 0);
      IRInst* i1 = new_move(env, rd, opr);
      IRInst* i2 = new_unaop(env, inst->unaop, rd, rd);

      remove_IRInstList(l);
      insert_IRInstList(i2, l);
      insert_IRInstList(i1, l);
      break;
    }
    default:
      break;
  }

  walk_insts(env, tail);
}

static void walk_blocks(Env* env, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }

  BasicBlock* b = head_BBList(l);
  walk_insts(env, b->insts);

  walk_blocks(env, tail_BBList(l));
}

static void transform_function(unsigned* inst_count, Function* f) {
  Env* env = init_Env(*inst_count, f);

  walk_blocks(env, f->blocks);

  finish_Env(env, inst_count, f);
}

static void walk_functions(unsigned* inst_count, FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }
  transform_function(inst_count, head_FunctionList(l));
  walk_functions(inst_count, tail_FunctionList(l));
}

void arch(IR* ir) {
  walk_functions(&ir->inst_count, ir->functions);
}
