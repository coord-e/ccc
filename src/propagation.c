#include "propagation.h"
#include "util.h"

typedef struct {
  Function* f;
  IR* ir;
} Env;

static Env* init_Env(IR* ir, Function* f) {
  Env* env = malloc(sizeof(Env));
  env->f   = f;
  env->ir  = ir;
  return env;
}

static void finish_Env(Env* env) {
  free(env);
}

static Reg* new_reg(Env* env, DataSize size) {
  return new_virtual_Reg(size, env->f->reg_count++);
}

static IRInst* new_move(Env* env, Reg* rd, Reg* ra) {
  IRInst* inst = new_inst(env->f->inst_count++, env->ir->inst_count++, IR_MOV);
  inst->rd     = copy_Reg(rd);
  push_RegVec(inst->ras, copy_Reg(ra));
  return inst;
}

static bool get_one_def(Env* env, Reg* r, IRInst** out) {
  if (r->definitions != NULL && count_BitSet(r->definitions) == 1 && !r->sticky) {
    unsigned inst_id = mssb_BitSet(r->definitions);
    *out             = get_IRInstList(env->f->instructions, inst_id);
    return true;
  } else {
    return false;
  }
}

static bool get_imm(Env* env, Reg* r, long* out) {
  IRInst* def;
  if (get_one_def(env, r, &def)) {
    if (def->kind == IR_IMM) {
      *out = def->imm;
      return true;
    }
  }
  return false;
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

static Reg* obtain_propagated_reg(Env* env, IRInst* inst, IRInst* def, Reg* r) {
  BitSet* set = copy_BitSet(r->definitions);
  and_BitSet(set, inst->reach_in);

  if (count_BitSet(set) == 0) {
    Reg* escape_reg = new_reg(env, r->size);
    IRInst* mov     = new_move(env, escape_reg, r);

    IRInstListIterator* it = get_iterator_IRInstList(env->f->instructions, def->local_id);
    insert_IRInstListIterator(env->f->instructions, it, mov);

    return escape_reg;
  } else {
    return copy_Reg(r);
  }
  release_BitSet(set);
}

static bool copy_propagation(Env* env, IRInst* inst, IRInst* def, Reg** out) {
  Reg* r = get_RegVec(def->ras, 0);
  if (r->kind == REG_FIXED) {
    // TODO: Remove this after implementation of split in reg_alloc
    return false;
  }

  *out = obtain_propagated_reg(env, inst, def, r);
  return true;
}

static bool copy_propagation2(Env* env, IRInst* inst, IRInst* def, Reg** out0, Reg** out1) {
  Reg* r0 = get_RegVec(def->ras, 0);
  Reg* r1 = get_RegVec(def->ras, 0);
  if (r0->kind == REG_FIXED || r1->kind == REG_FIXED) {
    // TODO: Remove this after implementation of split in reg_alloc
    return false;
  }

  *out0 = obtain_propagated_reg(env, inst, def, r0);
  *out1 = obtain_propagated_reg(env, inst, def, r1);
  return true;
}

static void perform_propagation(Env* env, IRInst* inst) {
  switch (inst->kind) {
    case IR_MOV: {
      Reg* r = get_RegVec(inst->ras, 0);

      long imm;
      if (get_imm(env, r, &imm)) {
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
      if (get_imm(env, rhs, &rhs_imm)) {
        if (get_imm(env, lhs, &lhs_imm)) {
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
      if (get_imm(env, rhs, &rhs_imm)) {
        if (get_imm(env, lhs, &lhs_imm)) {
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
      if (get_imm(env, rhs, &rhs_imm)) {
        if (get_imm(env, lhs, &lhs_imm)) {
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
      if (get_imm(env, lhs, &lhs_imm)) {
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
      if (get_imm(env, lhs, &lhs_imm)) {
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
      if (get_imm(env, lhs, &lhs_imm)) {
        // foldable
        bool c = eval_CompareOp(inst->predicate_op, lhs_imm, inst->imm);
        elim_branch(c, inst);
      }
      break;
    }
    case IR_TRUNC: {
      Reg* opr = get_RegVec(inst->ras, 0);

      IRInst* def;
      if (!get_one_def(env, opr, &def)) {
        break;
      }
      if (def->kind != IR_ZEXT) {
        break;
      }

      Reg* rr;
      if (copy_propagation(env, inst, def, &rr)) {
        inst->kind = IR_MOV;
        release_Reg(get_RegVec(inst->ras, 0));
        set_RegVec(inst->ras, 0, rr);
      }

      break;
    }
    case IR_BR: {
      Reg* opr = get_RegVec(inst->ras, 0);

      IRInst* def;
      if (!get_one_def(env, opr, &def)) {
        break;
      }
      switch (def->kind) {
        case IR_ZEXT: {
          Reg* rr;
          if (copy_propagation(env, inst, def, &rr)) {
            release_Reg(get_RegVec(inst->ras, 0));
            set_RegVec(inst->ras, 0, rr);
          }
          break;
        }
        case IR_CMP: {
          Reg *r0, *r1;
          if (copy_propagation2(env, inst, def, &r0, &r1)) {
            release_Reg(get_RegVec(inst->ras, 0));
            set_RegVec(inst->ras, 0, r0);
            release_Reg(get_RegVec(inst->ras, 1));
            set_RegVec(inst->ras, 1, r1);

            inst->kind         = IR_BR_CMP;
            inst->predicate_op = def->predicate_op;
          }
          break;
        }
        case IR_CMP_IMM: {
          Reg* rr;
          if (copy_propagation(env, inst, def, &rr)) {
            release_Reg(get_RegVec(inst->ras, 0));
            set_RegVec(inst->ras, 0, rr);

            if (def->imm == 0 && def->predicate_op == CMP_EQ) {
              BasicBlock* tmp = inst->then_;
              inst->then_     = inst->else_;
              inst->else_     = tmp;
            } else if (def->imm == 0 && def->predicate_op == CMP_NE) {
              // nothing to change
            } else {
              inst->kind         = IR_BR_CMP_IMM;
              inst->predicate_op = def->predicate_op;
              inst->imm          = def->imm;
            }
          }
          break;
        }
        default:
          break;
      }
      break;
    }
    default:
      break;
  }

  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* ra = get_RegVec(inst->ras, i);

    IRInst* def;
    if (!get_one_def(env, ra, &def)) {
      continue;
    }
    if (def->kind != IR_MOV) {
      continue;
    }

    Reg* rr;
    if (copy_propagation(env, inst, def, &rr)) {
      release_Reg(get_RegVec(inst->ras, i));
      set_RegVec(inst->ras, i, rr);
    }
  }
}

static void propagation_function(IR* ir, Function* f) {
  for (IRInstListIterator* it = back_IRInstList(f->instructions); !is_nil_IRInstListIterator(it);
       it                     = prev_IRInstListIterator(it)) {
    IRInst* inst = data_IRInstListIterator(it);

    Env* env = init_Env(ir, f);
    perform_propagation(env, inst);
    finish_Env(env);
  }
}

void propagation(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);
    propagation_function(ir, f);
    l = tail_FunctionList(l);
  }
}
