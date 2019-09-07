#ifndef CCC_IR_H
#define CCC_IR_H

#include <stdio.h>

#include "ast.h"
#include "bit_set.h"
#include "double_list.h"
#include "lexer.h"
#include "list.h"
#include "ops.h"
#include "type.h"
#include "vector.h"

typedef enum {
  IR_BIN,
  IR_UNA,
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
  IR_NOP,
} IRInstKind;

typedef enum {
  REG_VIRT,
  REG_REAL,
  REG_FIXED,
} RegKind;

typedef struct {
  RegKind kind;
  unsigned virtual;
  unsigned real;
  DataSize size;
  bool irreplaceable;
} Reg;

Reg* new_Reg(RegKind, DataSize);
Reg* new_virtual_Reg(DataSize, unsigned virtual);
Reg* new_real_Reg(DataSize, unsigned real);
Reg* new_fixed_Reg(DataSize, unsigned virtual, unsigned real);
Reg* copy_Reg(Reg*);
void release_Reg(Reg*);

DECLARE_VECTOR(Reg*, RegVec)
DECLARE_VECTOR_PRINTER(RegVec)

typedef struct BasicBlock BasicBlock;

typedef enum {
  GN_FUNCTION,
  GN_DATA,
} GlobalNameKind;

typedef struct IRInst {
  IRInstKind kind;

  unsigned local_id;   // unique in `Function`
  unsigned global_id;  // unique in `IR`

  BinaryOp binop;         // for IR_BIN
  UnaryOp unaop;          // for IR_UNA
  int imm;                // for IR_IMM
  unsigned stack_idx;     // for IR_STACK_*
  unsigned argument_idx;  // for IR_ARG
  DataSize data_size;     // for IR_{LOAD, STORE, STACK_LOAD, STACK_STORE}

  char* global_name;           // for IR_GLOBAL, owned
  GlobalNameKind global_kind;  // for IR_GLOBAL

  BasicBlock* label;  // for IR_LABEL, not owned

  BasicBlock* jump;  // for IR_JUMP, not owned

  BasicBlock* then_;  // for IR_BR, not owned
  BasicBlock* else_;  // for IR_BR, not owned

  bool is_vararg;  // for IR_CALL

  Reg* rd;      // destination register (null if unused)
  RegVec* ras;  // argument registers (won't be null)
} IRInst;

IRInst* new_inst(unsigned local_id, unsigned global_id, IRInstKind);
void release_inst(IRInst*);

DECLARE_DLIST(IRInst*, IRInstList)
DECLARE_DLIST(BasicBlock*, BBList)
DECLARE_DLIST(BasicBlock*, BBRefList)
DECLARE_VECTOR(IRInst*, IRInstVec)

// `BasicBlock` forms a control flow graph
struct BasicBlock {
  unsigned local_id;   // unique in `Function`
  unsigned global_id;  // unique in `IR`

  IRInstList* insts;  // owned

  BBRefList* succs;  // not owned (owned by `IR`)
  BBRefList* preds;  // not owned (owned by `IR`)

  // "call bb" is a bb with one call inst
  bool is_call_bb;

  // will filled in `data_flow`
  BitSet* live_in;     // owned, NULL before analysis
  BitSet* live_out;    // owned, ditto
  BitSet* live_gen;    // owned, ditto
  BitSet* live_kill;   // owned, ditto
  BitSet* reach_in;    // owned, ditto
  BitSet* reach_out;   // owned, ditto
  BitSet* reach_gen;   // owned, ditto
  BitSet* reach_kill;  // owned, ditto

  // will filled in `reorder`
  // sorted in normal order
  // NULL if there is no usable instance (e.g. before `reorder` or after `reg_alloc`)
  IRInstVec* sorted_insts;  // not owned

  // will filled in `reg_alloc`
  // available if `is_call_bb`` is true
  // a set of physical registers that lives through this bb
  BitSet* should_preserve;  // owned
};

typedef struct Function Function;

void detach_BasicBlock(Function*, BasicBlock*);
void connect_BasicBlock(BasicBlock* from, BasicBlock* to);
void release_BasicBlock(BasicBlock*);

typedef enum {
  IV_UNSET,
  IV_VIRTUAL,
  IV_FIXED,
} IntervalKind;

typedef struct {
  IntervalKind kind;

  // -1 for undefined
  // TODO: stop using -1 to indicate undefined value
  unsigned from;
  unsigned to;

  unsigned fixed_real;  // for IV_FIXED
} Interval;

DECLARE_VECTOR(Interval*, RegIntervals)
void print_Intervals(FILE*, RegIntervals*);

DECLARE_VECTOR(BasicBlock*, BBVec)
DECLARE_VECTOR(BitSet*, BSVec)

struct Function {
  char* name;  // owned

  BBList* blocks;  // owned

  unsigned bb_count;
  unsigned reg_count;
  unsigned stack_count;
  unsigned inst_count;

  BasicBlock* entry;  // not owned
  BasicBlock* exit;   // not owend

  unsigned call_count;

  // will filled in `arch`
  BitSet* used_fixed_regs;  // owned

  // will filled in `reorder`
  // sorted in reverse order
  BBVec* sorted_blocks;  // not owned
  // sorted in normal order
  IRInstVec* sorted_insts;  // not owned

  // will filled in `data_flow`
  BSVec* definitions;  // owned, virtual -> inst local id

  // will filled in `liveness`
  RegIntervals* intervals;  // owned

  // will filled in `reg_alloc`
  BitSet* used_regs;  // owned
};

DECLARE_LIST(Function*, FunctionList)

typedef enum {
  GE_ADD,
  GE_SUB,
  GE_NAME,
  GE_NUM,
  GE_STRING,
} GlobalExprKind;

// represent the constant data in asm
typedef struct {
  GlobalExprKind kind;

  char* lhs;  // for GE_ADD, GE_SUB, owned
  long rhs;   // ditto

  char* name;  // for GE_NAME, owned

  long num;       // for GE_NUM
  DataSize size;  // for GE_NUM

  char* string;  // for GE_STRING
} GlobalExpr;

DECLARE_LIST(GlobalExpr*, GlobalInitializer)

typedef struct {
  char* name;  // owned
  GlobalInitializer* init;
} GlobalVar;

DECLARE_VECTOR(GlobalVar*, GlobalVarVec)

typedef struct {
  unsigned inst_count;
  unsigned bb_count;

  FunctionList* functions;  // owned
  GlobalVarVec* globals;    // owned
} IR;

// build IR from ast
IR* generate_IR(AST* ast);

// free the memory space used in IR
void release_IR(IR*);

// print for debugging purpose
void print_IR(FILE*, IR*);

#endif
