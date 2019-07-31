#include "liveness.h"

static void release_Interval(Interval* iv) {
  free(iv);
}
DEFINE_VECTOR(release_Interval, Interval*, RegIntervals)

static void print_Interval(FILE* p, Interval* iv) {
  fprintf(p, "[%d, %d]", iv->from, iv->to);
}

void print_Intervals(FILE* p, RegIntervals* v) {
  for (unsigned i = 0; i < length_RegIntervals(v); i++) {
    Interval* iv = get_RegIntervals(v, i);
    fprintf(p, "%d: ", i);
    print_Interval(p, iv);
    fputs("\n", p);
  }
}

static void compute_local_live_sets(Function*);
static void compute_global_live_sets(Function*);
static RegIntervals* build_intervals(Function*);

static void liveness_functions(FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  Function* f = head_FunctionList(l);
  compute_local_live_sets(f);
  compute_global_live_sets(f);

  f->intervals = build_intervals(f);

  liveness_functions(tail_FunctionList(l));
}

void liveness(IR* ir) {
  liveness_functions(ir);
}

static void iter_insts(BasicBlock* b, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }
  IRInst* inst = head_IRInstList(l);

  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg ra = get_RegVec(inst->ras, i);
    assert(ra.is_used);

    unsigned vi = ra.virtual;
    if (!get_BitSet(b->live_kill, vi)) {
      set_BitSet(b->live_gen, vi, true);
    }
  }

  if (inst->rd.is_used) {
    set_BitSet(b->live_kill, inst->rd.virtual, true);
  }

  iter_insts(b, tail_IRInstList(l));
}

void compute_local_live_sets(Function* ir) {
  BBVec* v = ir->sorted_blocks;
  for (unsigned i = length_BBVec(v); i > 0; i--) {
    BasicBlock* b = get_BBVec(v, i - 1);
    b->live_gen   = zero_BitSet(ir->reg_count);
    b->live_kill  = zero_BitSet(ir->reg_count);

    iter_insts(b, b->insts);
  }
}

static void iter_succs(BasicBlock* b, BBList* l) {
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

void compute_global_live_sets(Function* ir) {
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

static Interval* new_interval(unsigned from, unsigned to) {
  Interval* iv = calloc(1, sizeof(Interval));
  iv->from     = from;
  iv->to       = to;
  return iv;
}

static void add_range(RegIntervals* ivs, unsigned virtual, unsigned from, unsigned to) {
  Interval* iv = get_RegIntervals(ivs, virtual);
  if (iv->from == -1 || iv->from > from) {
    iv->from = from;
  }
  if (iv->to == -1 || iv->to < to) {
    iv->to = to;
  }
}

static void set_from(RegIntervals* ivs, unsigned virtual, unsigned from) {
  Interval* iv = get_RegIntervals(ivs, virtual);
  iv->from     = from;
}

static void build_intervals_insts(RegIntervals* ivs, IRInstVec* v, unsigned block_from) {
  // reverse order
  for (unsigned ii = length_IRInstVec(v); ii > 0; ii--) {
    IRInst* inst = get_IRInstVec(v, ii - 1);

    for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
      Reg ra = get_RegVec(inst->ras, i);
      assert(ra.is_used);

      add_range(ivs, ra.virtual, block_from, inst->id);
    }

    if (inst->rd.is_used) {
      set_from(ivs, inst->rd.virtual, inst->id);
    }
  }
}

RegIntervals* build_intervals(Function* ir) {
  BBVec* v = ir->sorted_blocks;

  RegIntervals* ivs = new_RegIntervals(ir->reg_count);
  for (unsigned i = 0; i < ir->reg_count; i++) {
    push_RegIntervals(ivs, new_interval(-1, -1));
  }

  // reverse order
  for (unsigned i = 0; i < length_BBVec(v); i++) {
    BasicBlock* b       = get_BBVec(v, i);
    IRInstVec* is       = b->sorted_insts;
    unsigned block_from = get_IRInstVec(is, 0)->id;
    unsigned block_to   = get_IRInstVec(is, length_IRInstVec(is) - 1)->id;

    for (unsigned vi = 0; vi < length_BitSet(b->live_out); vi++) {
      if (get_BitSet(b->live_out, vi)) {
        add_range(ivs, vi, block_from, block_to);
      }
    }

    build_intervals_insts(ivs, is, block_from);
  }

  return ivs;
}
