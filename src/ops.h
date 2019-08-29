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
  UNAOP_POSITIVE,
  UNAOP_INTEGER_NEG,
  UNAOP_BITWISE_NEG,
} UnaopKind;

void print_unaop(FILE*, UnaopKind);
long eval_unaop(UnaopKind, long);

#endif
