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
  IR_STORE,
  IR_LOAD,
  IR_SUBS,
} IRInstKind;

typedef enum {
  REG_VIRT,
  REG_REAL,
} RegKind;

typedef struct {
  RegKind kind;
  unsigned virtual;
  unsigned real;
} Reg;

typedef struct IRInst {
  IRInstKind kind;
  BinopKind binop;    // for ND_BIN
  int imm;            // for ND_IMM
  unsigned stack_idx; // for ND_STORE, ND_LOAD

  Reg rd; // destination register
  Reg ra; // argument register
} IRInst;

IRInst* new_inst(IRInstKind);
void release_inst(IRInst*);

DECLARE_LIST(IRInst*, IRInstList)

// IR is a list of `IRInst` ... with some metadata
typedef struct {
  IRInstList* insts;
  unsigned reg_count;
} IR;

// build IR from ast
IR* generate_IR(Node* ast);

// free the memory space used in IR
void release_IR(IR*);

// print for debugging purpose
void print_IR(FILE*, IR*);

#endif
