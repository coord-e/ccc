#ifndef CCC_OPS_H
#define CCC_OPS_H

#include <stdio.h>

typedef enum {
  BINOP_ADD,
  BINOP_SUB,
  BINOP_MUL,
  BINOP_DIV,
  BINOP_EQ,
  BINOP_NE,
  BINOP_GT,
  BINOP_GE,
  BINOP_LT,
  BINOP_LE,
} BinopKind;

void print_binop(FILE*, BinopKind);

typedef enum {
  UNAOP_ADDR,
  UNAOP_DEREF,
} UnaopKind;

void print_unaop(FILE*, UnaopKind);

#endif
