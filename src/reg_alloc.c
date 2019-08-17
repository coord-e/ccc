#include "reg_alloc.h"
#include "bit_set.h"
#include "double_list.h"
#include "vector.h"

// TODO: Type and distinguish real and virtual register index
// TODO: Stop using -1 or 0 to mark something

static void release_unsigned(unsigned i) {}

DECLARE_DLIST(unsigned, UIDList)
DEFINE_DLIST(release_unsigned, unsigned, UIDList)

DECLARE_LIST(unsigned, UIList)
DEFINE_LIST(release_unsigned, unsigned, UIList)

DECLARE_VECTOR(unsigned, UIVec)
DEFINE_VECTOR(release_unsigned, unsigned, UIVec)

typedef struct {
  RegIntervals* intervals;  // not owned
  UIDList* active;          // owned
  unsigned active_count;
  UIVec* used_by;  // owned, -1 -> not used
  UIVec* result;   // owned, -1 -> not allocated, -2 -> spilled
  unsigned stack_count;
  UIVec* locations;  // owned, -1 -> not spilled
  unsigned usable_regs_count;
  unsigned reserved_for_spill;

  unsigned local_count;   // local inst count
  unsigned global_count;  // global inst count
} Env;

static Env* init_Env(unsigned virt_count,
                     unsigned real_count,
                     unsigned stack_count,
                     unsigned local_inst_count,
                     unsigned global_inst_count,
                     RegIntervals* ivs) {
  Env* env                = calloc(1, sizeof(Env));
  env->usable_regs_count  = real_count - 1;
  env->reserved_for_spill = real_count - 1;
  env->intervals          = ivs;
  env->active             = new_UIDList();
  env->active_count       = 0;
  env->used_by            = new_UIVec(env->usable_regs_count);
  resize_UIVec(env->used_by, env->usable_regs_count);
  fill_UIVec(env->used_by, -1);

  env->result = new_UIVec(virt_count);
  resize_UIVec(env->result, virt_count);
  fill_UIVec(env->result, -1);

  env->stack_count  = stack_count;
  env->local_count  = local_inst_count;
  env->global_count = global_inst_count;

  env->locations = new_UIVec(virt_count);
  resize_UIVec(env->locations, virt_count);
  fill_UIVec(env->locations, -1);

  return env;
}

static void release_Env(Env* env) {
  release_UIDList(env->active);
  release_UIVec(env->used_by);
  release_UIVec(env->result);
  release_UIVec(env->locations);
  free(env);
}

static IRInst* new_inst_(Env* env, IRInstKind kind) {
  return new_inst(env->local_count++, env->global_count++, kind);
}

static Interval* interval_of(Env* env, unsigned virtual) {
  return get_RegIntervals(env->intervals, virtual);
}

static unsigned find_free_reg(Env* env) {
  for (unsigned i = 0; i < length_UIVec(env->used_by); i++) {
    if (get_UIVec(env->used_by, i) == -1) {
      return i;
    }
  }
  error("no free reg found");
}

static void alloc_specific_reg(Env* env, unsigned virtual, unsigned real) {
  assert(get_UIVec(env->used_by, real) == -1);
  set_UIVec(env->used_by, real, virtual);
  set_UIVec(env->result, virtual, real);
}

static void alloc_reg(Env* env, unsigned virtual) {
  unsigned real = find_free_reg(env);
  alloc_specific_reg(env, virtual, real);
}

static void release_reg(Env* env, unsigned virtual) {
  unsigned real = get_UIVec(env->result, virtual);
  if (real == -1) {
    return;
  }

  set_UIVec(env->used_by, real, -1);
}

static void add_to_active_iter(Env* env,
                               unsigned target_virt,
                               Interval* current,
                               UIDListIterator* l) {
  if (is_nil_UIDListIterator(l)) {
    insert_UIDListIterator(l, target_virt);
    return;
  }

  Interval* intv = interval_of(env, data_UIDListIterator(l));
  if (intv->to > current->to) {
    insert_UIDListIterator(l, target_virt);
    return;
  }

  add_to_active_iter(env, target_virt, current, next_UIDListIterator(l));
}

static void add_to_active(Env* env, unsigned target_virt) {
  add_to_active_iter(env, target_virt, interval_of(env, target_virt), front_UIDList(env->active));
  env->active_count++;
}

static void remove_from_active(Env* env, UIDListIterator* cur) {
  remove_UIDListIterator(cur);
  env->active_count--;
}

static void expire_old_intervals_iter(Env* env, Interval* current, UIDListIterator* l) {
  if (is_nil_UIDListIterator(l)) {
    return;
  }

  unsigned virtual = data_UIDListIterator(l);
  Interval* intv   = interval_of(env, virtual);
  if (intv->to >= current->from) {
    return;
  }

  UIDListIterator* next = next_UIDListIterator(l);
  // expired
  remove_from_active(env, l);
  release_reg(env, virtual);

  expire_old_intervals_iter(env, current, next);
}

static void remove_virtual_from_active_iter(Env* env, unsigned target, UIDListIterator* l) {
  if (is_nil_UIDListIterator(l)) {
    CCC_UNREACHABLE;
    return;
  }

  if (data_UIDListIterator(l) == target) {
    remove_from_active(env, l);
    return;
  }

  remove_virtual_from_active_iter(env, target, next_UIDListIterator(l));
}

static void remove_virtual_from_active(Env* env, unsigned target) {
  remove_virtual_from_active_iter(env, target, front_UIDList(env->active));
}

static void expire_old_intervals(Env* env, Interval* target_iv) {
  expire_old_intervals_iter(env, target_iv, front_UIDList(env->active));
}

static void alloc_stack(Env* env, unsigned virt) {
  // TODO: Store the size of spilled registers and use that here
  env->stack_count += 8;
  set_UIVec(env->locations, virt, env->stack_count);
  set_UIVec(env->result, virt, -2);  // mark as spilled
}

static void spill_at_interval(Env* env, unsigned target) {
  UIDListIterator* spill_ptr = back_UIDList(env->active);
  unsigned spill             = data_UIDListIterator(spill_ptr);

  Interval* spill_intv  = interval_of(env, spill);
  Interval* target_intv = interval_of(env, target);
  if (spill_intv->to > target_intv->to) {
    set_UIVec(env->result, target, get_UIVec(env->result, spill));
    alloc_stack(env, spill);
    remove_from_active(env, spill_ptr);
    add_to_active(env, target);
  } else {
    alloc_stack(env, target);
  }
}

static void sort_intervals_insert_reg(RegIntervals* ivs, Interval* t, unsigned v, UIList* l) {
  if (is_nil_UIList(l)) {
    insert_UIList(v, l);
    return;
  }

  Interval* intv = get_RegIntervals(ivs, head_UIList(l));
  if (intv->from > t->from) {
    insert_UIList(v, l);
    return;
  }

  sort_intervals_insert_reg(ivs, t, v, tail_UIList(l));
}

// TODO: Faster sort
static UIList* sort_intervals(RegIntervals* ivs) {
  unsigned len   = length_RegIntervals(ivs);
  UIList* result = nil_UIList();
  for (unsigned i = 0; i < len; i++) {
    Interval* interval = get_RegIntervals(ivs, i);
    sort_intervals_insert_reg(ivs, interval, i, result);
  }
  return result;
}

static void walk_regs(Env* env, UIList* l) {
  if (is_nil_UIList(l)) {
    return;
  }

  unsigned virtual = head_UIList(l);
  Interval* iv     = interval_of(env, virtual);

  expire_old_intervals(env, iv);

  switch (iv->kind) {
    case IV_VIRTUAL:
      if (env->active_count == env->usable_regs_count) {
        spill_at_interval(env, virtual);
      } else {
        alloc_reg(env, virtual);
        add_to_active(env, virtual);
      }
      break;
    case IV_FIXED: {
      unsigned u = get_UIVec(env->used_by, iv->fixed_real);
      if (u != -1) {
        unsigned blocked_virt = u;
        release_reg(env, blocked_virt);
        alloc_stack(env, blocked_virt);
        remove_virtual_from_active(env, blocked_virt);
      }
      alloc_specific_reg(env, virtual, iv->fixed_real);
      add_to_active(env, virtual);
      break;
    }
    default:
      CCC_UNREACHABLE;
  }

  walk_regs(env, tail_UIList(l));
}

static bool assign_reg(Env* env, Reg* r) {
  unsigned real = get_UIVec(env->result, r->virtual);
  if (real == -1) {
    error("failed to allocate register: %d", r->virtual);
  }

  if (r->kind == REG_FIXED) {
    assert(r->real == real);
  }

  r->kind = REG_REAL;

  if (real == -2) {
    r->real = env->reserved_for_spill;
    return true;
  }

  r->real = real;
  return false;
}

static IRInstList* emit_spill_load(Env* env, Reg r, IRInstList** lref) {
  IRInstList* l = *lref;

  IRInst* inst    = new_inst_(env, IR_STACK_LOAD);
  inst->rd        = r;
  inst->stack_idx = get_UIVec(env->locations, r.virtual);
  inst->data_size = r.size;
  insert_IRInstList(inst, l);

  IRInstList* t = tail_IRInstList(l);
  *lref         = t;
  return tail_IRInstList(t);
}

static IRInstList* emit_spill_store(Env* env, Reg r, IRInstList* l) {
  IRInst* inst = new_inst_(env, IR_STACK_STORE);
  push_RegVec(inst->ras, r);
  inst->stack_idx = get_UIVec(env->locations, r.virtual);
  inst->data_size = r.size;

  IRInstList* t = tail_IRInstList(l);
  insert_IRInstList(inst, t);
  return tail_IRInstList(t);
}

static void assign_reg_num_iter_insts(Env* env, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }

  IRInst* inst     = head_IRInstList(l);
  IRInstList* tail = tail_IRInstList(l);

  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg ra = get_RegVec(inst->ras, i);
    assert(ra.is_used);

    if (assign_reg(env, &ra)) {
      tail = emit_spill_load(env, ra, &l);
    }

    set_RegVec(inst->ras, i, ra);
  }

  if (inst->rd.is_used) {
    if (assign_reg(env, &inst->rd)) {
      tail = emit_spill_store(env, inst->rd, l);
    }
  }

  assign_reg_num_iter_insts(env, tail);
}

static void assign_reg_num(Env* env, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }

  BasicBlock* b = head_BBList(l);
  assign_reg_num_iter_insts(env, b->insts);
  b->sorted_insts = NULL;

  assign_reg_num(env, tail_BBList(l));
}

static void reg_alloc_function(unsigned num_regs, unsigned* global_inst_count, Function* ir) {
  RegIntervals* ivs = ir->intervals;
  Env* env =
      init_Env(ir->reg_count, num_regs, ir->stack_count, ir->inst_count, *global_inst_count, ivs);
  UIList* ordered_regs = sort_intervals(ivs);

  walk_regs(env, ordered_regs);

  release_UIList(ordered_regs);

  assign_reg_num(env, ir->blocks);

  ir->used_regs = zero_BitSet(num_regs);
  for (unsigned i = 0; i < length_UIVec(env->result); i++) {
    unsigned real = get_UIVec(env->result, i);
    if (real == -2) {
      set_BitSet(ir->used_regs, env->reserved_for_spill, true);
    } else {
      assert(real != -1);
      set_BitSet(ir->used_regs, real, true);
    }
  }
  ir->real_reg_count = count_BitSet(ir->used_regs);
  ir->stack_count    = env->stack_count;
  ir->inst_count     = env->local_count;
  *global_inst_count = env->global_count;

  release_Env(env);
}

static void reg_alloc_functions(unsigned num_regs, unsigned* inst_count, FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  reg_alloc_function(num_regs, inst_count, head_FunctionList(l));

  reg_alloc_functions(num_regs, inst_count, tail_FunctionList(l));
}

void reg_alloc(unsigned num_regs, IR* ir) {
  reg_alloc_functions(num_regs, &ir->inst_count, ir->functions);
}
