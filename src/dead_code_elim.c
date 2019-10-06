#include "dead_code_elim.h"

static void perform_dce(IRInstList* list, IRInstListIterator* it) {
  IRInst* inst = data_IRInstListIterator(it);
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

  remove_IRInstListIterator(list, it);
}

static void dead_code_elim_function(Function* f) {
  IRInstListIterator* it = front_IRInstList(f->instructions);
  while (!is_nil_IRInstListIterator(it)) {
    IRInstListIterator* next = next_IRInstListIterator(it);
    perform_dce(f->instructions, it);
    it = next;
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
