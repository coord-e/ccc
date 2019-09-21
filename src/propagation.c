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

static IRInst* obtain_definition(Function* f, BitSet* reach, Reg* r) {
  BitSet* defs = copy_BitSet(get_BSVec(f->definitions, r->virtual));
  and_BitSet(defs, reach);

  assert(count_BitSet(defs) != 0);
  if (count_BitSet(defs) != 1) {
    return NULL;
  }
  for (unsigned i = 0; i < length_BitSet(defs); i++) {
    if (!get_BitSet(defs, i)) {
      continue;
    }

    IRInst* def = get_IRInstVec(f->sorted_insts, i);
    assert(def->rd->virtual == r->virtual);

    release_BitSet(defs);
    return def;
  }
  CCC_UNREACHABLE;
}

static bool is_imm_inst(IRInst* inst) {
  if (inst == NULL) {
    return false;
  }
  return inst->kind == IR_IMM;
}

static void perform_propagation(Function* f, BitSet* reach, IRInst* inst) {
  IRInstVec* defs = new_IRInstVec(length_RegVec(inst->ras));
  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* r      = get_RegVec(inst->ras, i);
    IRInst* def = r->irreplaceable ? NULL : obtain_definition(f, reach, r);
    // `def` is possibly null
    push_IRInstVec(defs, def);
  }

  switch (inst->kind) {
    case IR_MOV: {
      IRInst* def = get_IRInstVec(defs, 0);
      if (def == NULL) {
        break;
      }
      if (def->kind == IR_IMM) {
        inst->kind = IR_IMM;
        inst->imm  = def->imm;
        resize_RegVec(inst->ras, 0);
      }
      break;
    }
    case IR_BIN: {
      IRInst* lhs_def = get_IRInstVec(defs, 0);
      IRInst* rhs_def = get_IRInstVec(defs, 1);

      if (is_imm_inst(rhs_def)) {
        if (is_imm_inst(lhs_def)) {
          // foldable
          long c     = eval_ArithOp(inst->binary_op, lhs_def->imm, rhs_def->imm);
          inst->kind = IR_IMM;
          inst->imm  = c;
          resize_RegVec(inst->ras, 0);
        } else {
          // not foldable, but able to propagate
          inst->kind = IR_BIN_IMM;
          inst->imm  = rhs_def->imm;
          resize_RegVec(inst->ras, 1);
        }
      }
      break;
    }
    default:
      break;
  }

  for (unsigned i = 0; i < length_IRInstVec(defs); i++) {
    IRInst* def = get_IRInstVec(defs, i);
    if (def == NULL) {
      continue;
    }

    if (def->kind == IR_MOV) {
      Reg* r = get_RegVec(def->ras, 0);
      if (r->irreplaceable) {
        break;
      }
      release_Reg(get_RegVec(inst->ras, i));
      set_RegVec(inst->ras, i, copy_Reg(r));
    }
  }
}

static void propagation_function(Function* f) {
  BBListIterator* it = front_BBList(f->blocks);
  while (!is_nil_BBListIterator(it)) {
    BasicBlock* bb = data_BBListIterator(it);

    BitSet* reach = copy_BitSet(bb->reach_in);
    for (unsigned i = 0; i < length_IRInstVec(bb->sorted_insts); i++) {
      IRInst* inst = get_IRInstVec(bb->sorted_insts, i);
      perform_propagation(f, reach, inst);
      update_reach(f, reach, inst);
    }
    assert(equal_to_BitSet(bb->reach_out, reach));

    it = next_BBListIterator(it);
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
