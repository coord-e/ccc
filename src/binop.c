#include "binop.h"
#include "error.h"

void print_binop(FILE* p, BinopKind kind) {
  switch (kind) {
    case BINOP_ADD:
      fprintf(p, "+");
      return;
    case BINOP_SUB:
      fprintf(p, "-");
      return;
    case BINOP_MUL:
      fprintf(p, "*");
      return;
    case BINOP_DIV:
      fprintf(p, "/");
      return;
    case BINOP_EQ:
      fprintf(p, "==");
      return;
    case BINOP_NE:
      fprintf(p, "!=");
      return;
    case BINOP_GT:
      fprintf(p, ">");
      return;
    case BINOP_GE:
      fprintf(p, ">=");
      return;
    case BINOP_LT:
      fprintf(p, "<");
      return;
    case BINOP_LE:
      fprintf(p, "<=");
      return;
    default:
      CCC_UNREACHABLE;
  }
}
