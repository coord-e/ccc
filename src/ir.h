#ifndef CCC_IR_H
#define CCC_IR_H

#include <stdio.h>

#include "lexer.h"
#include "list.h"
#include "parser.h"
#include "binop.h"

typedef enum {
  IR_BIN,
  IR_IMM,
  IR_RET,
} IRInstKind;

typedef struct Reg {
  int virtual;
  int real;
} Reg;

typedef struct IRInst {
  IRInstKind kind;
  BinopKind binop;    // for ND_BIN
  int imm;            // for ND_IMM

  Reg rd; // destination register
  Reg ra; // argument register
} IRInst;

DECLARE_LIST(IRInst, IR)
DECLARE_LIST_PRINTER(IR)

// build IR from ast
IR* generate_ir(Node* ast);

#endif
