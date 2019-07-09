#include "ir.h"
#include "parser.h"
#include "error.h"

DEFINE_LIST(IRInst, IR)

void print_reg(FILE* p, Reg r) {
  fprintf(p, "r(%d, %d)", r.virtual, r.real);
}

void print_inst(FILE* p, IRInst i) {
  switch(i.kind) {
    case IR_IMM:
      fprintf(p, "IMM %d", i.imm);
      break;
    case IR_MOV:
      fprintf(p, "MOV ");
      print_reg(p, i.rd);
      fprintf(p, " <- ");
      print_reg(p, i.ra);
      break;
    case IR_RET:
      fprintf(p, "RET ");
      print_reg(p, i.ra);
      break;
    case IR_BIN:
      fprintf(p, "BIN ");
      print_binop(p, i.binop);
      fprintf(p, " ");
      print_reg(p, i.rd);
      fprintf(p, " <- ");
      print_reg(p, i.ra);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

DEFINE_LIST_PRINTER(print_inst, IR)
