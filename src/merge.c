#include "merge.h"

void merge_two(BasicBlock* from, BasicBlock* to) {
  assert(is_single_BBList(from->succs));
  assert(is_single_BBList(to->preds));
  assert(head_BBList(from->succs)->global_id == head_BBList(to->preds)->global_id);

  // TODO: efficiency (list last)
  IRInstList* from_last  = last_IRInstList(from->insts);
  IRInstList* to_last    = last_IRInstList(to->insts);
  IRInstList* to_head    = to->insts;
  IRInst* from_last_inst = head_IRInstList(from_last);
  IRInst* to_last_inst   = head_IRInstList(to_last);
  IRInst* to_head_inst   = head_IRInstList(to_head);

  assert(to_head_inst->kind == IR_LABEL);

  switch (from_last_inst->kind) {
    case IR_RET:
      assert(to_last_inst->kind == IR_RET);
      assert(length_RegVec(to_last_inst->ras) == 0);
      // fallthrough
    case IR_JUMP:
      remove_IRInstList(from_last);
      remove_IRInstList(to_head);
      append_IRInstList(from->insts, to->insts);
      from->succs = to->succs;
      release_BasicBlock(to);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

void merge_blocks_search(Function* f, BasicBlock* b1) {
  BBList* l = b1->preds;
  while (!is_nil_BBList(l)) {
    BasicBlock* b2 = head_BBList(l);
    merge_blocks_search(f, b2);
    l = tail_BBList(l);
  }

  if (is_single_BBList(b1->preds)) {
    BasicBlock* t = head_BBList(b1->preds);
    if (is_single_BBList(t->succs)) {
      if (f->exit == b1) {
        f->exit = t;
      }

      merge_two(t, b1);
    }
  }
}

void merge_blocks(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);
    merge_blocks_search(f, f->exit);
    l = tail_FunctionList(l);
  }
}
