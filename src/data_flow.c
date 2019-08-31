#include "data_flow.h"

static void collect_defs(Function*);
static void compute_local_live_sets(Function*);
static void compute_local_reach_sets(Function*);
static void compute_global_live_sets(Function*);
static void compute_global_reach_sets(Function*);

void live_data_flow(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);

    // compute `live_gen` and `live_kill`
    compute_local_live_sets(f);

    // compute `live_out` and `live_in`
    compute_global_live_sets(f);

    l = tail_FunctionList(l);
  }
}

void reach_data_flow(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);

    // compute `f->definitions`
    collect_defs(f);

    // compute `reach_gen` and `reach_kill`
    compute_local_reach_sets(f);

    // compute `reach_out` and `reach_in`
    compute_global_reach_sets(f);

    l = tail_FunctionList(l);
  }
}

static void collect_defs_insts(BSVec* defs, IRInstList* l) {
  while (!is_nil_IRInstList(l)) {
    IRInst* inst = head_IRInstList(l);
    if (inst->rd != NULL) {
      set_BitSet(get_BSVec(defs, inst->rd->virtual), inst->local_id, true);
    }
    l = tail_IRInstList(l);
  }
}

static void collect_defs(Function* f) {
  BSVec* defs = new_BSVec(f->reg_count);
  for (unsigned i = 0; i < f->reg_count; i++) {
    push_BSVec(defs, zero_BitSet(f->inst_count));
  }

  BBList* l = f->blocks;
  while (!is_nil_BBList(l)) {
    BasicBlock* b = head_BBList(l);
    collect_defs_insts(defs, b->insts);
    l = tail_BBList(l);
  }

  f->definitions = defs;
}

static void iter_insts_forward(BasicBlock* b, IRInstVec* insts) {
  for (unsigned j = 0; j < length_IRInstVec(insts); j++) {
    IRInst* inst = get_IRInstVec(insts, j);

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
  }
}

static void iter_insts_backward(BSVec* defs, BasicBlock* b, IRInstVec* insts) {
  for (unsigned ti = length_IRInstVec(insts); ti > 0; ti--) {
    IRInst* inst = get_IRInstVec(insts, ti - 1);
    unsigned id  = inst->local_id;

    if (inst->rd == NULL) {
      continue;
    }

    if (!get_BitSet(b->reach_kill, id)) {
      set_BitSet(b->reach_gen, id, true);
    }

    BitSet* kill = copy_BitSet(get_BSVec(defs, inst->rd->virtual));
    set_BitSet(kill, id, false);
    or_BitSet(b->reach_kill, kill);
    release_BitSet(kill);
  }
}

static void compute_local_live_sets(Function* ir) {
  BBVec* v = ir->sorted_blocks;
  for (unsigned i = length_BBVec(v); i > 0; i--) {
    BasicBlock* b = get_BBVec(v, i - 1);
    b->live_gen   = zero_BitSet(ir->reg_count);
    b->live_kill  = zero_BitSet(ir->reg_count);

    iter_insts_forward(b, b->sorted_insts);
  }
}

static void compute_local_reach_sets(Function* ir) {
  BBVec* v = ir->sorted_blocks;
  for (unsigned i = length_BBVec(v); i > 0; i--) {
    BasicBlock* b = get_BBVec(v, i - 1);
    b->reach_gen  = zero_BitSet(ir->inst_count);
    b->reach_kill = zero_BitSet(ir->inst_count);

    iter_insts_backward(ir->definitions, b, b->sorted_insts);
  }
}

// TODO: Organize these similar iteration algorithms
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

static void iter_preds(BasicBlock* b, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }

  BasicBlock* pre = head_BBList(l);
  if (pre->reach_out != NULL) {
    or_BitSet(b->reach_in, pre->reach_out);
  }

  iter_preds(b, tail_BBList(l));
}

static void compute_global_reach_sets(Function* ir) {
  BBVec* v = ir->sorted_blocks;

  // temporary vector to detect changes in `reach_out`
  BSVec* lasts = new_BSVec(length_BBVec(v));
  for (unsigned i = 0; i < length_BBVec(v); i++) {
    push_BSVec(lasts, zero_BitSet(ir->inst_count));
  }

  bool changed;
  bool is_first_loop = true;  // TODO: Improve the control flow
  do {
    // first two loops are performed unconditionally
    changed       = is_first_loop;
    is_first_loop = false;

    // straight order
    for (unsigned ti = length_BBVec(v); ti > 0; ti--) {
      unsigned i    = ti - 1;
      BasicBlock* b = get_BBVec(v, i);

      b->reach_out = zero_BitSet(ir->inst_count);
      if (b->reach_in == NULL) {
        b->reach_in = zero_BitSet(ir->inst_count);
      }

      iter_preds(b, b->preds);

      copy_to_BitSet(b->reach_out, b->reach_in);
      diff_BitSet(b->reach_out, b->reach_kill);
      or_BitSet(b->reach_out, b->reach_gen);

      BitSet* last = get_BSVec(lasts, i);
      changed      = changed || !equal_to_BitSet(b->reach_out, last);
      copy_to_BitSet(last, b->reach_out);
    }
  } while (changed);

  release_BSVec(lasts);
}
