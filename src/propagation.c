#include "propagation.h"

static void update_reach(BitSet* reach, IRInst* inst) {
  error("unimplemented");
}

static void perform_propagation(Function* f, BasicBlock* bb, BitSet* reach, IRInst* inst) {
  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* ra      = get_RegVec(inst->ras, i);
    BitSet* defs = copy_BitSet(get_BSVec(f->definitions, ra->virtual));
    and_BitSet(defs, reach);
    if (count_BitSet(defs) != 1) {
      continue;
    }
    for (unsigned j = 0; j < length_BitSet(defs); j++) {
      if (!get_BitSet(defs, j)) {
        continue;
      }
      IRInst* def_inst = get_IRInstVec(bb->sorted_insts, j);
      switch (def_inst->kind) {
        case IR_MOV: {
          assert(def_inst->rd->virtual == ra->virtual);
          Reg* r = get_RegVec(def_inst->ras, 0);
          release_Reg(ra);
          set_RegVec(inst->ras, i, copy_Reg(r));
        } break;
        default:
          break;
      }
      break;
    }
    release_BitSet(defs);
  }
}

static void propagation_function(Function* f) {
  BBList* l = f->blocks;
  while (!is_nil_BBList(l)) {
    BasicBlock* bb = head_BBList(l);
    if (bb->dead) {
      l = tail_BBList(l);
      continue;
    }

    BitSet* reach  = copy_BitSet(bb->reach_in);
    for (unsigned i = 0; i < length_IRInstVec(bb->sorted_insts); i++) {
      IRInst* inst = get_IRInstVec(bb->sorted_insts, i);
      perform_propagation(f, bb, reach, inst);
      update_reach(reach, inst);
    }
    l = tail_BBList(l);
  }
}

void propagation(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);
    propagation_function(f);
    l = tail_FunctionList(l);
  }
}
