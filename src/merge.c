#include "merge.h"

static bool has_single(BBList* l, BasicBlock** dst) {
  BasicBlock* encounted = NULL;
  while (!is_nil_BBList(l)) {
    BasicBlock* bb = head_BBList(l);
    if (!bb->dead) {
      if (encounted != NULL) {
        return false;
      }
      encounted = bb;
    }
    l = tail_BBList(l);
  }
  if (encounted != NULL) {
    if (dst != NULL) {
      *dst = encounted;
    }
    return true;
  } else {
    return false;
  }
}

static bool merge_assertion(BasicBlock* from, BasicBlock* to) {
  BasicBlock *fb, *tb;
  if (!has_single(from->succs, &fb)) {
    return false;
  }
  if (!has_single(to->preds, &tb)) {
    return false;
  }
  return from->global_id == tb->global_id && to->global_id == fb->global_id;
}

static void merge_two(BasicBlock* from, BasicBlock* to) {
  assert(merge_assertion(from, to));

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
      push_RegVec(to_last_inst->ras, copy_Reg(get_RegVec(from_last_inst->ras, 0)));
      // fallthrough
    case IR_JUMP:
      remove_IRInstList(from_last);
      remove_IRInstList(to_head);
      append_IRInstList(from->insts, to->insts);
      to->insts = nil_IRInstList();
      from->succs = copy_BBList(to->succs);
      to->dead    = true;
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

  if (b1->dead) {
    return;
  }

  BasicBlock* t;
  if (has_single(b1->preds, &t)) {
    if (has_single(t->succs, NULL)) {
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
