#include "dead_code_elim.h"

static void perform_dce(BitSet* live, IRInst* inst) {
  if (inst->rd == NULL) {
    return;
  }

  if (get_BitSet(live, inst->rd->virtual)) {
    return;
  }

  if (inst->kind == IR_CALL) {
    release_Reg(inst->rd);
    inst->rd = NULL;
    return;
  }

  release_Reg(inst->rd);
  inst->rd = NULL;
  resize_RegVec(inst->ras, 0);
  inst->kind = IR_NOP;
}

static void update_live(BitSet* live, IRInst* inst) {
  if (inst->rd != NULL) {
    set_BitSet(live, inst->rd->virtual, false);
  }
  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* ra = get_RegVec(inst->ras, i);
    set_BitSet(live, ra->virtual, true);
  }
}

static void dead_code_elim_function(Function* f) {
  BBList* l = f->blocks;
  while (!is_nil_BBList(l)) {
    BasicBlock* bb = head_BBList(l);
    if (bb->dead) {
      l = tail_BBList(l);
      continue;
    }

    BitSet* live = copy_BitSet(bb->live_out);
    for (unsigned i = length_IRInstVec(bb->sorted_insts); i > 0; i--) {
      IRInst* inst = get_IRInstVec(bb->sorted_insts, i - 1);
      update_live(live, inst);
      perform_dce(live, inst);
    }
    assert(equal_to_BitSet(bb->live_in, live));

    l = tail_BBList(l);
  }
}

void dead_code_elim(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);
    dead_code_elim_function(f);
    l = tail_FunctionList(l);
  }
}
