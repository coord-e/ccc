#include "reg_alloc.h"
#include "vector.h"

// TODO: Type and distinguish real and virtual register index
// TODO: Stop using -1 or 0 to mark something

typedef struct {
  // collect_last_uses
  unsigned inst_count;  // counts instructions in loop
  IntVec* last_uses;    // index: virtual reg, value: ic
  IntVec* first_uses;   // index: virtual reg, value: ic or -1 (not appeared yet)
  IntVec* sorted_regs;  // virtual registers are stored in order of first occurrence
  // alloc_regs
  unsigned num_regs;    // permitted number of registers
  IntVec* used_regs;    // index: real reg, value: virtual reg or -1 (not used)
  IntVec* result;       // index: virtual reg, value: real reg or -1 (spill) or -2 (not filled yet)
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
  e->sorted_regs = new_IntVec(reg_count);

  // reserve one reg for spilling
  e->num_regs = num_regs - 1;
  e->used_regs = new_IntVec(reg_count);
  resize_IntVec(e->used_regs, reg_count);
  fill_IntVec(e->used_regs, -1);
  e->result = new_IntVec(reg_count);
  resize_IntVec(e->result, reg_count);
  fill_IntVec(e->result, -2);

  e->stack_count = 0;
  e->stacks = new_IntVec(reg_count);
  resize_IntVec(e->stacks, reg_count);
  fill_IntVec(e->stacks, -1);
  e->insts = nil_IRInstList();
  e->cursor = e->insts;
  return e;
}

void set_as_used(Env* env, Reg r) {
  if (!r.is_used) {
    return;
  }
  int idx = r.virtual;

  // last_uses
  set_IntVec(env->last_uses, idx, env->inst_count);

  // first_uses
  // take first occurrence as a point of definition
  if (get_IntVec(env->first_uses, idx) == -1) {
    // first occurrence
    set_IntVec(env->first_uses, idx, env->inst_count);
    push_IntVec(env->sorted_regs, idx);
  }
}

void collect_last_uses(Env* env, IRInstList* insts) {
  if (is_nil_IRInstList(insts)) {
    return;
  }

  IRInst* inst = head_IRInstList(insts);
  set_as_used(env, inst->rd);
  if (inst->ras != NULL) {
    for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
      set_as_used(env, get_RegVec(inst->ras, i));
    }
  }

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

    int r = get_IntVec(env->result, i);
    if (r == -1 || r == -2) {
      // already spilled or not allocated yet
      continue;
    }

    int last = get_IntVec(env->last_uses, i);
    int first = get_IntVec(env->first_uses, i);
    int t_def = get_IntVec(env->first_uses, vi);
    if (last < t_def || first > t_def) {
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
  for(unsigned i = 0; i < length_IntVec(env->sorted_regs); i++) {
    int vi = get_IntVec(env->sorted_regs, i);
    // vi: virtual register index

    int ri;
    if(find_unused(env, vi, &ri)) {
      // store the mapping from virtual reg to real reg
      set_IntVec(env->result, vi, ri);
      // mark as used
      set_IntVec(env->used_regs, ri, vi);

      continue;
    }

    // spilling

    int t_vi = select_spill_target(env, vi);
    int prev = get_IntVec(env->result, t_vi);

    // mark as spilled
    set_IntVec(env->result, t_vi, -1);

    set_IntVec(env->result, vi, prev);
    set_IntVec(env->used_regs, prev, vi);
  }
}

void append_inst(Env* env, IRInst* i) {
  env->cursor = snoc_IRInstList(i, env->cursor);
}

void update_reg(Env* env, Reg* r) {
  if(!r->is_used) {
    return;
  }

  int ri = get_IntVec(env->result, r->virtual);
  r->kind = REG_REAL;
  if (ri == -1) {
    // spilled
    r->real = env->num_regs; // reserved reg
    r->is_spilled = true;
  } else {
    r->real = ri;
    r->is_spilled = false;
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
  if (!r.is_spilled) {
    return;
  }

  IRInst* load = new_inst(IR_LOAD);
  load->stack_idx = stack_idx_of(env, r.virtual);
  load->rd = r;
  append_inst(env, load);
}

void emit_spill_store(Env* env, Reg r) {
  if (!r.is_spilled) {
    return;
  }

  IRInst* store = new_inst(IR_STORE);
  store->stack_idx = stack_idx_of(env, r.virtual);
  store->ras = new_RegVec(1);
  push_RegVec(store->ras, r);
  append_inst(env, store);
}

void emit_spill_subs(Env* env) {
  IRInst* subs = new_inst(IR_SUBS);
  subs->stack_idx = env->stack_count;
  env->insts = cons_IRInstList(subs, env->insts);
}

void rewrite_IR(Env* env, IRInstList* insts) {
  if(is_nil_IRInstList(insts)) {
    return;
  }

  IRInst* inst = head_IRInstList(insts);
  update_reg(env, &inst->rd);
  if (inst->ras != NULL) {
    for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
      update_reg(env, ptr_RegVec(inst->ras, i));
    }
  }

  emit_spill_load(env, inst->rd);
  if (inst->ras != NULL) {
    for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
      emit_spill_load(env, get_RegVec(inst->ras, i));
    }
  }

  append_inst(env, inst);

  emit_spill_store(env, inst->rd);
  if (inst->ras != NULL) {
    for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
      emit_spill_store(env, get_RegVec(inst->ras, i));
    }
  }

  rewrite_IR(env, tail_IRInstList(insts));
}

IR* reg_alloc(unsigned num_regs, IR* ir) {
  Env* env = init_env(num_regs, ir->reg_count);
  collect_last_uses(env, ir->insts);
  alloc_regs(env);
  rewrite_IR(env, ir->insts);

  emit_spill_subs(env);

  IR* new_ir = calloc(1, sizeof(IR));
  new_ir->reg_count = num_regs;
  new_ir->insts = env->insts;
  return new_ir;
}
