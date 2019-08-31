#include "dead_code_elim.h"

static void perform_dce(BitSet* live, IRInst* inst) {}
static void update_live(BitSet* live, IRInst* inst) {}

static void dead_code_elim_function(Function* f) {
  BBList* l = f->blocks;
  while (!is_nil_BBList(l)) {
    BasicBlock* bb = head_BBList(l);
    if (bb->dead) {
      l = tail_BBList(l);
      continue;
    }

    BitSet* live = copy_BitSet(bb->live_in);
    for (unsigned i = 0; i < length_IRInstVec(bb->sorted_insts); i++) {
      IRInst* inst = get_IRInstVec(bb->sorted_insts, i);
      perform_dce(live, inst);
      update_live(live, inst);
    }
    assert(equal_to_BitSet(bb->live_out, live));

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
