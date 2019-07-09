#include "reg_alloc.h"
#include "vector.h"

typedef struct {
  unsigned inst_count;
  IntVec* last_uses;
} Env;

Env* init_env(unsigned reg_count) {
  Env* e = calloc(1, sizeof(Env));
  e->inst_count = 0;
  e->last_uses = new_IntVec(reg_count);
  resize_IntVec(e->last_uses, reg_count);
  return e;
}

void add_use(Env* env, Reg r) {
  set_IntVec(env->last_uses, r.virtual, env->inst_count);
}

void last_use(Env* env, IRInstList* insts) {
  if (is_nil_IRInstList(insts)) {
    return;
  }

  IRInst i = head_IRInstList(insts);
  add_use(env, i.rd);
  add_use(env, i.ra);

  env->inst_count++;

  last_use(env, tail_IRInstList(insts));
}

IR* reg_alloc(int num_regs, IR* ir) {
  Env* env = init_env(ir->reg_count);
  last_use(env, ir->insts);
  return ir;
}
