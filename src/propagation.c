#include "propagation.h"

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

static void elim_branch(bool c, BasicBlock* bb, IRInst* inst) {
  BasicBlock *selected, *discarded;
  if (c) {
    selected  = inst->then_;
    discarded = inst->else_;
  } else {
    selected  = inst->else_;
    discarded = inst->then_;
  }
  disconnect_BasicBlock(bb, discarded);
  inst->kind  = IR_JUMP;
  inst->jump  = selected;
  inst->then_ = inst->else_ = NULL;
  resize_RegVec(inst->ras, 0);
}

static void perform_propagation(Function* f, BasicBlock* bb, IRInst* inst) {
  IRInstVec* defs = new_IRInstVec(length_RegVec(inst->ras));
  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* r      = get_RegVec(inst->ras, i);
    IRInst* def = r->irreplaceable ? NULL : obtain_definition(f, inst->reach_in, r);
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
    case IR_CMP: {
      IRInst* lhs_def = get_IRInstVec(defs, 0);
      IRInst* rhs_def = get_IRInstVec(defs, 1);

      if (is_imm_inst(rhs_def)) {
        if (is_imm_inst(lhs_def)) {
          // foldable
          bool c     = eval_CompareOp(inst->predicate_op, lhs_def->imm, rhs_def->imm);
          inst->kind = IR_IMM;
          inst->imm  = c;
          resize_RegVec(inst->ras, 0);
        } else {
          // not foldable, but able to propagate
          inst->kind = IR_CMP_IMM;
          inst->imm  = rhs_def->imm;
          resize_RegVec(inst->ras, 1);
        }
      }
      break;
    }
    case IR_BR_CMP: {
      IRInst* lhs_def = get_IRInstVec(defs, 0);
      IRInst* rhs_def = get_IRInstVec(defs, 1);

      if (is_imm_inst(rhs_def)) {
        if (is_imm_inst(lhs_def)) {
          // foldable
          bool c = eval_CompareOp(inst->predicate_op, lhs_def->imm, rhs_def->imm);
          elim_branch(c, bb, inst);
        } else {
          // not foldable, but able to propagate
          inst->kind = IR_BR_CMP_IMM;
          inst->imm  = rhs_def->imm;
          resize_RegVec(inst->ras, 1);
        }
      }
      break;
    }
    case IR_BIN_IMM: {
      IRInst* lhs_def = get_IRInstVec(defs, 0);

      if (is_imm_inst(lhs_def)) {
        // foldable
        long c     = eval_ArithOp(inst->binary_op, lhs_def->imm, inst->imm);
        inst->kind = IR_IMM;
        inst->imm  = c;
        resize_RegVec(inst->ras, 0);
      }
      break;
    }
    case IR_CMP_IMM: {
      IRInst* lhs_def = get_IRInstVec(defs, 0);

      if (is_imm_inst(lhs_def)) {
        // foldable
        bool c     = eval_CompareOp(inst->predicate_op, lhs_def->imm, inst->imm);
        inst->kind = IR_IMM;
        inst->imm  = c;
        resize_RegVec(inst->ras, 0);
      }
      break;
    }
    case IR_BR_CMP_IMM: {
      IRInst* lhs_def = get_IRInstVec(defs, 0);

      if (is_imm_inst(lhs_def)) {
        // foldable
        bool c = eval_CompareOp(inst->predicate_op, lhs_def->imm, inst->imm);
        elim_branch(c, bb, inst);
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
  for (BBListIterator* it1 = front_BBList(f->blocks); !is_nil_BBListIterator(it1);
       it1                 = next_BBListIterator(it1)) {
    BasicBlock* b = data_BBListIterator(it1);

    for (IRInstListIterator* it2 = back_IRInstList(b->insts); !is_nil_IRInstListIterator(it2);
         it2                     = prev_IRInstListIterator(it2)) {
      IRInst* inst = data_IRInstListIterator(it2);
      perform_propagation(f, b, inst);
    }
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
