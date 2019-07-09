#include "reg_alloc.h"
#include "vector.h"

// TODO: Type and distinguish real and virtual register index
// TODO: Stop using -1 or 0 to mark something

typedef struct {
  // collect_last_uses
  unsigned inst_count;  // counts instructions in loop
  IntVec* last_uses;    // index: virtual reg, value: ic
  IntVec* first_uses;   // index: virtual reg, value: ic or -1 (not appeared yet)
  // alloc_regs
  unsigned num_regs;    // permitted number of registers
  IntVec* used_regs;    // index: real reg, value: virtual reg or zero (not used)
  IntVec* result;       // index: virtual reg, value: real reg
} Env;

Env* init_env(unsigned num_regs, unsigned reg_count) {
  Env* e = calloc(1, sizeof(Env));
  e->inst_count = 0;
  e->last_uses = new_IntVec(reg_count);
  resize_IntVec(e->last_uses, reg_count);
  e->first_uses = new_IntVec(reg_count);
  resize_IntVec(e->first_uses, reg_count);
  fill_IntVec(e->first_uses, -1);

  e->num_regs = num_regs;
  e->used_regs = new_IntVec(reg_count);
  resize_IntVec(e->used_regs, reg_count);
  e->result = new_IntVec(reg_count);
  resize_IntVec(e->result, reg_count);
  return e;
}

void set_as_used(Env* env, Reg r) {
  // zero -> unused
  if (r.virtual == 0) {
    return;
  }
  int idx = r.virtual - 1;

  // last_uses
  set_IntVec(env->last_uses, idx, env->inst_count);

  // first_uses
  // take first occurrence as a point of definition
  if (get_IntVec(env->first_uses, idx) == -1) {
    // first occurrence
    set_IntVec(env->first_uses, idx, env->inst_count);
  }
}

void collect_last_uses(Env* env, IRInstList* insts) {
  if (is_nil_IRInstList(insts)) {
    return;
  }

  IRInst i = head_IRInstList(insts);
  set_as_used(env, i.rd);
  set_as_used(env, i.ra);

  env->inst_count++;

  collect_last_uses(env, tail_IRInstList(insts));
}

// if found, return `true` and set real reg index to `r`
// `target`: virtual register index targetted in `alloc_regs`
bool find_unused(Env* env, int target, int* r) {
  for (unsigned i = 0; i < env->num_regs; i++) {
    // iterating over usable registers
    // i: real reg index

    // zero -> unused
    int vi = get_IntVec(env->used_regs, i);
    if (vi != 0) {
      // already allocated
      int last = get_IntVec(env->last_uses, vi);
      int t_def = get_IntVec(env->first_uses, target);
      if (last > t_def) {
        // ... and overlapping liveness
        continue;
      }
    }

    *r = vi;
    return true;
  }
  return false;
}

void alloc_regs(Env* env) {
  for(unsigned i = 0; i < length_IntVec(env->last_uses); i++) {
    // i: virtual register index

    int rn;
    if(find_unused(env, i, &rn)) {
      // store the mapping from virtual reg to real reg
      set_IntVec(env->result, i, rn);
      // mark as used
      set_IntVec(env->used_regs, rn, i);

      continue;
    }

    error("spill not implemented!");
  }
}

void reg_alloc(unsigned num_regs, IR* ir) {
  Env* env = init_env(num_regs, ir->reg_count);
  collect_last_uses(env, ir->insts);
  env->inst_count = 0;
  alloc_regs(env);
}
