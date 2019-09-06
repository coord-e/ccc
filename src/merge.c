#include "merge.h"

static void reconnect_blocks(BasicBlock* from, BBRefList* l) {
  BBRefListIterator* it = front_BBRefList(l);
  while (!is_nil_BBRefListIterator(it)) {
    BasicBlock* to = data_BBRefListIterator(it);
    connect_BasicBlock(from, to);
    it = next_BBRefListIterator(it);
  }
}

static bool merge_assertion(BasicBlock* from, BasicBlock* to) {
  if (from->is_call_bb || to->is_call_bb) {
    return false;
  }
  if (!is_single_BBRefList(from->succs)) {
    return false;
  }
  if (!is_single_BBRefList(to->preds)) {
    return false;
  }
  BasicBlock* fb = head_BBRefList(from->succs);
  BasicBlock* tb = head_BBRefList(to->preds);
  return from->global_id == tb->global_id && to->global_id == fb->global_id;
}

static void merge_two(Function* f, BasicBlock* from, BasicBlock* to) {
  assert(merge_assertion(from, to));

  IRInstListIterator* from_last = back_IRInstList(from->insts);
  IRInstListIterator* to_last   = back_IRInstList(to->insts);
  IRInstListIterator* to_head   = front_IRInstList(to->insts);
  IRInst* from_last_inst        = data_IRInstListIterator(from_last);
  IRInst* to_last_inst          = data_IRInstListIterator(to_last);
  IRInst* to_head_inst          = data_IRInstListIterator(to_head);

  assert(to_head_inst->kind == IR_LABEL);

  switch (from_last_inst->kind) {
    case IR_RET:
      assert(to_last_inst->kind == IR_RET);
      assert(length_RegVec(to_last_inst->ras) == 0);
      push_RegVec(to_last_inst->ras, copy_Reg(get_RegVec(from_last_inst->ras, 0)));
      // fallthrough
    case IR_JUMP:
      remove_IRInstListIterator(from_last);
      remove_IRInstListIterator(to_head);
      append_IRInstList(from->insts, to->insts);
      // prevent `to->insts` (which is now included in `from->insts`)
      // from being released by detaching of `to`
      to->insts        = new_IRInstList();
      BBRefList* succs = shallow_copy_BBRefList(to->succs);
      detach_BasicBlock(f, to);
      reconnect_blocks(from, succs);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

void merge_blocks_search(BitSet* visited, Function* f, BasicBlock* b1) {
  if (get_BitSet(visited, b1->local_id)) {
    return;
  }
  set_BitSet(visited, b1->local_id, true);

  BBRefListIterator* it = front_BBRefList(b1->preds);
  while (!is_nil_BBRefListIterator(it)) {
    BasicBlock* b2 = data_BBRefListIterator(it);
    it             = next_BBRefListIterator(it);
    merge_blocks_search(visited, f, b2);
  }

  if (!b1->is_call_bb && is_single_BBRefList(b1->preds)) {
    BasicBlock* t = head_BBRefList(b1->preds);
    if (!t->is_call_bb && is_single_BBRefList(t->succs)) {
      if (f->exit == b1) {
        f->exit = t;
      }

      merge_two(f, t, b1);
    }
  }
}

void merge_blocks(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);

    BitSet* visited = zero_BitSet(f->bb_count);
    merge_blocks_search(visited, f, f->exit);
    release_BitSet(visited);

    l = tail_FunctionList(l);
  }
}
