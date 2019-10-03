#include "reg_alloc.h"
#include "arch.h"
#include "bit_set.h"
#include "indexed_list.h"
#include "list.h"
#include "vector.h"

// TODO: Type and distinguish real and virtual register index
// TODO: Stop using -1 or 0 to mark something

static void release_unsigned(unsigned i) {}
DECLARE_LIST(unsigned, UIList)
DEFINE_LIST(release_unsigned, unsigned, UIList)

DECLARE_INDEXED_LIST(unsigned, UIIList)
DEFINE_INDEXED_LIST(release_unsigned, unsigned, UIIList)

typedef struct {
  UIIList* active;     // owned, a list of virtual registers
  UIIList* available;  // owned, a list of real registers
  UIVec* used_by;      // owned, -1 -> not used
  UIVec* result;       // owned, -1 -> not allocated, -2 -> spilled
  UIVec* locations;    // owned, -1 -> not spilled
  unsigned usable_regs_count;
  unsigned reserved_for_spill;

  unsigned* global_count;

  Function* f;
} Env;

static void add_to_available(Env* env, unsigned real);

static Env* init_Env(Function* f, unsigned* global_count, unsigned real_count) {
  unsigned virt_count = f->reg_count;

  Env* env                = calloc(1, sizeof(Env));
  env->f                  = f;
  env->usable_regs_count  = real_count - 1;
  env->reserved_for_spill = real_count - 1;
  env->active             = new_UIIList(virt_count);
  env->available          = new_UIIList(env->usable_regs_count);
  env->used_by            = new_UIVec(env->usable_regs_count);
  resize_UIVec(env->used_by, env->usable_regs_count);
  fill_UIVec(env->used_by, -1);

  env->result = new_UIVec(virt_count);
  resize_UIVec(env->result, virt_count);
  fill_UIVec(env->result, -1);

  env->locations = new_UIVec(virt_count);
  resize_UIVec(env->locations, virt_count);
  fill_UIVec(env->locations, -1);

  env->global_count = global_count;

  for (unsigned i = 0; i < env->usable_regs_count; i++) {
    add_to_available(env, i);
  }

  return env;
}

static void release_Env(Env* env) {
  release_UIIList(env->active);
  release_UIIList(env->available);
  release_UIVec(env->used_by);
  release_UIVec(env->result);
  release_UIVec(env->locations);
  free(env);
}

static IRInst* new_inst_(Env* env, IRInstKind kind) {
  return new_inst(env->f->inst_count++, (*env->global_count)++, kind);
}

static Interval* interval_of(Env* env, unsigned virtual) {
  return get_RegIntervals(env->f->intervals, virtual);
}

// return true if r1 is preferred than r2
static bool compare_priority(Env* env, unsigned r1, unsigned r2) {
  if (get_BitSet(env->f->used_fixed_regs, r1)) {
    return false;
  }
  if (get_BitSet(env->f->used_fixed_regs, r2)) {
    return true;
  }

  if (env->f->call_count > 0) {
    return is_scratch[r1] < is_scratch[r2];
  } else {
    return is_scratch[r1] > is_scratch[r2];
  }
}

static void add_to_available(Env* env, unsigned real) {
  UIIListIterator* it = front_UIIList(env->available);
  while (true) {
    if (is_nil_UIIListIterator(it) || compare_priority(env, real, data_UIIListIterator(it))) {
      insert_with_idx_UIIListIterator(env->available, real, it, real);
      break;
    }
    it = next_UIIListIterator(it);
  }
}

static void add_to_active(Env* env, unsigned target_virt) {
  Interval* current = interval_of(env, target_virt);

  UIIListIterator* it = front_UIIList(env->active);
  while (true) {
    if (is_nil_UIIListIterator(it) ||
        interval_of(env, data_UIIListIterator(it))->to > current->to) {
      insert_with_idx_UIIListIterator(env->active, target_virt, it, target_virt);
      break;
    }
    it = next_UIIListIterator(it);
  }
}

static unsigned find_free_reg(Env* env) {
  if (is_empty_UIIList(env->available)) {
    error("no free reg found");
  }
  return head_UIIList(env->available);
}

static void alloc_specific_reg(Env* env, unsigned virtual, unsigned real) {
  assert(get_UIVec(env->used_by, real) == -1);
  set_UIVec(env->used_by, real, virtual);
  set_UIVec(env->result, virtual, real);
  remove_by_idx_UIIListIterator(env->available, real);
  add_to_active(env, virtual);
}

static void alloc_reg(Env* env, unsigned virtual) {
  unsigned real = find_free_reg(env);
  alloc_specific_reg(env, virtual, real);
}

static void release_reg(Env* env, unsigned virtual) {
  unsigned real = get_UIVec(env->result, virtual);
  assert(real != -1 && real != -2);

  set_UIVec(env->used_by, real, -1);
  remove_by_idx_UIIListIterator(env->active, virtual);
  add_to_available(env, real);
}

static void expire_old_intervals(Env* env, Interval* target_iv) {
  UIIListIterator* it = front_UIIList(env->active);
  while (!is_nil_UIIListIterator(it)) {
    unsigned virtual = data_UIIListIterator(it);
    Interval* intv   = interval_of(env, virtual);
    if (intv->to >= target_iv->from) {
      return;
    }
    // NOTE: obtain `it` earlier because `release_reg` modifies active list
    it = next_UIIListIterator(it);

    release_reg(env, virtual);
  }
}

static void alloc_stack(Env* env, unsigned virt) {
  // TODO: Store the size of spilled registers and use that here
  env->f->stack_count += 8;
  set_UIVec(env->locations, virt, env->f->stack_count);
  set_UIVec(env->result, virt, -2);  // mark as spilled
}

static void spill_reg(Env* env, unsigned virt) {
  release_reg(env, virt);
  alloc_stack(env, virt);
}

static void spill_at_interval(Env* env, unsigned target) {
  UIIListIterator* spill_ptr = back_UIIList(env->active);
  unsigned spill;
  Interval* spill_intv = NULL;
  while (true) {
    spill      = data_UIIListIterator(spill_ptr);
    spill_intv = interval_of(env, spill);
    if (spill_intv->kind != IV_FIXED) {
      break;
    } else {
      spill_ptr = prev_UIIListIterator(spill_ptr);
    }
  }
  assert(spill_intv->kind != IV_FIXED);
  Interval* target_intv = interval_of(env, target);
  if (spill_intv->to > target_intv->to) {
    unsigned r = get_UIVec(env->result, spill);
    spill_reg(env, spill);
    alloc_specific_reg(env, target, r);
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
      if (is_empty_UIIList(env->available)) {
        spill_at_interval(env, virtual);
      } else {
        alloc_reg(env, virtual);
      }
      break;
    case IV_FIXED: {
      unsigned u = get_UIVec(env->used_by, iv->fixed_real);
      if (u != -1) {
        spill_reg(env, u);
      }
      alloc_specific_reg(env, virtual, iv->fixed_real);
      break;
    }
    case IV_UNSET:
      assert(iv->from == -1 && iv->to == -1);
      break;
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

static void emit_spill_load(Env* env, Reg* r, IRInstListIterator* it) {
  IRInst* inst    = new_inst_(env, IR_STACK_LOAD);
  inst->rd        = copy_Reg(r);
  inst->stack_idx = get_UIVec(env->locations, r->virtual);
  inst->data_size = r->size;
  insert_IRInstListIterator(it, inst);
}

static void emit_spill_store(Env* env, Reg* r, IRInstListIterator* it) {
  IRInst* inst = new_inst_(env, IR_STACK_STORE);
  push_RegVec(inst->ras, copy_Reg(r));
  inst->stack_idx = get_UIVec(env->locations, r->virtual);
  inst->data_size = r->size;

  insert_IRInstListIterator(it, inst);
}

static void assign_reg_num_iter_insts(Env* env, IRInstListIterator* it) {
  if (is_nil_IRInstListIterator(it)) {
    return;
  }

  IRInst* inst             = data_IRInstListIterator(it);
  IRInstListIterator* next = next_IRInstListIterator(it);

  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* ra = get_RegVec(inst->ras, i);

    if (assign_reg(env, ra)) {
      emit_spill_load(env, ra, it);
    }
  }

  if (inst->rd != NULL) {
    if (assign_reg(env, inst->rd)) {
      emit_spill_store(env, inst->rd, next);
      next = next_IRInstListIterator(next);
    }
  }

  assign_reg_num_iter_insts(env, next);
}

static void assign_reg_num(Env* env, BBListIterator* it) {
  if (is_nil_BBListIterator(it)) {
    return;
  }

  BasicBlock* b = data_BBListIterator(it);

  assign_reg_num_iter_insts(env, front_IRInstList(b->insts));

  if (b->is_call_bb) {
    b->should_preserve = new_BitSet(env->usable_regs_count + 1);
    BitSet* s          = copy_BitSet(b->live_in);
    and_BitSet(s, b->live_out);
    for (unsigned i = 0; i < length_BitSet(s); i++) {
      if (get_BitSet(s, i)) {
        unsigned real = get_UIVec(env->result, i);
        if (real != -2) {
          assert(real != -1);
          set_BitSet(b->should_preserve, real, true);
        }
      }
    }
    release_BitSet(s);
  }

  assign_reg_num(env, next_BBListIterator(it));
}

static void reg_alloc_function(unsigned num_regs, unsigned* global_inst_count, Function* ir) {
  RegIntervals* ivs    = ir->intervals;
  Env* env             = init_Env(ir, global_inst_count, num_regs);
  UIList* ordered_regs = sort_intervals(ivs);

  walk_regs(env, ordered_regs);

  release_UIList(ordered_regs);

  assign_reg_num(env, front_BBList(ir->blocks));

  ir->used_regs = zero_BitSet(num_regs);
  for (unsigned i = 0; i < length_UIVec(env->result); i++) {
    unsigned real = get_UIVec(env->result, i);
    if (real == -2) {
      set_BitSet(ir->used_regs, env->reserved_for_spill, true);
    } else if (real != -1) {
      set_BitSet(ir->used_regs, real, true);
    }
  }

  release_Env(env);
}

static void reg_alloc_functions(unsigned num_regs, unsigned* inst_count, FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  reg_alloc_function(num_regs, inst_count, head_FunctionList(l));

  reg_alloc_functions(num_regs, inst_count, tail_FunctionList(l));
}

static Interval* new_interval(unsigned from, unsigned to) {
  Interval* iv = calloc(1, sizeof(Interval));
  iv->from     = from;
  iv->to       = to;
  return iv;
}

static void set_interval_kind(Interval* iv, Reg* r) {
  switch (r->kind) {
    case REG_FIXED:
      assert(iv->kind == IV_UNSET || iv->kind == IV_FIXED);
      iv->kind       = IV_FIXED;
      iv->fixed_real = r->real;
      break;
    case REG_VIRT:
      assert(iv->kind == IV_UNSET || iv->kind == IV_VIRTUAL);
      iv->kind = IV_VIRTUAL;
      break;
    case REG_REAL:
      assert(false && "real registers found in register allocation");
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static void add_range(Interval* iv, unsigned from, unsigned to) {
  if (iv->from == -1 || iv->from > from) {
    iv->from = from;
  }
  if (iv->to == -1 || iv->to < to) {
    iv->to = to;
  }
}

static void set_from(Interval* iv, unsigned from) {
  iv->from = from;
  if (iv->to == -1) {
    iv->to = from;
  }
}

static void build_intervals_insts(RegIntervals* ivs, IRInstList* insts, unsigned block_from) {
  // reverse order
  for (IRInstListIterator* it = back_IRInstList(insts); !is_nil_IRInstListIterator(it);
       it                     = prev_IRInstListIterator(it)) {
    IRInst* inst = data_IRInstListIterator(it);

    for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
      Reg* ra = get_RegVec(inst->ras, i);

      Interval* iv = get_RegIntervals(ivs, ra->virtual);
      add_range(iv, block_from, inst->local_id);
      set_interval_kind(iv, ra);
    }

    if (inst->rd != NULL) {
      Interval* iv = get_RegIntervals(ivs, inst->rd->virtual);
      set_from(iv, inst->local_id);
      set_interval_kind(iv, inst->rd);
    }
  }
}

static RegIntervals* build_intervals(Function* ir) {
  RegIntervals* ivs = new_RegIntervals(ir->reg_count);
  for (unsigned i = 0; i < ir->reg_count; i++) {
    push_RegIntervals(ivs, new_interval(-1, -1));
  }

  // reverse order
  for (BBListIterator* it = back_BBList(ir->blocks); !is_nil_BBListIterator(it);
       it                 = prev_BBListIterator(it)) {
    BasicBlock* b       = data_BBListIterator(it);
    unsigned block_from = head_IRInstList(b->insts)->local_id;
    unsigned block_to   = last_IRInstList(b->insts)->local_id;

    for (unsigned vi = 0; vi < length_BitSet(b->live_out); vi++) {
      if (get_BitSet(b->live_out, vi)) {
        Interval* iv = get_RegIntervals(ivs, vi);
        add_range(iv, block_from, block_to);
      }
    }

    build_intervals_insts(ivs, b->insts, block_from);
  }

  return ivs;
}

static void liveness_functions(FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  Function* f  = head_FunctionList(l);
  f->intervals = build_intervals(f);

  liveness_functions(tail_FunctionList(l));
}

static void liveness(IR* ir) {
  liveness_functions(ir->functions);
}

void reg_alloc(unsigned num_regs, IR* ir) {
  liveness(ir);
  reg_alloc_functions(num_regs, &ir->inst_count, ir->functions);
}
