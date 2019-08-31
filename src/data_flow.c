#include "data_flow.h"

static void compute_local_sets(Function*);
static void compute_global_live_sets(Function*);
static void compute_global_reach_sets(Function*);

DECLARE_VECTOR(BitSet*, BSVec)
DEFINE_VECTOR(release_BitSet, BitSet*, BSVec)

void data_flow(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);

    compute_local_sets(f);

    // compute `live_out` and `live_in`
    compute_global_live_sets(f);

    // compute `reach_out` and `reach_in`
    compute_global_reach_sets(f);

    l = tail_FunctionList(l);
  }
}

static void collect_defs_insts(BBVec* defs, IRInstList* l) {
  while (!is_nil_IRInstList(l)) {
    IRInst* inst = head_IRInstList(l);
    if (inst->rd != NULL) {
      set_BitSet(get_BBVec(defs, inst->rd->virtual), inst->id, true);
    }
    l = tail_IRInstList(l);
  }
}

static BBVec* collect_defs(Function* f) {
  BBVec* defs = new_BBVec(f->reg_count);
  for (unsigned i = 0; i < f->reg_count; i++) {
    push_BBVec(defs, zero_BitSet(f->inst_count));
  }

  BBList* l = f->blocks;
  while (!is_nil_BBList(l)) {
    BasicBlock* b = head_BBList(l);
    collect_defs_insts(defs, b->insts);
    l = tail_BBList(l);
  }

  return defs;
}

static void iter_insts(BBVec* defs, BasicBlock* b, IRInstList* l) {
  if (is_nil_IRInstList(l)) {
    return;
  }
  IRInst* inst = head_IRInstList(l);

  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* ra = get_RegVec(inst->ras, i);

    unsigned vi = ra->virtual;
    if (!get_BitSet(b->live_kill, vi)) {
      set_BitSet(b->live_gen, vi, true);
    }
  }

  if (inst->rd != NULL) {
    set_BitSet(b->live_kill, inst->rd->virtual, true);

    BitSet* kill = copy_BitSet(get_BBVec(defs, inst->rd->virtual));
    set_BitSet(kill, inst->id, false);
    or_BitSet(b->reach_kill, kill);
    release_BitSet(kill);

    // hmm??
  }

  iter_insts(defs, b, tail_IRInstList(l));
}

static void compute_local_sets(Function* ir) {
  BBVec* defs = collect_defs(ir);
  BBVec* v    = ir->sorted_blocks;
  for (unsigned i = length_BBVec(v); i > 0; i--) {
    BasicBlock* b = get_BBVec(v, i - 1);
    b->live_gen   = zero_BitSet(ir->reg_count);
    b->live_kill  = zero_BitSet(ir->reg_count);
    b->reach_gen  = zero_BitSet(ir->inst_count);
    b->reach_kill = zero_BitSet(ir->inst_count);

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

static void compute_global_live_sets(Function* ir) {
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
