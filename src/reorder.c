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
  if (get_BitSet(env->visited, b->local_id)) {
    return;
  }
  set_BitSet(env->visited, b->local_id, true);

  traverse_bblist(env, b->succs);
  push_BBVec(env->bbs, b);
}

void traverse_insts(Function* f, unsigned* count, IRInstVec* acc, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }

  IRInst* inst   = head_IRInstList(l);
  inst->local_id = (*count)++;
  push_IRInstVec(acc, inst);
  push_IRInstVec(f->sorted_insts, inst);

  traverse_insts(f, count, acc, tail_IRInstList(l));
}

void number_insts(Function* f, BBVec* v) {
  f->sorted_insts     = new_IRInstVec(32);  // TODO: allocate accurate number of insts
  unsigned inst_count = 0;
  unsigned len_bbs    = length_BBVec(v);
  for (unsigned i = len_bbs; i > 0; i--) {
    BasicBlock* b   = get_BBVec(v, i - 1);
    b->local_id     = len_bbs - i;
    b->sorted_insts = new_IRInstVec(32);  // TODO: allocate accurate number of insts
    traverse_insts(f, &inst_count, b->sorted_insts, b->insts);
  }
}

static bool mark_dead_iter(BasicBlock* entry, IntVec* visited, BBList* l, bool acc) {
  if (is_nil_BBList(l)) {
    return acc;
  }

  BasicBlock* b = head_BBList(l);

  int v = get_IntVec(visited, b->local_id);
  if (v != -1) {
    return mark_dead_iter(entry, visited, tail_BBList(l), acc && v);
  }

  set_IntVec(visited, b->local_id, false);

  if (b->local_id != entry->local_id && mark_dead_iter(entry, visited, b->preds, true)) {
    b->dead = true;
  }

  set_IntVec(visited, b->local_id, b->dead);

  return mark_dead_iter(entry, visited, tail_BBList(l), acc && b->dead);
}

// change `local_id`s of `BasicBlock` and `IRInst`
// and collect `BasicBlock`s to `sorted_blocks` in reversed order
static void reorder_blocks_function(Function* ir) {
  Env* env = init_Env(ir->bb_count);

  traverse_blocks(env, ir->entry);
  ir->sorted_blocks = env->bbs;
  release_BitSet(env->visited);

  number_insts(ir, ir->sorted_blocks);
}

static void reorder_blocks_functions(FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  reorder_blocks_function(head_FunctionList(l));

  reorder_blocks_functions(tail_FunctionList(l));
}

void reorder_blocks(IR* ir) {
  reorder_blocks_functions(ir->functions);
}

static void mark_dead(unsigned bb_count, BasicBlock* entry, BasicBlock* exit) {
  IntVec* visited = new_IntVec(bb_count);
  resize_IntVec(visited, bb_count);
  fill_IntVec(visited, -1);
  mark_dead_iter(entry, visited, exit->preds, true);
  release_IntVec(visited);
}

static void mark_dead_functions(FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  Function* f = head_FunctionList(l);
  mark_dead(f->bb_count, f->entry, f->exit);

  reorder_blocks_functions(tail_FunctionList(l));
}

void mark_dead_blocks(IR* ir) {
  mark_dead_functions(ir->functions);
}
