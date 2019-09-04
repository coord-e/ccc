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

void traverse_bblist(Env* env, BBRefListIterator* it) {
  if (is_nil_BBRefListIterator(it)) {
    return;
  }

  traverse_blocks(env, data_BBRefListIterator(it));

  traverse_bblist(env, next_BBRefListIterator(it));
}

void traverse_blocks(Env* env, BasicBlock* b) {
  if (get_BitSet(env->visited, b->local_id)) {
    return;
  }
  set_BitSet(env->visited, b->local_id, true);

  traverse_bblist(env, front_BBRefList(b->succs));
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

static void mark_visited(BitSet* visited, BasicBlock* target) {
  if (get_BitSet(visited, target->local_id)) {
    return;
  }
  set_BitSet(visited, target->local_id, true);

  BBRefListIterator* it = front_BBRefList(target->succs);
  while (!is_nil_BBRefListIterator(it)) {
    BasicBlock* suc = data_BBRefListIterator(it);
    mark_visited(visited, suc);
    it = next_BBRefListIterator(it);
  }
}

static void remove_dead(Function* f, BasicBlock* entry) {
  BitSet* visited = zero_BitSet(f->bb_count);
  mark_visited(visited, entry);

  BBListIterator* it = front_BBList(f->blocks);
  while (!is_nil_BBListIterator(it)) {
    BasicBlock* bb = data_BBListIterator(it);
    it             = next_BBListIterator(it);
    if (!get_BitSet(visited, bb->local_id)) {
      detach_BasicBlock(f, bb);
    }
  }
  release_BitSet(visited);
}

// change `local_id`s of `BasicBlock` and `IRInst`
// and collect `BasicBlock`s to `sorted_blocks` in reversed order
static void reorder_blocks_function(Function* ir) {
  Env* env = init_Env(ir->bb_count);

  remove_dead(ir, ir->entry);

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
