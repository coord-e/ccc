#include "liveness.h"

static void release_Range(Range* r) {
  free(r);
}
DEFINE_LIST(release_Range, Range*, RangeList)

static void release_Interval(Interval* iv) {
  if (iv == NULL) {
    return;
  }

  release_RangeList(iv->ranges);
  free(iv);
}
DEFINE_VECTOR(release_Interval, Interval*, RegIntervals)

static void compute_local_live_sets(IR*);
static void compute_global_live_sets(IR*);
static RegIntervals* build_intervals(IR*);

RegIntervals* liveness(IR* ir) {
  compute_local_live_sets(ir);
  compute_global_live_sets(ir);
  return build_intervals(ir);
}

void iter_insts(BasicBlock* b, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }
  IRInst* inst = head_IRInstList(l);

  if (inst->rd.is_used) {
    set_BitSet(b->live_kill, inst->rd.virtual, true);
  }

  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg ra = get_RegVec(inst->ras, i);
    assert(ra.is_used);

    unsigned vi = ra.virtual;
    if (!get_BitSet(b->live_kill, vi)) {
      set_BitSet(b->live_gen, vi, true);
    }
  }

  iter_insts(b, tail_IRInstList(l));
}

void compute_local_live_sets(IR* ir) {
  BBVec* v = ir->sorted_blocks;
  for (unsigned i = length_BBVec(v); i > 0; i--) {
    BasicBlock* b = get_BBVec(v, i - 1);
    b->live_gen   = zero_BitSet(ir->reg_count);
    b->live_kill  = zero_BitSet(ir->reg_count);

    iter_insts(b, b->insts);
  }
}

void iter_succs(BasicBlock* b, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }

  BasicBlock* sux = head_BBList(l);
  if (sux->live_in != NULL) {
    or_BitSet(b->live_out, sux->live_in);
  }

  iter_succs(b, tail_BBList(l));
}

DECLARE_VECTOR(BitSet*, BSVec)
DEFINE_VECTOR(release_BitSet, BitSet*, BSVec)

void compute_global_live_sets(IR* ir) {
  BBVec* v = ir->sorted_blocks;

  // temporary vector to detect changes in `live_in`
  BSVec* lasts = new_BSVec(length_BBVec(v));
  for (unsigned i = 0; i < length_BBVec(v); i++) {
    push_BSVec(lasts, zero_BitSet(ir->reg_count));
  }

  bool changed;
  bool is_first_loop = true;  // TODO: Improve the control flow
  do {
    // first two loops are performed unconditionally
    changed       = is_first_loop;
    is_first_loop = false;

    // reverse order
    for (unsigned i = 0; i < length_BBVec(v); i++) {
      BasicBlock* b = get_BBVec(v, i);

      b->live_out = zero_BitSet(ir->reg_count);
      if (b->live_in == NULL) {
        b->live_in = zero_BitSet(ir->reg_count);
      }

      iter_succs(b, b->succs);

      copy_to_BitSet(b->live_in, b->live_out);
      diff_BitSet(b->live_in, b->live_kill);
      or_BitSet(b->live_in, b->live_gen);

      BitSet* last = get_BSVec(lasts, i);
      changed      = changed || !equal_to_BitSet(b->live_in, last);
      copy_to_BitSet(last, b->live_in);
    }
  } while (changed);

  release_BSVec(lasts);
}

RegIntervals* build_intervals(IR* ir) {
  return NULL;
}
