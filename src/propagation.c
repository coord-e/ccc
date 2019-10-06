#include "propagation.h"
#include "util.h"

static bool is_propagatable(Reg* r) {
  return r->one_def != NULL && !r->sticky;
}

static bool get_imm(Reg* r, long* out) {
  if (!is_propagatable(r)) {
    return false;
  }
  if (r->one_def->kind == IR_IMM) {
    *out = r->one_def->imm;
    return true;
  } else {
    return false;
  }
}

static BasicBlock* find_parent_block(IRInst* inst) {
  // TODO: Use cheaper way to obtain corresponding block
  assert(inst->kind == IR_BR || inst->kind == IR_BR_CMP || inst->kind == IR_BR_CMP_IMM);
  for (BBRefListIterator* it = front_BBRefList(inst->then_->preds); !is_nil_BBRefListIterator(it);
       it                    = next_BBRefListIterator(it)) {
    BasicBlock* b = data_BBRefListIterator(it);
    if (last_IRInstRange(b->instructions) == inst) {
      return b;
    }
  }
  CCC_UNREACHABLE;
}

static void elim_branch(bool c, IRInst* inst) {
  BasicBlock* bb = find_parent_block(inst);
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

static void perform_propagation(Function* f, IRInst* inst) {
  switch (inst->kind) {
    case IR_MOV: {
      Reg* r = get_RegVec(inst->ras, 0);

      long imm;
      if (get_imm(r, &imm)) {
        inst->kind = IR_IMM;
        inst->imm  = imm;
        resize_RegVec(inst->ras, 0);
      }
      break;
    }
    case IR_BIN: {
      Reg* lhs = get_RegVec(inst->ras, 0);
      Reg* rhs = get_RegVec(inst->ras, 1);

      long lhs_imm, rhs_imm;
      if (get_imm(rhs, &rhs_imm)) {
        if (get_imm(lhs, &lhs_imm)) {
          // foldable
          long c     = eval_ArithOp(inst->binary_op, lhs_imm, rhs_imm);
          inst->kind = IR_IMM;
          inst->imm  = c;
          resize_RegVec(inst->ras, 0);
        } else {
          // not foldable, but able to propagate
          inst->kind = IR_BIN_IMM;
          inst->imm  = rhs_imm;
          resize_RegVec(inst->ras, 1);
        }
      }
      break;
    }
    case IR_CMP: {
      Reg* lhs = get_RegVec(inst->ras, 0);
      Reg* rhs = get_RegVec(inst->ras, 1);

      long lhs_imm, rhs_imm;
      if (get_imm(rhs, &rhs_imm)) {
        if (get_imm(lhs, &lhs_imm)) {
          // foldable
          bool c     = eval_CompareOp(inst->predicate_op, lhs_imm, rhs_imm);
          inst->kind = IR_IMM;
          inst->imm  = c;
          resize_RegVec(inst->ras, 0);
        } else {
          // not foldable, but able to propagate
          inst->kind = IR_CMP_IMM;
          inst->imm  = rhs_imm;
          resize_RegVec(inst->ras, 1);
        }
      }
      break;
    }
    case IR_BR_CMP: {
      Reg* lhs = get_RegVec(inst->ras, 0);
      Reg* rhs = get_RegVec(inst->ras, 1);

      long lhs_imm, rhs_imm;
      if (get_imm(rhs, &rhs_imm)) {
        if (get_imm(lhs, &lhs_imm)) {
          // foldable
          bool c = eval_CompareOp(inst->predicate_op, lhs_imm, rhs_imm);
          elim_branch(c, inst);
        } else {
          // not foldable, but able to propagate
          inst->kind = IR_BR_CMP_IMM;
          inst->imm  = rhs_imm;
          resize_RegVec(inst->ras, 1);
        }
      }
      break;
    }
    case IR_BIN_IMM: {
      Reg* lhs = get_RegVec(inst->ras, 0);

      long lhs_imm;
      if (get_imm(lhs, &lhs_imm)) {
        // foldable
        long c     = eval_ArithOp(inst->binary_op, lhs_imm, inst->imm);
        inst->kind = IR_IMM;
        inst->imm  = c;
        resize_RegVec(inst->ras, 0);
      }
      break;
    }
    case IR_CMP_IMM: {
      Reg* lhs = get_RegVec(inst->ras, 0);

      long lhs_imm;
      if (get_imm(lhs, &lhs_imm)) {
        // foldable
        bool c     = eval_CompareOp(inst->predicate_op, lhs_imm, inst->imm);
        inst->kind = IR_IMM;
        inst->imm  = c;
        resize_RegVec(inst->ras, 0);
      }
      break;
    }
    case IR_BR_CMP_IMM: {
      Reg* lhs = get_RegVec(inst->ras, 0);

      long lhs_imm;
      if (get_imm(lhs, &lhs_imm)) {
        // foldable
        bool c = eval_CompareOp(inst->predicate_op, lhs_imm, inst->imm);
        elim_branch(c, inst);
      }
      break;
    }
    default:
      break;
  }

  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* ra = get_RegVec(inst->ras, i);
    if (!is_propagatable(ra)) {
      continue;
    }

    if (ra->one_def->kind == IR_MOV) {
      Reg* r = get_RegVec(ra->one_def->ras, 0);
      if (r->kind == REG_FIXED) {
        // TODO: Remove this after implementation of split in reg_alloc
        continue;
      }
      release_Reg(get_RegVec(inst->ras, i));
      set_RegVec(inst->ras, i, copy_Reg(r));
    }
  }
}

static void propagation_function(Function* f) {
  for (IRInstListIterator* it = back_IRInstList(f->instructions); !is_nil_IRInstListIterator(it);
       it                     = prev_IRInstListIterator(it)) {
    IRInst* inst = data_IRInstListIterator(it);
    perform_propagation(f, inst);
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
