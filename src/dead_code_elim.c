#include "dead_code_elim.h"

static void perform_dce(IRInst* inst) {
  if (inst->rd == NULL) {
    return;
  }

  if (get_BitSet(inst->live_out, inst->rd->virtual)) {
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

static void dead_code_elim_function(Function* f) {
  BBListIterator* it = front_BBList(f->blocks);
  while (!is_nil_BBListIterator(it)) {
    BasicBlock* bb = data_BBListIterator(it);

    for (unsigned i = 0; i < length_IRInstVec(bb->sorted_insts); i++) {
      perform_dce(get_IRInstVec(bb->sorted_insts, i));
    }

    it = next_BBListIterator(it);
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
