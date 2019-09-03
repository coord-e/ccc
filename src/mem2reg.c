#include "mem2reg.h"
#include "bit_set.h"
#include "vector.h"

typedef struct {
  IR* ir;
  Function* function;

  BitSet* candidates;
  BitSet* excluded;
  BitSet* in_stack;

  BitSet* replaceable;

  // TODO: fix inefficient memory usage
  UIVec* replaced_regs;  // stack -> replaced reg
  UIVec* assoc_areas;    // addr reg -> stack
} Env;

static Env* init_Env(Function*, IR*);
static void finish_Env(Env*);
static void collect_uses(Env*, Function*);
static void apply_conversion(Env*, Function*);
static void compute_replaceable(Env*);

void mem2reg(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);

    Env* env = init_Env(f, ir);
    collect_uses(env, f);
    compute_replaceable(env);
    apply_conversion(env, f);
    finish_Env(env);

    l = tail_FunctionList(l);
  }
}

static Env* init_Env(Function* f, IR* ir) {
  unsigned reg_count   = f->reg_count;
  unsigned stack_count = f->stack_count + 1;

  Env* env        = malloc(sizeof(Env));
  env->function   = f;
  env->ir         = ir;
  env->candidates = zero_BitSet(reg_count);
  env->excluded   = zero_BitSet(reg_count);
  env->in_stack   = zero_BitSet(reg_count);

  env->assoc_areas = new_UIVec(reg_count);
  resize_UIVec(env->assoc_areas, reg_count);
  fill_UIVec(env->assoc_areas, -1);

  env->replaced_regs = new_UIVec(stack_count);
  resize_UIVec(env->replaced_regs, stack_count);
  fill_UIVec(env->replaced_regs, -1);
  return env;
}

static void finish_Env(Env* env) {
  release_BitSet(env->candidates);
  release_BitSet(env->excluded);
  release_BitSet(env->in_stack);
  release_BitSet(env->replaceable);
  release_UIVec(env->replaced_regs);
  release_UIVec(env->assoc_areas);
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

static void bind_associated_area(Env* env, Reg* addr_reg, unsigned s) {
  assert(addr_reg != NULL);
  assert(addr_reg->kind == REG_VIRT);

  set_UIVec(env->assoc_areas, addr_reg->virtual, s);
}

static void allocate_replaced_reg_for(Env* env, unsigned s) {
  set_UIVec(env->replaced_regs, s, env->function->reg_count++);
}

static void collect_uses_insts(Env* env, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }

  IRInst* inst = head_IRInstList(l);
  switch (inst->kind) {
    case IR_STACK_ADDR:
      set_as_in_stack(env, inst->rd);
      bind_associated_area(env, inst->rd, inst->stack_idx);
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
      if (inst->rd != NULL) {
        set_as_excluded(env, inst->rd);
      }
      break;
  }

  collect_uses_insts(env, tail_IRInstList(l));
}

static void collect_uses(Env* env, Function* ir) {
  BBListIterator* it = front_BBList(ir->blocks);
  while (!is_nil_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);
    collect_uses_insts(env, b->insts);
    it = next_BBListIterator(it);
  }
}

static void compute_replaceable(Env* env) {
  env->replaceable = copy_BitSet(env->candidates);
  diff_BitSet(env->replaceable, env->excluded);
  and_BitSet(env->replaceable, env->in_stack);

  BitSet* ex_areas = zero_BitSet(env->function->stack_count + 1);
  // TODO: reduce the number of iterations
  for (unsigned i = 0; i < length_BitSet(env->replaceable); i++) {
    // i: virt reg idx
    if (get_BitSet(env->replaceable, i)) {
      continue;
    }
    unsigned s = get_UIVec(env->assoc_areas, i);
    if (s == -1) {
      continue;
    }
    // i is not replaceable while i is used as address register in some place
    set_BitSet(ex_areas, s, true);
  }
  for (unsigned i = 0; i < length_BitSet(env->replaceable); i++) {
    // i: virt reg idx
    if (!get_BitSet(env->replaceable, i)) {
      continue;
    }
    unsigned s = get_UIVec(env->assoc_areas, i);
    assert(s != -1);
    if (get_BitSet(ex_areas, s)) {
      set_BitSet(env->replaceable, i, false);
    } else {
      if (get_UIVec(env->replaced_regs, s) == -1) {
        allocate_replaced_reg_for(env, s);
      }
    }
  }
  release_BitSet(ex_areas);
}

static bool is_replaceable(Env* env, Reg* r) {
  assert(r != NULL);
  assert(r->kind == REG_VIRT);

  return get_BitSet(env->replaceable, r->virtual);
}

static IRInst* new_move(Env* env, Reg* rd, Reg* ra) {
  IRInst* inst = new_inst(env->function->inst_count++, env->ir->inst_count++, IR_MOV);
  inst->rd     = copy_Reg(rd);
  push_RegVec(inst->ras, copy_Reg(ra));
  return inst;
}

static Reg* assoc_reg(Env* env, Reg* addr_reg, DataSize size) {
  assert(addr_reg != NULL);
  assert(addr_reg->kind == REG_VIRT);

  unsigned area = get_UIVec(env->assoc_areas, addr_reg->virtual);
  assert(area != -1);
  unsigned reg = get_UIVec(env->replaced_regs, area);
  assert(reg != -1);

  return new_virtual_Reg(size, reg);
}

static void apply_conversion_insts(Env* env, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }

  IRInst* inst     = head_IRInstList(l);
  IRInstList* tail = tail_IRInstList(l);
  switch (inst->kind) {
    case IR_STACK_ADDR:
      if (is_replaceable(env, inst->rd)) {
        remove_IRInstList(l);
        tail = l;
      }
      break;
    case IR_LOAD: {
      Reg* addr_reg = get_RegVec(inst->ras, 0);
      if (is_replaceable(env, addr_reg)) {
        Reg* dest_reg = inst->rd;
        IRInst* m = new_move(env, copy_Reg(dest_reg), assoc_reg(env, addr_reg, inst->data_size));
        remove_IRInstList(l);
        insert_IRInstList(m, l);
        tail = tail_IRInstList(l);
      }
      break;
    }
    case IR_STORE: {
      Reg* addr_reg = get_RegVec(inst->ras, 0);
      if (is_replaceable(env, addr_reg)) {
        Reg* value_reg = get_RegVec(inst->ras, 1);
        IRInst* m = new_move(env, assoc_reg(env, addr_reg, inst->data_size), copy_Reg(value_reg));
        remove_IRInstList(l);
        insert_IRInstList(m, l);
        tail = tail_IRInstList(l);
      }
      break;
    }
    default:
      break;
  }

  apply_conversion_insts(env, tail);
}

static void apply_conversion(Env* env, Function* ir) {
  BBListIterator* it = front_BBList(ir->blocks);
  while (!is_nil_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);
    apply_conversion_insts(env, b->insts);
    it = next_BBListIterator(it);
  }
}
