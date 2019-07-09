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

void set_as_used(Env* env, Reg r) {
  // zero -> unused
  if (r.virtual != 0) {
    set_IntVec(env->last_uses, r.virtual - 1, env->inst_count);
  }
}

void collect_last_uses(Env* env, IRInstList* insts) {
  if (is_nil_IRInstList(insts)) {
    return;
  }

  IRInst i = head_IRInstList(insts);
  set_as_used(env, i.rd);
  set_as_used(env, i.ra);

  env->inst_count++;

  collect_last_uses(env, tail_IRInstList(insts));
}

IR* reg_alloc(int num_regs, IR* ir) {
  Env* env = init_env(ir->reg_count);
  collect_last_uses(env, ir->insts);
  return ir;
}
