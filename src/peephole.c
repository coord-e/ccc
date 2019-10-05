#include "peephole.h"

static void modify_inst(IRInst* inst) {
  switch (inst->kind) {
    case IR_BIN_IMM:
      switch (inst->binary_op) {
        case ARITH_ADD:
          if (inst->imm == 0) {
            inst->kind = IR_MOV;
          }
          break;
        case ARITH_MUL:
          if (inst->imm == 1) {
            inst->kind = IR_MOV;
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
  for (IRInstListIterator* it = front_IRInstList(ir->instructions); !is_nil_IRInstListIterator(it);
       it                     = next_IRInstListIterator(it)) {
    IRInst* inst = data_IRInstListIterator(it);
    modify_inst(inst);
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
