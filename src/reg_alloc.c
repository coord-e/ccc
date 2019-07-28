#include "reg_alloc.h"
#include "bit_set.h"
#include "vector.h"

// TODO: Type and distinguish real and virtual register index
// TODO: Stop using -1 or 0 to mark something

static void release_unsigned(unsigned i) {}

DECLARE_LIST(unsigned, UIList)
DEFINE_LIST(release_unsigned, unsigned, UIList)

DECLARE_VECTOR(unsigned, UIVec)
DEFINE_VECTOR(release_unsigned, unsigned, UIVec)

typedef struct {
  RegIntervals* intervals;
  UIList* active;
  unsigned active_count;
  BitSet* used;
  UIVec* result;  // -1 -> not allocated, -2 -> spilled
  unsigned stack_count;
  UIVec* locations;  // -1 -> not spilled
} Env;

static Env* init_Env(unsigned virt_count, unsigned real_count, RegIntervals* ivs) {
  Env* env          = calloc(1, sizeof(Env));
  env->intervals    = ivs;
  env->active       = nil_UIList();
  env->active_count = 0;
  env->used         = zero_BitSet(real_count);

  env->result = new_UIVec(virt_count);
  resize_UIVec(env->result, virt_count);
  fill_UIVec(env->result, -1);

  env->stack_count = 0;

  env->locations = new_UIVec(virt_count);
  resize_UIVec(env->locations, virt_count);
  fill_UIVec(env->locations, -1);

  return env;
}

static Interval* interval_of(Env* env, unsigned virtual) {
  return get_RegIntervals(env->intervals, virtual);
}

static unsigned find_free_reg(Env* env) {
  for (unsigned i = 0; i < length_BitSet(env->used); i++) {
    if (!get_BitSet(env->used, i)) {
      return i;
    }
  }
  error("no free reg found");
}

static void alloc_reg(Env* env, unsigned virtual) {
  unsigned real = find_free_reg(env);
  set_BitSet(env->used, real, true);
  set_UIVec(env->result, virtual, real);
}

static void release_reg(Env* env, unsigned virtual) {
  unsigned real = get_UIVec(env->result, virtual);
  set_BitSet(env->used, real, false);
  set_UIVec(env->result, virtual, -1);
}

static void add_to_active_iter(Env* env, unsigned target_virt, Interval* current, UIList* l) {
  if (is_nil_UIList(l)) {
    insert_UIList(target_virt, l);
    return;
  }

  Interval* intv = interval_of(env, head_UIList(l));
  if (intv->to > current->to) {
    insert_UIList(target_virt, l);
    return;
  }

  add_to_active_iter(env, target_virt, current, tail_UIList(l));
}

static void add_to_active(Env* env, unsigned target_virt) {
  add_to_active_iter(env, target_virt, interval_of(env, target_virt), env->active);
  env->active_count++;
}

static void remove_from_active(Env* env, UIList* cur) {
  remove_UIList(cur);
  env->active_count--;
}

static void expire_old_intervals_iter(Env* env, Interval* current, UIList* l) {
  if (is_nil_UIList(l)) {
    return;
  }

  unsigned virtual = head_UIList(l);
  Interval* intv   = interval_of(env, virtual);
  if (intv->to >= current->from) {
    return;
  }

  // expired
  remove_from_active(env, l);
  release_reg(env, virtual);

  expire_old_intervals_iter(env, current, tail_UIList(l));
}

static void expire_old_intervals(Env* env, unsigned target_virt) {
  expire_old_intervals_iter(env, interval_of(env, target_virt), env->active);
}

static void alloc_stack(Env* env, unsigned virt) {
  set_UIVec(env->locations, virt, env->stack_count++);
  set_UIVec(env->result, virt, -2);  // mark as spilled
}

static void spill_at_interval(Env* env, unsigned target) {
  // TODO: can we eliminate this traversal of last element?
  UIList* spill_ptr = last_UIList(env->active);
  unsigned spill    = head_UIList(spill_ptr);

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
  if (intv->to > t->to) {
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

static void walk_regs(Env* env, unsigned real_count, UIList* l) {
  if (is_nil_UIList(l)) {
    return;
  }

  unsigned virtual = head_UIList(l);

  expire_old_intervals(env, virtual);

  if (env->active_count == real_count) {
    spill_at_interval(env, virtual);
  } else {
    alloc_reg(env, virtual);
    add_to_active(env, virtual);
  }

  walk_regs(env, real_count, tail_UIList(l));
}

IR* reg_alloc(unsigned num_regs, RegIntervals* ivs, IR* ir) {
  Env* env             = init_Env(ir->reg_count, num_regs, ivs);
  UIList* ordered_regs = sort_intervals(ivs);

  walk_regs(env, num_regs, ordered_regs);

  release_UIList(ordered_regs);
}
