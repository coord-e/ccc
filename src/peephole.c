#include "peephole.h"

static void disable_inst(IRInstList* list, IRInstListIterator* it, IRInst* inst) {
  assert(inst->rd->kind != REG_REAL);
  assert(get_RegVec(inst->ras, 0)->kind != REG_REAL);
  if (inst->rd->virtual == get_RegVec(inst->ras, 0)->virtual) {
    remove_IRInstListIterator(list, it);
  } else {
    inst->kind = IR_MOV;
  }
}

static bool log2(unsigned long in, unsigned long* out) {
  if ((in & (in - 1)) != 0) {
    return false;
  }

  unsigned long x = 0;
  while (in >>= 1)
    x++;

  *out = x;
  return true;
}

static void modify_inst(IRInstList* list, IRInstListIterator* it) {
  IRInst* inst = data_IRInstListIterator(it);
  switch (inst->kind) {
    case IR_BIN_IMM:
      switch (inst->binary_op) {
        case ARITH_ADD:
        case ARITH_SUB:
          if (inst->imm == 0) {
            disable_inst(list, it, inst);
          }
          break;
        case ARITH_MUL:
          switch (inst->imm) {
            case 1:
              disable_inst(list, it, inst);
              break;
            case 0:
              inst->kind = IR_IMM;
              resize_RegVec(inst->ras, 0);
              break;
            default: {
              unsigned long c;
              if (log2(inst->imm, &c)) {
                inst->binary_op = ARITH_SHIFT_LEFT;
                inst->imm       = c;
              }
              break;
            }
          }
          break;
        case ARITH_DIV:
          switch (inst->imm) {
            case 1:
              disable_inst(list, it, inst);
              break;
            case 0:
              error("zero-division detected");
              break;
            default: {
              unsigned long c;
              if (log2(inst->imm, &c)) {
                inst->binary_op = ARITH_SHIFT_RIGHT;
                inst->imm       = c;
              }
              break;
            }
          }
          break;
        default:
          break;
      }
      break;
    case IR_BR_CMP_IMM:
      if (inst->imm != 0) {
        break;
      }
      switch (inst->predicate_op) {
        case CMP_NE:
          inst->kind = IR_BR;
          break;
        case CMP_EQ:
          inst->kind      = IR_BR;
          BasicBlock* tmp = inst->then_;
          inst->then_     = inst->else_;
          inst->else_     = tmp;
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

static void peephole_function(Function* ir) {
  IRInstListIterator* it = front_IRInstList(ir->instructions);
  while (!is_nil_IRInstListIterator(it)) {
    IRInstListIterator* next = next_IRInstListIterator(it);
    modify_inst(ir->instructions, it);
    it = next;
  }
}

void peephole(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);
    peephole_function(f);
    l = tail_FunctionList(l);
  }
}
