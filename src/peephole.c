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

static void modify_inst(IRInstList* list, IRInstListIterator* it) {
  IRInst* inst = data_IRInstListIterator(it);
  switch (inst->kind) {
    case IR_BIN_IMM:
      switch (inst->binary_op) {
        case ARITH_ADD:
          if (inst->imm == 0) {
            disable_inst(list, it, inst);
          }
          break;
        case ARITH_MUL:
          if (inst->imm == 1) {
            disable_inst(list, it, inst);
          }
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
