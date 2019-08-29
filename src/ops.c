#include "ops.h"
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
    case BINOP_REM:
      fprintf(p, "%%");
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
    case BINOP_OR:
      fprintf(p, "|");
      return;
    case BINOP_XOR:
      fprintf(p, "^");
      return;
    case BINOP_AND:
      fprintf(p, "&");
      return;
    case BINOP_SHIFT_RIGHT:
      fprintf(p, ">>");
      return;
    case BINOP_SHIFT_LEFT:
      fprintf(p, "<<");
      return;
    default:
      CCC_UNREACHABLE;
  }
}

long eval_binop(BinopKind kind, long lhs, long rhs) {
  switch (kind) {
    case BINOP_ADD:
      return lhs + rhs;
    case BINOP_SUB:
      return lhs - rhs;
    case BINOP_MUL:
      return lhs * rhs;
    case BINOP_DIV:
      return lhs / rhs;
    case BINOP_REM:
      return lhs % rhs;
    case BINOP_EQ:
      return lhs == rhs;
    case BINOP_NE:
      return lhs != rhs;
    case BINOP_GT:
      return lhs > rhs;
    case BINOP_GE:
      return lhs >= rhs;
    case BINOP_LT:
      return lhs < rhs;
    case BINOP_LE:
      return lhs <= rhs;
    case BINOP_OR:
      return lhs | rhs;
    case BINOP_XOR:
      return lhs ^ rhs;
    case BINOP_AND:
      return lhs & rhs;
    case BINOP_SHIFT_RIGHT:
      return lhs >> rhs;
    case BINOP_SHIFT_LEFT:
      return lhs << rhs;
    default:
      CCC_UNREACHABLE;
  }
}

void print_unaop(FILE* p, UnaopKind kind) {
  switch (kind) {
    case UNAOP_POSITIVE:
      fprintf(p, "+");
      return;
    case UNAOP_INTEGER_NEG:
      fprintf(p, "-");
      return;
    case UNAOP_BITWISE_NEG:
      fprintf(p, "~");
      return;
    default:
      CCC_UNREACHABLE;
  }
}

long eval_unaop(UnaopKind kind, long opr) {
  switch (kind) {
    case UNAOP_POSITIVE:
      return +opr;
    case UNAOP_INTEGER_NEG:
      return -opr;
    case UNAOP_BITWISE_NEG:
      return ~opr;
    default:
      CCC_UNREACHABLE;
  }
}
