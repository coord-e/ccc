#include "data_flow.h"

static void compute_local_live_sets(Function*);
static void compute_global_live_sets(Function*);

void data_flow(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);

    compute_local_live_sets(f);
    compute_global_live_sets(f);

    l = tail_FunctionList(l);
  }
}

static void iter_insts(BasicBlock* b, IRInstList* l) {
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
  }

  iter_insts(b, tail_IRInstList(l));
}

static void compute_local_live_sets(Function* ir) {
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
