#include "reorder.h"

typedef struct {
  BBList* bbs;
  BitSet* visited;
} Env;

Env* init_Env(unsigned expected_bb_count) {
  Env* env     = calloc(1, sizeof(Env));
  env->bbs     = new_BBList();
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
  push_front_BBList(env->bbs, b);
}

void traverse_insts(Function* f, unsigned* count, IRInstListIterator* it) {
  if (is_nil_IRInstListIterator(it)) {
    return;
  }

  IRInst* inst   = data_IRInstListIterator(it);
  inst->local_id = (*count)++;
  push_IRInstVec(f->sorted_insts, inst);

  traverse_insts(f, count, next_IRInstListIterator(it));
}

void number_insts_and_blocks(Function* f) {
  f->sorted_insts     = new_IRInstVec(32);  // TODO: allocate accurate number of insts
  unsigned inst_count = 0;
  unsigned bb_count   = 0;
  for (BBListIterator* it = front_BBList(f->blocks); !is_nil_BBListIterator(it);
       it                 = next_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);
    b->local_id   = bb_count++;
    traverse_insts(f, &inst_count, front_IRInstList(b->insts));
  }
}

// change `local_id`s of `BasicBlock` and `IRInst`
static void reorder_blocks_function(Function* ir) {
  Env* env = init_Env(ir->bb_count);

  traverse_blocks(env, ir->entry);
  if (ir->blocks) {
    // TODO: shallow release
    /* release_BBList(ir->blocks); */
  }
  ir->blocks = env->bbs;
  release_BitSet(env->visited);

  number_insts_and_blocks(ir);
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

static void remove_dead_functions(FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  Function* f = head_FunctionList(l);
  remove_dead(f, f->entry);

  remove_dead_functions(tail_FunctionList(l));
}

void remove_dead_blocks(IR* ir) {
  remove_dead_functions(ir->functions);
}
