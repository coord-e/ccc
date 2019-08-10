#ifndef CCC_IR_H
#define CCC_IR_H

#include <stdio.h>

#include "ast.h"
#include "bit_set.h"
#include "lexer.h"
#include "list.h"
#include "ops.h"
#include "vector.h"

typedef enum {
  SIZE_BYTE  = 1,  // 1 byte, 8  bits
  SIZE_WORD  = 2,  // 2 byte, 16 bits
  SIZE_DWORD = 4,  // 4 byte, 32 bits
  SIZE_QWORD = 8,  // 8 byte, 64 bits
} DataSize;

DataSize to_data_size(unsigned);

typedef enum {
  IR_BIN,
  IR_IMM,
  IR_ARG,
  IR_RET,
  IR_STACK_ADDR,
  IR_STACK_STORE,
  IR_STACK_LOAD,
  IR_STORE,
  IR_LOAD,
  IR_MOV,
  IR_BR,    // conditional branch
  IR_JUMP,  // unconditional branch
  IR_LABEL,
  IR_CALL,
  IR_SEXT,
  IR_TRUNC,
  IR_GLOBAL_ADDR,
} IRInstKind;

typedef enum {
  REG_VIRT,
  REG_REAL,
} RegKind;

typedef struct {
  RegKind kind;
  unsigned virtual;
  unsigned real;
  DataSize size;

  bool is_used;  // TODO: Use better way to represent unsued register slot
} Reg;

DECLARE_VECTOR(Reg, RegVec)
DECLARE_VECTOR_PRINTER(RegVec)

typedef struct BasicBlock BasicBlock;

typedef struct IRInst {
  IRInstKind kind;

  unsigned local_id;   // unique in `Function`
  unsigned global_id;  // unique in `IR`

  BinopKind binop;        // for IR_BIN
  int imm;                // for IR_IMM
  unsigned stack_idx;     // for IR_STACK_*
  unsigned argument_idx;  // for IR_ARG
  DataSize data_size;     // for IR_{LOAD, STORE, STACK_LOAD, STACK_STORE}

  char* global_name;  // for IR_GLOBAL, owned

  BasicBlock* label;  // for IR_LABEL, not owned

  BasicBlock* jump;  // for IR_JUMP, not owned

  BasicBlock* then_;  // for IR_BR, not owned
  BasicBlock* else_;  // for IR_BR, not owned

  Reg rd;       // destination register
  RegVec* ras;  // argument registers (won't be null)
} IRInst;

IRInst* new_inst(unsigned local_id, unsigned global_id, IRInstKind);
void release_inst(IRInst*);

DECLARE_LIST(IRInst*, IRInstList)
DECLARE_LIST(BasicBlock*, BBList)
DECLARE_VECTOR(IRInst*, IRInstVec)

// `BasicBlock` forms a control flow graph
struct BasicBlock {
  unsigned local_id;   // unique in `Function`
  unsigned global_id;  // unique in `IR`

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

  // will filled in `reg_alloc`
  BitSet* used_regs;  // owned
  unsigned real_reg_count;
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
