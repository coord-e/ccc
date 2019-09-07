#include <assert.h>

#include "error.h"
#include "ops.h"

void print_BinaryOp(FILE* p, BinaryOp op) {
  switch (kind_of_BinaryOp(op)) {
    case BINOP_ARITH:
      print_ArithOp(p, as_ArithOp(op));
      return;
    case BINOP_COMPARE:
      print_CompareOp(p, as_CompareOp(op));
      return;
    default:
      CCC_UNREACHABLE;
  }
}

long eval_BinaryOp(BinaryOp op, long lhs, long rhs) {
  switch (kind_of_BinaryOp(op)) {
    case BINOP_ARITH:
      return eval_ArithOp(as_ArithOp(op), lhs, rhs);
    case BINOP_COMPARE:
      return eval_CompareOp(as_CompareOp(op), lhs, rhs);
    default:
      CCC_UNREACHABLE;
  }
}

BinaryOpKind kind_of_BinaryOp(BinaryOp op) {
  switch (op) {
    case BINOP_ADD:
    case BINOP_SUB:
    case BINOP_MUL:
    case BINOP_DIV:
    case BINOP_REM:
    case BINOP_OR:
    case BINOP_XOR:
    case BINOP_AND:
    case BINOP_SHIFT_RIGHT:
    case BINOP_SHIFT_LEFT:
      return BINOP_ARITH;
    case BINOP_EQ:
    case BINOP_NE:
    case BINOP_GT:
    case BINOP_GE:
    case BINOP_LT:
    case BINOP_LE:
      return BINOP_COMPARE;
    default:
      CCC_UNREACHABLE;
  }
}

void print_ArithOp(FILE* p, ArithOp op) {
  switch (op) {
    case ARITH_ADD:
      fprintf(p, "+");
      return;
    case ARITH_SUB:
      fprintf(p, "-");
      return;
    case ARITH_MUL:
      fprintf(p, "*");
      return;
    case ARITH_DIV:
      fprintf(p, "/");
      return;
    case ARITH_REM:
      fprintf(p, "%%");
      return;
    case ARITH_OR:
      fprintf(p, "|");
      return;
    case ARITH_XOR:
      fprintf(p, "^");
      return;
    case ARITH_AND:
      fprintf(p, "&");
      return;
    case ARITH_SHIFT_RIGHT:
      fprintf(p, ">>");
      return;
    case ARITH_SHIFT_LEFT:
      fprintf(p, "<<");
      return;
    default:
      CCC_UNREACHABLE;
  }
}

long eval_ArithOp(ArithOp op, long lhs, long rhs) {
  switch (op) {
    case ARITH_ADD:
      return lhs + rhs;
    case ARITH_SUB:
      return lhs - rhs;
    case ARITH_MUL:
      return lhs * rhs;
    case ARITH_DIV:
      return lhs / rhs;
    case ARITH_REM:
      return lhs % rhs;
    case ARITH_OR:
      return lhs | rhs;
    case ARITH_XOR:
      return lhs ^ rhs;
    case ARITH_AND:
      return lhs & rhs;
    case ARITH_SHIFT_RIGHT:
      return lhs >> rhs;
    case ARITH_SHIFT_LEFT:
      return lhs << rhs;
    default:
      CCC_UNREACHABLE;
  }
}

ArithOp as_ArithOp(BinaryOp op) {
  assert(kind_of_BinaryOp(op) == BINOP_ARITH);
  return (ArithOp)op;
}

void print_CompareOp(FILE* p, CompareOp op) {
  switch (op) {
    case CMP_EQ:
      fprintf(p, "==");
      return;
    case CMP_NE:
      fprintf(p, "!=");
      return;
    case CMP_GT:
      fprintf(p, ">");
      return;
    case CMP_GE:
      fprintf(p, ">=");
      return;
    case CMP_LT:
      fprintf(p, "<");
      return;
    case CMP_LE:
      fprintf(p, "<=");
      return;
    default:
      CCC_UNREACHABLE;
  }
}

bool eval_CompareOp(CompareOp op, long lhs, long rhs) {
  switch (op) {
    case CMP_EQ:
      return lhs == rhs;
    case CMP_NE:
      return lhs != rhs;
    case CMP_GT:
      return lhs > rhs;
    case CMP_GE:
      return lhs >= rhs;
    case CMP_LT:
      return lhs < rhs;
    case CMP_LE:
      return lhs <= rhs;
    default:
      CCC_UNREACHABLE;
  }
}

CompareOp as_CompareOp(BinaryOp op) {
  assert(kind_of_BinaryOp(op) == BINOP_COMPARE);
  return (CompareOp)op;
}

void print_UnaryOp(FILE* p, UnaryOp kind) {
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

long eval_UnaryOp(UnaryOp kind, long opr) {
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
