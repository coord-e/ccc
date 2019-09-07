#ifndef CCC_OPS_H
#define CCC_OPS_H

#include <stdio.h>

typedef enum {
  BINOP_ADD,
  BINOP_SUB,
  BINOP_MUL,
  BINOP_DIV,
  BINOP_REM,
  BINOP_EQ,
  BINOP_NE,
  BINOP_GT,
  BINOP_GE,
  BINOP_LT,
  BINOP_LE,
  BINOP_OR,
  BINOP_XOR,
  BINOP_AND,
  BINOP_SHIFT_RIGHT,
  BINOP_SHIFT_LEFT,
} BinopKind;

void print_binop(FILE*, BinopKind);
long eval_binop(BinopKind, long, long);

typedef enum {
  ARITH_ADD         = BINOP_ADD,
  ARITH_SUB         = BINOP_SUB,
  ARITH_MUL         = BINOP_MUL,
  ARITH_DIV         = BINOP_DIV,
  ARITH_REM         = BINOP_REM,
  ARITH_OR          = BINOP_OR,
  ARITH_XOR         = BINOP_XOR,
  ARITH_AND         = BINOP_AND,
  ARITH_SHIFT_RIGHT = BINOP_SHIFT_RIGHT,
  ARITH_SHIFT_LEFT  = BINOP_SHIFT_RIGHT,
} ArithOp;

typedef enum {
  CMP_EQ = BINOP_EQ,
  CMP_NE = BINOP_NE,
  CMP_GT = BINOP_GT,
  CMP_GE = BINOP_GE,
  CMP_LT = BINOP_LT,
  CMP_LE = BINOP_LE,
} CompareOp;

typedef enum {
  UNAOP_POSITIVE,
  UNAOP_INTEGER_NEG,
  UNAOP_BITWISE_NEG,
} UnaopKind;

void print_unaop(FILE*, UnaopKind);
long eval_unaop(UnaopKind, long);

#endif
