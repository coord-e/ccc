#ifndef CCC_IR_H
#define CCC_IR_H

#include <stdio.h>

#include "ast.h"
#include "binop.h"
#include "bit_set.h"
#include "lexer.h"
#include "list.h"
#include "vector.h"

typedef enum {
  IR_BIN,
  IR_IMM,
  IR_ARG,
  IR_RET,
  IR_STORE,
  IR_LOAD,
  IR_SUBS,
  IR_MOV,
  IR_BR,    // conditional branch
  IR_JUMP,  // unconditional branch
  IR_LABEL,
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
  unsigned id;  // just for idenfitication

  BinopKind binop;        // for IR_BIN
  int imm;                // for IR_IMM
  unsigned stack_idx;     // for IR_STORE, IR_LOAD
  unsigned argument_idx;  // for IR_ARG

  BasicBlock* label;  // for IR_LABEL, not owned

  BasicBlock* jump;  // for IR_JUMP, not owned

  BasicBlock* then_;  // for IR_BR, not owned
  BasicBlock* else_;  // for IR_BR, not owned

  Reg rd;       // destination register
  RegVec* ras;  // argument registers (won't be null)
} IRInst;

IRInst* new_inst(unsigned id, IRInstKind);
void release_inst(IRInst*);

DECLARE_LIST(IRInst*, IRInstList)
DECLARE_LIST(BasicBlock*, BBList)
DECLARE_VECTOR(IRInst*, IRInstVec)

// `BasicBlock` forms a control flow graph
struct BasicBlock {
  unsigned id;        // just for identification
  IRInstList* insts;  // owned

  BBList* succs;  // not owned (owned by `IR`)
  BBList* preds;  // not owned (owned by `IR`)

  // it costs too much to remove all references from `preds` and `succs`
  // and remove from `blocks`,
  // so mark as dead with this field instead of removing it
  bool dead;

  // will filled in `liveness`
  BitSet* live_gen;   // owned, NULL before analysis
  BitSet* live_kill;  // owned, ditto
  BitSet* live_in;    // owned, ditto
  BitSet* live_out;   // owned, ditto

  // will filled in `reorder`
  // sorted in normal order
  // NULL if there is no usable instance (e.g. before `reorder` or after `reg_alloc`)
  IRInstVec* sorted_insts;  // not owned
};

DECLARE_VECTOR(BasicBlock*, BBVec)

// forward decralation; will declared in `liveness.h`
typedef struct RegIntervals RegIntervals;

typedef struct {
  char* name;  // owned

  BBList* blocks;  // owned

  unsigned bb_count;
  unsigned reg_count;
  unsigned stack_count;
  unsigned inst_count;

  BasicBlock* entry;  // not owned
  BasicBlock* exit;   // not owend

  // will filled in `reorder`
  // sorted in reverse order
  BBVec* sorted_blocks;  // not owned

  // will filled in `liveness`
  RegIntervals* intervals;  // owned
} Function;

DECLARE_LIST(Function*, FunctionList)

typedef struct {
  unsigned inst_count;
  unsigned bb_count;

  FunctionList* functions;  // owned
} IR;

// build IR from ast
IR* generate_IR(AST* ast);

// free the memory space used in IR
void release_IR(IR*);

// print for debugging purpose
void print_IR(FILE*, IR*);

#endif
