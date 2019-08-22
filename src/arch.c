#include "arch.h"

// clang-format off
const char* regs8[]      = {"dil", "sil", "dl",  "cl",  "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b", "al",  "bl"};
const char* regs16[]     = {"di",  "si",  "dx",  "cx",  "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w", "ax",  "bx"};
const char* regs32[]     = {"edi", "esi", "edx", "ecx", "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d", "eax", "ebx"};
const char* regs64[]     = {"rdi", "rsi", "rdx", "rcx", "r8",  "r9",  "r10",  "r11",  "r12",  "r13",  "r14",  "r15",  "rax", "rbx"};
const bool  is_scratch[] = {true,  true,  true,  true,  true,  true,  true,   true,   false,  false,  false,  false,  true,  false};
// clang-format on

// first 6 registers are used to pass arguments to a function
const unsigned max_args   = 6;
const size_t num_regs     = sizeof(regs64) / sizeof(*regs64);
const unsigned rax_reg_id = 12;
const unsigned rcx_reg_id = 3;
const unsigned rdx_reg_id = 2;

unsigned nth_arg_id(unsigned n) {
  if (n >= max_args) {
    error("unsupported number of arguments/parameters. (%d)", n);
  }

  return n;
}

typedef struct {
  unsigned global_inst_count;
  unsigned inst_count;
  unsigned reg_count;
  BitSet* used_fixed_regs;
} Env;

static Env* init_Env(unsigned global_inst_count, Function* f) {
  Env* env               = calloc(1, sizeof(Env));
  env->global_inst_count = global_inst_count;
  env->inst_count        = f->inst_count;
  env->reg_count         = f->reg_count;
  env->used_fixed_regs   = zero_BitSet(num_regs);
  return env;
}

static void finish_Env(Env* env, unsigned* inst_count, Function* f) {
  *inst_count        = env->global_inst_count;
  f->inst_count      = env->inst_count;
  f->reg_count       = env->reg_count;
  f->used_fixed_regs = env->used_fixed_regs;
  free(env);
}

static Reg* new_fixed_reg(Env* env, unsigned id, DataSize size) {
  Reg* r = new_fixed_Reg(size, env->reg_count++, id);
  set_BitSet(env->used_fixed_regs, id, true);
  return r;
}

static Reg* rax_fixed_reg(Env* env, DataSize size) {
  return new_fixed_reg(env, rax_reg_id, size);
}

static Reg* rdx_fixed_reg(Env* env, DataSize size) {
  return new_fixed_reg(env, rdx_reg_id, size);
}

static Reg* rcx_fixed_reg(Env* env, DataSize size) {
  return new_fixed_reg(env, rcx_reg_id, size);
}

static Reg* nth_arg_fixed_reg(Env* env, unsigned n, DataSize size) {
  return new_fixed_reg(env, nth_arg_id(n), size);
}

static IRInst* new_move(Env* env, Reg* rd, Reg* ra) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_MOV);
  inst->rd     = copy_Reg(rd);
  push_RegVec(inst->ras, copy_Reg(ra));
  return inst;
}

static IRInst* new_ret(Env* env, Reg* ra) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_RET);
  push_RegVec(inst->ras, copy_Reg(ra));
  return inst;
}

static IRInst* new_binop(Env* env, BinopKind kind, Reg* rd, Reg* lhs, Reg* rhs) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_BIN);
  inst->binop  = kind;
  inst->rd     = copy_Reg(rd);
  push_RegVec(inst->ras, copy_Reg(lhs));
  push_RegVec(inst->ras, copy_Reg(rhs));
  return inst;
}

static IRInst* new_unaop(Env* env, UnaopKind kind, Reg* rd, Reg* opr) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_UNA);
  inst->unaop  = kind;
  inst->rd     = copy_Reg(rd);
  push_RegVec(inst->ras, copy_Reg(opr));
  return inst;
}

static IRInst* new_call(Env* env, Reg* rd, Reg* rf, bool is_vararg) {
  IRInst* inst    = new_inst(env->inst_count++, env->global_inst_count++, IR_CALL);
  inst->rd        = copy_Reg(rd);
  inst->is_vararg = is_vararg;
  push_RegVec(inst->ras, copy_Reg(rf));
  return inst;
}

static void walk_insts(Env* env, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }

  IRInst* inst     = head_IRInstList(l);
  IRInstList* tail = tail_IRInstList(l);
  switch (inst->kind) {
    case IR_BIN: {
      Reg* rd  = inst->rd;
      Reg* lhs = get_RegVec(inst->ras, 0);
      Reg* rhs = get_RegVec(inst->ras, 1);
      switch (inst->binop) {
        case BINOP_DIV: {
          Reg* rax   = rax_fixed_reg(env, lhs->size);
          IRInst* i1 = new_move(env, rax, lhs);
          IRInst* i2 = new_binop(env, inst->binop, rax, rax, rhs);
          IRInst* i3 = new_move(env, rd, rax);
          release_Reg(rax);
          remove_IRInstList(l);
          insert_IRInstList(i3, l);
          insert_IRInstList(i2, l);
          insert_IRInstList(i1, l);
          tail = tail_IRInstList(tail_IRInstList(tail_IRInstList(l)));
          break;
        }
        case BINOP_REM: {
          Reg* rax   = rax_fixed_reg(env, lhs->size);
          Reg* rdx   = rdx_fixed_reg(env, rd->size);
          IRInst* i1 = new_move(env, rax, lhs);
          IRInst* i2 = new_binop(env, inst->binop, rdx, rax, rhs);
          IRInst* i3 = new_move(env, rd, rdx);
          release_Reg(rax);
          release_Reg(rdx);

          remove_IRInstList(l);
          insert_IRInstList(i3, l);
          insert_IRInstList(i2, l);
          insert_IRInstList(i1, l);
          tail = tail_IRInstList(tail_IRInstList(tail_IRInstList(l)));
          break;
        }
        case BINOP_SHIFT_LEFT:
        case BINOP_SHIFT_RIGHT: {
          Reg* rcx   = rcx_fixed_reg(env, rhs->size);
          IRInst* i1 = new_move(env, rcx, rhs);
          IRInst* i2 = new_move(env, rd, lhs);
          IRInst* i3 = new_binop(env, inst->binop, rd, rd, rcx);
          release_Reg(rcx);

          remove_IRInstList(l);
          insert_IRInstList(i3, l);
          insert_IRInstList(i2, l);
          insert_IRInstList(i1, l);
          tail = tail_IRInstList(tail_IRInstList(tail_IRInstList(l)));
          break;
        }
        default: {
          IRInst* i1 = new_move(env, rd, lhs);
          IRInst* i2 = new_binop(env, inst->binop, rd, rd, rhs);

          remove_IRInstList(l);
          insert_IRInstList(i2, l);
          insert_IRInstList(i1, l);
          tail = tail_IRInstList(tail_IRInstList(l));
          break;
        }
      }
      break;
    }
    case IR_UNA: {
      Reg* rd    = inst->rd;
      Reg* opr   = get_RegVec(inst->ras, 0);
      IRInst* i1 = new_move(env, rd, opr);
      IRInst* i2 = new_unaop(env, inst->unaop, rd, rd);

      remove_IRInstList(l);
      insert_IRInstList(i2, l);
      insert_IRInstList(i1, l);
      tail = tail_IRInstList(tail_IRInstList(l));
      break;
    }
    case IR_CALL: {
      Reg* rd        = inst->rd;
      Reg* rf        = get_RegVec(inst->ras, 0);
      bool is_vararg = inst->is_vararg;

      remove_IRInstList(l);
      Reg* rax = rax_fixed_reg(env, rd->size);
      insert_IRInstList(new_move(env, rd, rax), l);
      IRInst* call = new_call(env, rax, rf, is_vararg);
      release_Reg(rax);
      insert_IRInstList(call, l);
      for (unsigned i = 1; i < length_RegVec(inst->ras); i++) {
        Reg* r = get_RegVec(inst->ras, i);
        Reg* p = nth_arg_fixed_reg(env, i - 1, r->size);
        push_RegVec(call->ras, copy_Reg(p));
        insert_IRInstList(new_move(env, p, r), l);
        release_Reg(p);
      }
      // TODO: remove the redundant loop
      tail = tail_IRInstList(tail_IRInstList(l));
      for (unsigned i = 1; i < length_RegVec(inst->ras); i++) {
        tail = tail_IRInstList(tail);
      }
      break;
    }
    case IR_ARG: {
      Reg* rd      = inst->rd;
      unsigned idx = inst->argument_idx;
      Reg* ra      = nth_arg_fixed_reg(env, idx, rd->size);
      IRInst* inst = new_move(env, rd, ra);
      release_Reg(ra);

      remove_IRInstList(l);
      insert_IRInstList(inst, l);
      tail = tail_IRInstList(l);
      break;
    }
    case IR_RET:
      if (length_RegVec(inst->ras) == 0) {
        break;
      }
      assert(length_RegVec(inst->ras) == 1);
      Reg* ra    = get_RegVec(inst->ras, 0);
      Reg* rax   = rax_fixed_reg(env, ra->size);
      IRInst* i1 = new_move(env, rax, ra);
      IRInst* i2 = new_ret(env, rax);
      release_Reg(rax);

      remove_IRInstList(l);
      insert_IRInstList(i2, l);
      insert_IRInstList(i1, l);
      tail = tail_IRInstList(tail_IRInstList(l));
      break;
    default:
      break;
  }

  walk_insts(env, tail);
}

static void walk_blocks(Env* env, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }

  BasicBlock* b = head_BBList(l);
  walk_insts(env, b->insts);

  walk_blocks(env, tail_BBList(l));
}

static void transform_function(unsigned* inst_count, Function* f) {
  Env* env = init_Env(*inst_count, f);

  walk_blocks(env, f->blocks);

  finish_Env(env, inst_count, f);
}

static void walk_functions(unsigned* inst_count, FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }
  transform_function(inst_count, head_FunctionList(l));
  walk_functions(inst_count, tail_FunctionList(l));
}

void arch(IR* ir) {
  walk_functions(&ir->inst_count, ir->functions);
}
