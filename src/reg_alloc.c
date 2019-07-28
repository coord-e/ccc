#include "reg_alloc.h"
#include "vector.h"
#include "bit_set.h"

// TODO: Type and distinguish real and virtual register index
// TODO: Stop using -1 or 0 to mark something

DECLARE_LIST(unsigned, UIList)
static void release_unsigned(unsigned i) {}
DEFINE_LIST(release_unsigned, unsigned, UIList)

typeof struct {
  RegIntervals* intervals;
  UIList* active;
  unsigned active_count;
  BitSet* used;
  UIVec* result;
} Env;

static Env* init_Env(unsigned reg_count, RegIntervals* ivs) {
  Env* env = calloc(1, sizeof(Env));
  env->intervals = ivs;
  env->active = nil_UIList();
  env->active_count = 0;
  env->used = zero_BitSet(reg_count);
  env->result = new_UIVec(reg_count);
  resize_UIVec(env->result, reg_count);
  return env;
}

IR* reg_alloc(unsigned num_regs, RegIntervals* ivs, IR* ir) {
  Env* env = init_Env(ir->reg_count, ivs);
  UIVec* ordered_regs = sort_intervals(ivs);

  for (unsigned virtual = 0; virtual < length_UIVec(ordered_regs); virtual++) {
    expire_old_intervals(env, virtual);

    if (env->active_count == num_regs) {
      spill_at_interval(env, virtual);
    } else {
      alloc_free_reg(env, virtual);
      add_to_active(env, virtual);
    }
  }
}
