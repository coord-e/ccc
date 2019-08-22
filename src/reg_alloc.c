#include "reg_alloc.h"
#include "arch.h"
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

static void release_dummy(UIDListIterator* p) {}
DECLARE_VECTOR(UIDListIterator*, UIDLIterVec)
DEFINE_VECTOR(release_dummy, UIDListIterator*, UIDLIterVec)

typedef struct {
  UIDList* list;
  UIDLIterVec* iterators;
} IndexedUIList;

static IndexedUIList* new_IndexedUIList(unsigned max_idx) {
  IndexedUIList* l = calloc(1, sizeof(IndexedUIList));
  l->list          = new_UIDList();
  l->iterators     = new_UIDLIterVec(max_idx);
  resize_UIDLIterVec(l->iterators, max_idx);
  fill_UIDLIterVec(l->iterators, NULL);
  return l;
}

static void release_IndexedUIList(IndexedUIList* l) {
  release_UIDList(l->list);
  // TODO: shallow release
  /* release_UIDIterVec(l->iterators); */
}

static UIDListIterator* get_IndexedUIList(IndexedUIList* l, unsigned idx) {
  return get_UIDLIterVec(l->iterators, idx);
}

static UIDListIterator* insert_IndexedUIList(IndexedUIList* l,
                                             unsigned idx,
                                             UIDListIterator* it,
                                             unsigned val) {
  UIDListIterator* new = insert_UIDListIterator(it, val);
  set_UIDLIterVec(l->iterators, idx, new);
  return new;
}

static void remove_IndexedUIList(IndexedUIList* l, unsigned idx, UIDListIterator* it) {
  remove_UIDListIterator(it);
  set_UIDLIterVec(l->iterators, idx, NULL);
}

static void remove_by_idx_IndexedUIList(IndexedUIList* l, unsigned idx) {
  UIDListIterator* it = get_UIDLIterVec(l->iterators, idx);
  assert(it != NULL);
  remove_IndexedUIList(l, idx, it);
}

typedef struct {
  IndexedUIList* active;     // owned, a list of virtual registers
  IndexedUIList* available;  // owned, a list of real registers
  UIVec* used_by;            // owned, -1 -> not used
  UIVec* result;             // owned, -1 -> not allocated, -2 -> spilled
  UIVec* locations;          // owned, -1 -> not spilled
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
  env->active             = new_IndexedUIList(virt_count);
  env->available          = new_IndexedUIList(env->usable_regs_count);
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
  release_IndexedUIList(env->active);
  release_IndexedUIList(env->available);
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
  UIDListIterator* it = front_UIDList(env->available->list);
  while (true) {
    if (is_nil_UIDListIterator(it) || compare_priority(env, real, data_UIDListIterator(it))) {
      insert_IndexedUIList(env->available, real, it, real);
      break;
    }
    it = next_UIDListIterator(it);
  }
}

static void add_to_active(Env* env, unsigned target_virt) {
  Interval* current = interval_of(env, target_virt);

  UIDListIterator* it = front_UIDList(env->active->list);
  while (true) {
    if (is_nil_UIDListIterator(it) ||
        interval_of(env, data_UIDListIterator(it))->to > current->to) {
      insert_IndexedUIList(env->active, target_virt, it, target_virt);
      break;
    }
    it = next_UIDListIterator(it);
  }
}

static unsigned find_free_reg(Env* env) {
  if (is_empty_UIDList(env->available->list)) {
    error("no free reg found");
  }
  return head_UIDList(env->available->list);
}

static void alloc_specific_reg(Env* env, unsigned virtual, unsigned real) {
  assert(get_UIVec(env->used_by, real) == -1);
  set_UIVec(env->used_by, real, virtual);
  set_UIVec(env->result, virtual, real);
  remove_by_idx_IndexedUIList(env->available, real);
  add_to_active(env, virtual);
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
  remove_by_idx_IndexedUIList(env->active, virtual);
  add_to_available(env, real);
}

static void expire_old_intervals(Env* env, Interval* target_iv) {
  UIDListIterator* it = front_UIDList(env->active->list);
  while (!is_nil_UIDListIterator(it)) {
    unsigned virtual = data_UIDListIterator(it);
    Interval* intv   = interval_of(env, virtual);
    if (intv->to >= target_iv->from) {
      return;
    }
    // NOTE: obtain `it` earlier because `release_reg` modifies active list
    it = next_UIDListIterator(it);

    release_reg(env, virtual);
  }
}

static void alloc_stack(Env* env, unsigned virt) {
  // TODO: Store the size of spilled registers and use that here
  env->f->stack_count += 8;
  set_UIVec(env->locations, virt, env->f->stack_count);
  set_UIVec(env->result, virt, -2);  // mark as spilled
}

static void spill_at_interval(Env* env, unsigned target) {
  UIDListIterator* spill_ptr = back_UIDList(env->active->list);
  unsigned spill;
  Interval* spill_intv = NULL;
  while (true) {
    spill      = data_UIDListIterator(spill_ptr);
    spill_intv = interval_of(env, spill);
    if (spill_intv->kind != IV_FIXED) {
      break;
    } else {
      spill_ptr = prev_UIDListIterator(spill_ptr);
    }
  }
  assert(spill_intv->kind != IV_FIXED);
  Interval* target_intv = interval_of(env, target);
  if (spill_intv->to > target_intv->to) {
    unsigned r = get_UIVec(env->result, spill);
    release_reg(env, spill);
    alloc_stack(env, spill);
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
      if (is_empty_UIDList(env->available->list)) {
        spill_at_interval(env, virtual);
      } else {
        alloc_reg(env, virtual);
      }
      break;
    case IV_FIXED: {
      unsigned u = get_UIVec(env->used_by, iv->fixed_real);
      if (u != -1) {
        unsigned blocked_virt = u;
        alloc_stack(env, blocked_virt);
        release_reg(env, blocked_virt);
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
  if (b->dead) {
    assign_reg_num(env, tail_BBList(l));
    return;
  }

  assign_reg_num_iter_insts(env, b->insts);
  b->sorted_insts = NULL;

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

  assign_reg_num(env, tail_BBList(l));
}

static void reg_alloc_function(unsigned num_regs, unsigned* global_inst_count, Function* ir) {
  RegIntervals* ivs    = ir->intervals;
  Env* env             = init_Env(ir, global_inst_count, num_regs);
  UIList* ordered_regs = sort_intervals(ivs);

  walk_regs(env, ordered_regs);

  release_UIList(ordered_regs);

  assign_reg_num(env, ir->blocks);

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

void reg_alloc(unsigned num_regs, IR* ir) {
  reg_alloc_functions(num_regs, &ir->inst_count, ir->functions);
}
