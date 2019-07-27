#include "reorder.h"

typedef struct {
  unsigned bb_count;
  unsigned inst_count;
  BBVec* bbs;
} Env;

Env* init_Env(unsigned expected_bb_count) {
  Env* env        = calloc(1, sizeof(Env));
  env->bb_count   = 0;
  env->inst_count = 0;
  env->bbs        = new_BBVec(expected_bb_count);
  return env;
}

void traverse_blocks(Env* env, BasicBlock* b);

void traverse_bblist(Env* env, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }

  traverse_blocks(env, head_BBList(l));

  traverse_bblist(env, tail_BBList(l));
}

void traverse_blocks(Env* env, BasicBlock* b) {
  b->id = env->bb_count++;
  push_BBVec(env->bbs, b);
  traverse_bblist(env, b->succs);
}

void traverse_insts(Env* env, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }

  IRInst* inst = head_IRInstList(l);
  inst->id     = env->inst_count++;

  traverse_insts(env, tail_IRInstList(l));
}

void number_insts(Env* env, BBVec* v) {
  for (unsigned i = 0; i < length_BBVec(v); i++) {
    BasicBlock* b = get_BBVec(v, i);
    traverse_insts(env, b->insts);
  }
}

// change `id`s of `BasicBlock` and `IRInst`
// and collect `BasicBlock`s to `sorted_blocks` in order
void reorder_blocks(IR* ir) {
  Env* env = init_Env(ir->bb_count);
  traverse_blocks(env, ir->entry);
  ir->sorted_blocks = env->bbs;
  number_insts(env, ir->sorted_blocks);
}
