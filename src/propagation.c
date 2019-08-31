#include "propagation.h"

static void update_reach(Function* f, BitSet* reach, IRInst* inst) {
  if (inst->rd == NULL) {
    return;
  }
  unsigned id  = inst->local_id;
  BitSet* defs = copy_BitSet(get_BSVec(f->definitions, inst->rd->virtual));
  set_BitSet(defs, id, false);
  diff_BitSet(reach, defs);
  set_BitSet(reach, id, true);
  release_BitSet(defs);
}

static IRInst* lookup_inst(Function* f, unsigned target) {
  for (unsigned i = 0; i < length_BBVec(f->sorted_blocks); i++) {
    BasicBlock* bb = get_BBVec(f->sorted_blocks, i);
    for (unsigned j = 0; j < length_IRInstVec(bb->sorted_insts); j++) {
      IRInst* inst = get_IRInstVec(bb->sorted_insts, j);
      if (inst->local_id == target) {
        return inst;
      }
    }
  }
  CCC_UNREACHABLE;
}

static void perform_propagation(Function* f, BitSet* reach, IRInst* inst) {
  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* ra = get_RegVec(inst->ras, i);
    if (ra->irreplaceable) {
      continue;
    }

    BitSet* defs = copy_BitSet(get_BSVec(f->definitions, ra->virtual));
    and_BitSet(defs, reach);

    assert(count_BitSet(defs) != 0);
    if (count_BitSet(defs) != 1) {
      continue;
    }
    for (unsigned j = 0; j < length_BitSet(defs); j++) {
      if (!get_BitSet(defs, j)) {
        continue;
      }
      IRInst* def_inst = lookup_inst(f, j);
      switch (def_inst->kind) {
        case IR_MOV: {
          assert(def_inst->rd->virtual == ra->virtual);
          Reg* r = get_RegVec(def_inst->ras, 0);
          if (r->irreplaceable) {
            break;
          }
          release_Reg(ra);
          set_RegVec(inst->ras, i, copy_Reg(r));
          break;
        }
        case IR_IMM: {
          if (inst->kind != IR_MOV) {
            break;
          }
          inst->kind = IR_IMM;
          inst->imm  = def_inst->imm;
          resize_RegVec(inst->ras, 0);
          break;
        }
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

    BitSet* reach = copy_BitSet(bb->reach_in);
    for (unsigned i = 0; i < length_IRInstVec(bb->sorted_insts); i++) {
      IRInst* inst = get_IRInstVec(bb->sorted_insts, i);
      perform_propagation(f, reach, inst);
      update_reach(f, reach, inst);
    }
    assert(equal_to_BitSet(bb->reach_out, reach));

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
