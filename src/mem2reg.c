#include "mem2reg.h"

typedef struct {
  BitSet* candidates;
  BitSet* excluded;
  BitSet* in_stack;

  BitSet* replaceable;
} Env;

static Env* init_Env(Function* f);
static void finish_Env(Env*);
static void collect_uses(Env*, Function*);
static void apply_conversion(Env*, Function*);

void mem2reg(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);

    Env* env = init_Env(f);
    collect_uses(env, f);
    apply_conversion(env, f);
    finish_Env(env);

    l = tail_FunctionList(l);
  }
}

static Env* init_Env(Function* f) {
  unsigned reg_count = f->reg_count;

  Env* env         = malloc(sizeof(Env));
  env->candidates  = zero_BitSet(reg_count);
  env->excluded    = zero_BitSet(reg_count);
  env->in_stack    = zero_BitSet(reg_count);
  env->replaceable = zero_BitSet(reg_count);
  return env;
}

static void finish_Env(Env* env) {
  release_BitSet(env->candidates);
  release_BitSet(env->excluded);
  release_BitSet(env->in_stack);
  release_BitSet(env->replaceable);
  free(env);
}

static void set_reg(BitSet* s, Reg* r) {
  assert(r != NULL);
  assert(r->kind == REG_VIRT);
  set_BitSet(s, r->virtual, true);
}

static void set_as_candidate(Env* env, Reg* r) {
  set_reg(env->candidates, r);
}

static void set_as_excluded(Env* env, Reg* r) {
  set_reg(env->excluded, r);
}

static void set_as_in_stack(Env* env, Reg* r) {
  set_reg(env->in_stack, r);
}

static void collect_uses_insts(Env* env, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }

  IRInst* inst = head_IRInstList(l);
  switch (inst->kind) {
    case IR_STACK_ADDR:
      set_as_in_stack(env, inst->rd);
      break;
    case IR_LOAD:
      set_as_candidate(env, get_RegVec(inst->ras, 0));
      set_as_excluded(env, inst->rd);
      break;
    case IR_STORE:
      set_as_candidate(env, get_RegVec(inst->ras, 0));
      set_as_excluded(env, get_RegVec(inst->ras, 1));
      break;
    default:
      for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
        set_as_excluded(env, get_RegVec(inst->ras, i));
      }
      set_as_excluded(env, inst->rd);
      break;
  }

  collect_uses_insts(env, tail_IRInstList(l));
}

static void collect_uses(Env* env, Function* ir) {
  BBList* l = ir->blocks;
  while (!is_nil_BBList(l)) {
    BasicBlock* b = head_BBList(l);
    collect_uses_insts(env, b->insts);
    l = tail_BBList(l);
  }
}
