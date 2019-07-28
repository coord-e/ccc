#include "reorder.h"

typedef struct {
  BBVec* bbs;
  BitSet* visited;
} Env;

Env* init_Env(unsigned expected_bb_count) {
  Env* env     = calloc(1, sizeof(Env));
  env->bbs     = new_BBVec(expected_bb_count);
  env->visited = zero_BitSet(expected_bb_count);
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
  if (get_BitSet(env->visited, b->id)) {
    return;
  }
  set_BitSet(env->visited, b->id, true);

  traverse_bblist(env, b->succs);
  push_BBVec(env->bbs, b);
}

void traverse_insts(unsigned* count, IRInstVec* acc, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }

  IRInst* inst = head_IRInstList(l);
  inst->id     = (*count)++;
  push_IRInstVec(acc, inst);

  traverse_insts(count, acc, tail_IRInstList(l));
}

void number_insts(BBVec* v) {
  unsigned inst_count = 0;
  unsigned len_bbs    = length_BBVec(v);
  for (unsigned i = len_bbs; i > 0; i--) {
    BasicBlock* b   = get_BBVec(v, i - 1);
    b->id           = len_bbs - i;
    b->sorted_insts = new_IRInstVec(32);  // TODO: allocate accurate number of insts
    traverse_insts(&inst_count, b->sorted_insts, b->insts);
  }
}

static bool mark_dead_iter(BasicBlock* entry, IntVec* visited, BBList* l, bool acc) {
  if (is_nil_BBList(l)) {
    return acc;
  }

  BasicBlock* b = head_BBList(l);

  int v = get_IntVec(visited, b->id);
  if (v != -1) {
    return v;
  }

  if (b->id != entry->id && mark_dead_iter(entry, visited, b->preds, true)) {
    b->dead = true;
  }

  set_IntVec(visited, b->id, b->dead);

  return mark_dead_iter(entry, visited, tail_BBList(l), acc && b->dead);
}

static void mark_dead(unsigned bb_count, BasicBlock* entry, BasicBlock* exit) {
  IntVec* visited = new_IntVec(bb_count);
  resize_IntVec(visited, bb_count);
  fill_IntVec(visited, -1);
  mark_dead_iter(entry, visited, exit->preds, true);
  release_IntVec(visited);
}

// change `id`s of `BasicBlock` and `IRInst`
// and collect `BasicBlock`s to `sorted_blocks` in reversed order
void reorder_blocks(IR* ir) {
  Env* env = init_Env(ir->bb_count);

  mark_dead(ir->bb_count, ir->entry, ir->exit);

  traverse_blocks(env, ir->entry);
  ir->sorted_blocks = env->bbs;
  release_BitSet(env->visited);

  number_insts(ir->sorted_blocks);
}
