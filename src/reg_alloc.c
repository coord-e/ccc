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
  IntVec* used_regs;    // index: real reg, value: virtual reg or -1 (not used)
  IntVec* result;       // index: virtual reg, value: real reg or -1 (spill)
  // rewrite_IR
  unsigned stack_count; // counts allocated stack areas
  IntVec* stacks;       // index: virtual reg, value: stack index or -1 (not used)
  IRInstList* insts;    // a list of newly created instructions
  IRInstList* cursor;   // pointer to current head of the list
} Env;

Env* init_env(unsigned num_regs, unsigned reg_count) {
  Env* e = calloc(1, sizeof(Env));
  e->inst_count = 0;
  e->last_uses = new_IntVec(reg_count);
  resize_IntVec(e->last_uses, reg_count);
  e->first_uses = new_IntVec(reg_count);
  resize_IntVec(e->first_uses, reg_count);
  fill_IntVec(e->first_uses, -1);

  // reserve one reg for spilling
  e->num_regs = num_regs - 1;
  e->used_regs = new_IntVec(reg_count);
  resize_IntVec(e->used_regs, reg_count);
  fill_IntVec(e->used_regs, -1);
  e->result = new_IntVec(reg_count);
  resize_IntVec(e->result, reg_count);

  e->stack_count = 0;
  e->stacks = new_IntVec(reg_count);
  resize_IntVec(e->stacks, reg_count);
  fill_IntVec(e->stacks, -1);
  e->insts = nil_IRInstList();
  e->cursor = e->insts;
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

    // -1 -> unused
    int vi = get_IntVec(env->used_regs, i);
    if (vi != -1) {
      // already allocated
      int last = get_IntVec(env->last_uses, vi);
      int t_def = get_IntVec(env->first_uses, target);
      if (last > t_def) {
        // ... and overlapping liveness
        continue;
      }
    }

    *r = i;
    return true;
  }
  return false;
}

int select_spill_target(Env* env, int vi) {
  int candidate = vi;
  for(unsigned i = 0; i < length_IntVec(env->last_uses); i++) {
    // i: virtual register index

    if (get_IntVec(env->result, i) == -1) {
      // already spilled
      continue;
    }

    int last = get_IntVec(env->last_uses, i);
    int t_def = get_IntVec(env->first_uses, vi);
    if (last < t_def) {
      // not active here
      continue;
    }

    int c_last = get_IntVec(env->last_uses, candidate);
    if (c_last < last) {
      // update if `i`'s last occurrence is after `candidate`'s
      candidate = i;
    }
  }
  return candidate;
}

void alloc_regs(Env* env) {
  for(unsigned i = 0; i < length_IntVec(env->last_uses); i++) {
    // i: virtual register index

    int ri;
    if(find_unused(env, i, &ri)) {
      // store the mapping from virtual reg to real reg
      set_IntVec(env->result, i, ri);
      // mark as used
      set_IntVec(env->used_regs, ri, i);

      continue;
    }

    // spilling

    int t_vi = select_spill_target(env, i);
    int prev = get_IntVec(env->result, t_vi);

    // mark as spilled
    set_IntVec(env->result, t_vi, -1);

    set_IntVec(env->result, i, prev);
    set_IntVec(env->used_regs, prev, i);
  }
}

void append_inst(Env* env, IRInst i) {
  env->cursor = snoc_IRInstList(i, env->cursor);
}

bool update_reg(Env* env, Reg* r) {
  if(r->virtual == 0) {
    // zero -> unused
    return false;
  }

  int ri = get_IntVec(env->result, r->virtual - 1);
  if (ri == -1) {
    // spilled
    r->real = env->num_regs; // reserved reg
    return true;
  } else {
    r->real = ri;
    return false;
  }
}

int stack_idx_of(Env* env, int vi) {
  int idx = get_IntVec(env->stacks, vi);
  if (idx != -1) {
    return idx;
  } else {
    int new_i = env->stack_count++;
    set_IntVec(env->stacks, vi, new_i);
    return new_i;
  }
}

void emit_spill_load(Env* env, Reg r) {
  IRInst load = {
    .kind = IR_LOAD,
    .stack_idx = stack_idx_of(env, r.virtual - 1),
    .rd = r,
  };
  append_inst(env, load);
}

void emit_spill_store(Env* env, Reg r) {
  IRInst store = {
    .kind = IR_STORE,
    .stack_idx = stack_idx_of(env, r.virtual - 1),
    .ra = r,
  };
  append_inst(env, store);
}

void rewrite_IR(Env* env, IRInstList* insts) {
  if(is_nil_IRInstList(insts)) {
    return;
  }

  IRInst i = head_IRInstList(insts);
  bool rd_spilled = update_reg(env, &i.rd);
  bool ra_spilled = update_reg(env, &i.ra);

  if (ra_spilled) emit_spill_load(env, i.ra);
  if (rd_spilled) emit_spill_load(env, i.rd);

  append_inst(env, i);

  if (ra_spilled) emit_spill_store(env, i.ra);
  if (rd_spilled) emit_spill_store(env, i.rd);

  rewrite_IR(env, tail_IRInstList(insts));
}

IR* reg_alloc(unsigned num_regs, IR* ir) {
  Env* env = init_env(num_regs, ir->reg_count);
  collect_last_uses(env, ir->insts);
  alloc_regs(env);
  rewrite_IR(env, ir->insts);
  release_IR(ir);

  IR* new_ir = calloc(1, sizeof(IR));
  new_ir->reg_count = num_regs;
  new_ir->insts = env->insts;
  return new_ir;
}
