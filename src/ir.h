#ifndef CCC_IR_H
#define CCC_IR_H

#include <stdio.h>

#include "lexer.h"
#include "binop.h"

typedef enum {
  IR_BIN,
  IR_IMM,
  IR_MOV,
  IR_RET,
} IRInstKind;

typedef struct Reg {
  int virtual;
  int real;
} Reg;

typedef union IRInst {
  IRInstKind kind;
  BinopKind binop;    // for ND_BIN
  int imm;            // for ND_IMM

  Reg rd; // destination register
  Reg ra; // argument register
} IRInst;

DECLARE_LIST(IRInst, IR)
DECLARE_LIST_PRINTER(IR)

#endif
