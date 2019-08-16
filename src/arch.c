#include "arch.h"

// clang-format off
const char* regs8[]  = {"dil", "sil", "dl",  "cl",  "r8b", "r9b", "al",  "r12b", "r13b", "r14b", "r15b", "bl"};
const char* regs16[] = {"di",  "si",  "dx",  "cx",  "r8w", "r9w", "ax",  "r12w", "r13w", "r14w", "r15w", "bx"};
const char* regs32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d", "eax", "r12d", "r13d", "r14d", "r15d", "ebx"};
const char* regs64[] = {"rdi", "rsi", "rdx", "rcx", "r8",  "r9",  "rax", "r12",  "r13",  "r14",  "r15",  "rbx"};
// clang-format on

// first 6 registers are used to pass arguments to a function
const unsigned max_args   = 6;
const size_t num_regs     = sizeof(regs64) / sizeof(*regs64);
const unsigned rax_reg_id = 6;

unsigned nth_arg_id(unsigned n) {
  if (n >= max_args) {
    error("unsupported number of arguments/parameters. (%d)", n);
  }

  return n;
}

typedef struct {
  unsigned global_inst_count;
  unsigned inst_count;
} Env;

static Env* init_Env(unsigned global_inst_count, Function* f) {
  Env* env               = calloc(1, sizeof(Env));
  env->global_inst_count = global_inst_count;
  env->inst_count        = f->inst_count;
  return env;
}

static void finish_Env(Env* env, unsigned* inst_count, Function* f) {
  *inst_count   = env->global_inst_count;
  f->inst_count = env->inst_count;
  free(env);
}

static Reg new_real_reg(unsigned id, DataSize size) {
  Reg r = {.kind = REG_REAL, .virtual = 0, .real = id, .size = size, .is_used = true};
  return r;
}

static Reg rax_reg(DataSize size) {
  return new_real_reg(rax_reg_id, size);
}

static Reg nth_arg_reg(unsigned n, DataSize size) {
  return new_real_reg(nth_arg_id(n), size);
}

static IRInst* new_move(Env* env, Reg rd, Reg ra) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_MOV);
  inst->rd     = rd;
  push_RegVec(inst->ras, ra);
  return inst;
}

static IRInst* new_ret(Env* env, Reg ra) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_RET);
  push_RegVec(inst->ras, ra);
  return inst;
}

static IRInst* new_binop(Env* env, BinopKind kind, Reg rd, Reg lhs, Reg rhs) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_BIN);
  inst->binop  = kind;
  inst->rd     = rd;
  push_RegVec(inst->ras, lhs);
  push_RegVec(inst->ras, rhs);
  return inst;
}

static IRInst* new_unaop(Env* env, UnaopKind kind, Reg rd, Reg opr) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_UNA);
  inst->unaop  = kind;
  inst->rd     = rd;
  push_RegVec(inst->ras, opr);
  return inst;
}

static IRInst* new_call(Env* env, Reg rd, Reg rf) {
  IRInst* inst = new_inst(env->inst_count++, env->global_inst_count++, IR_CALL);
  inst->rd     = rd;
  push_RegVec(inst->ras, rf);
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
      Reg rd     = inst->rd;
      Reg lhs    = get_RegVec(inst->ras, 0);
      Reg rhs    = get_RegVec(inst->ras, 1);
      IRInst* i1 = new_move(env, rd, lhs);
      IRInst* i2 = new_binop(env, inst->binop, rd, rd, rhs);

      remove_IRInstList(l);
      insert_IRInstList(i2, l);
      insert_IRInstList(i1, l);
      tail = tail_IRInstList(tail_IRInstList(l));
      break;
    }
    case IR_UNA: {
      Reg rd     = inst->rd;
      Reg opr    = get_RegVec(inst->ras, 0);
      IRInst* i1 = new_move(env, rd, opr);
      IRInst* i2 = new_unaop(env, inst->unaop, rd, rd);

      remove_IRInstList(l);
      insert_IRInstList(i2, l);
      insert_IRInstList(i1, l);
      tail = tail_IRInstList(tail_IRInstList(l));
      break;
    }
    case IR_CALL: {
      Reg rd = inst->rd;
      Reg rf = get_RegVec(inst->ras, 0);

      remove_IRInstList(l);
      insert_IRInstList(new_move(env, rd, rax_reg(rd.size)), l);
      IRInst* call = new_call(env, rax_reg(rd.size), rf);
      insert_IRInstList(call, l);
      tail = tail_IRInstList(tail_IRInstList(l));
      for (unsigned i = 1; i < length_RegVec(inst->ras); i++) {
        Reg r = get_RegVec(inst->ras, i);
        Reg p = nth_arg_reg(i - 1, r.size);
        push_RegVec(call->ras, p);
        insert_IRInstList(new_move(env, p, r), l);
        tail = tail_IRInstList(tail);
      }
      break;
    }
    case IR_ARG: {
      Reg rd       = inst->rd;
      unsigned idx = inst->argument_idx;

      remove_IRInstList(l);
      insert_IRInstList(new_move(env, rd, nth_arg_reg(idx, rd.size)), l);
      tail = tail_IRInstList(l);
      break;
    }
    case IR_RET:
      if (length_RegVec(inst->ras) == 0) {
        break;
      }
      assert(length_RegVec(inst->ras) == 1);
      Reg ra = get_RegVec(inst->ras, 0);

      remove_IRInstList(l);
      insert_IRInstList(new_ret(env, rax_reg(ra.size)), l);
      insert_IRInstList(new_move(env, rax_reg(ra.size), ra), l);
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
