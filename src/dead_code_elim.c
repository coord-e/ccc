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
  for (BBListIterator* it1 = front_BBList(f->blocks); !is_nil_BBListIterator(it1);
       it1                 = next_BBListIterator(it1)) {
    BasicBlock* b = data_BBListIterator(it1);

    for (IRInstListIterator* it2 = back_IRInstList(b->insts); !is_nil_IRInstListIterator(it2);
         it2                     = prev_IRInstListIterator(it2)) {
      IRInst* inst = data_IRInstListIterator(it2);
      perform_dce(inst);
    }
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
