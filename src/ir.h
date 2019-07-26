#ifndef CCC_IR_H
#define CCC_IR_H

#include <stdio.h>

#include "ast.h"
#include "binop.h"
#include "lexer.h"
#include "list.h"
#include "vector.h"

typedef enum {
  IR_BIN,
  IR_IMM,
  IR_RET,
  IR_STORE,
  IR_LOAD,
  IR_SUBS,
  IR_MOV,
  IR_BR,    // conditional branch
  IR_JUMP,  // unconditional branch
} IRInstKind;

typedef enum {
  REG_VIRT,
  REG_REAL,
} RegKind;

typedef struct {
  RegKind kind;
  unsigned virtual;
  unsigned real;

  bool is_used;  // TODO: Use better way to represent unsued register slot
  bool is_spilled;
} Reg;

DECLARE_VECTOR(Reg, RegVec)
DECLARE_VECTOR_PRINTER(RegVec)

typedef struct BasicBlock BasicBlock;

typedef struct IRInst {
  IRInstKind kind;
  BinopKind binop;     // for ND_BIN
  int imm;             // for ND_IMM
  unsigned stack_idx;  // for ND_STORE, ND_LOAD

  BasicBlock* jump;  // for IR_JUMP, not owned

  BasicBlock* then_;  // for IR_BR, not owned
  BasicBlock* else_;  // for IR_BR, not owned

  Reg rd;       // destination register
  RegVec* ras;  // argument registers (won't be null)
} IRInst;

IRInst* new_inst(IRInstKind);
void release_inst(IRInst*);

DECLARE_LIST(IRInst*, IRInstList)
DECLARE_LIST(BasicBlock*, BBList)

// `BasicBlock` forms a control flow graph
struct BasicBlock {
  unsigned id;
  IRInstList* insts;

  BBList* succs;
  BBList* preds;

  bool released;  // used in destruction
};

typedef struct {
  BasicBlock* entry;
  BasicBlock* exit;

  unsigned reg_count;
  unsigned stack_count;
} IR;

// build IR from ast
IR* generate_IR(AST* ast);

// free the memory space used in IR
void release_IR(IR*);

// print for debugging purpose
void print_IR(FILE*, IR*);

#endif
