#ifndef CCC_OPS_H
#define CCC_OPS_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
  OP_ARITH,
  OP_COMPARE,
} BinaryOpKind;

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
} BinaryOp;

void print_BinaryOp(FILE*, BinaryOp);
long eval_BinaryOp(BinaryOp, long, long);
BinaryOpKind kind_of_BinaryOp(BinaryOp);

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
  ARITH_SHIFT_LEFT  = BINOP_SHIFT_LEFT,
} ArithOp;

void print_ArithOp(FILE*, ArithOp);
long eval_ArithOp(ArithOp, long, long);
ArithOp as_ArithOp(BinaryOp);

typedef enum {
  CMP_EQ = BINOP_EQ,
  CMP_NE = BINOP_NE,
  CMP_GT = BINOP_GT,
  CMP_GE = BINOP_GE,
  CMP_LT = BINOP_LT,
  CMP_LE = BINOP_LE,
} CompareOp;

void print_CompareOp(FILE*, CompareOp);
bool eval_CompareOp(CompareOp, long, long);
CompareOp as_CompareOp(BinaryOp);

typedef enum {
  UNAOP_POSITIVE,
  UNAOP_INTEGER_NEG,
  UNAOP_BITWISE_NEG,
} UnaryOp;

void print_UnaryOp(FILE*, UnaryOp);
long eval_UnaryOp(UnaryOp, long);

#endif
