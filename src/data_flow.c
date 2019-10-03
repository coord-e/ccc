#include "data_flow.h"

static void collect_defs(Function*);
static void compute_local_live_sets(Function*);
static void compute_local_reach_sets(Function*);
static void compute_global_live_sets(Function*);
static void compute_global_reach_sets(Function*);
static void compute_inst_live_sets(Function*);
static void compute_inst_reach_sets(Function*);

void live_data_flow(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);

    // compute `live_gen` and `live_kill` in `BasicBlock`
    compute_local_live_sets(f);

    // compute `live_out` and `live_in` in `BasicBlock`
    compute_global_live_sets(f);

    // compute `live_out` and `live_in` in `IRInst`
    compute_inst_live_sets(f);

    l = tail_FunctionList(l);
  }
}

void reach_data_flow(IR* ir) {
  FunctionList* l = ir->functions;
  while (!is_nil_FunctionList(l)) {
    Function* f = head_FunctionList(l);

    // compute `f->definitions`
    collect_defs(f);

    // compute `reach_gen` and `reach_kill` in `BasicBlock`
    compute_local_reach_sets(f);

    // compute `reach_out` and `reach_in` in `BasicBlock`
    compute_global_reach_sets(f);

    // compute `reach_out` and `reach_in` in `IRInst`
    compute_inst_reach_sets(f);

    l = tail_FunctionList(l);
  }
}

static void collect_defs_insts(BSVec* defs, IRInstListIterator* it) {
  while (!is_nil_IRInstListIterator(it)) {
    IRInst* inst = data_IRInstListIterator(it);
    if (inst->rd != NULL) {
      set_BitSet(get_BSVec(defs, inst->rd->virtual), inst->local_id, true);
    }
    it = next_IRInstListIterator(it);
  }
}

static void collect_defs(Function* f) {
  BSVec* defs = new_BSVec(f->reg_count);
  for (unsigned i = 0; i < f->reg_count; i++) {
    push_BSVec(defs, zero_BitSet(f->inst_count));
  }

  BBListIterator* it = front_BBList(f->blocks);
  while (!is_nil_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);
    collect_defs_insts(defs, front_IRInstList(b->insts));
    it = next_BBListIterator(it);
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
static void iter_succs(BasicBlock* b, BBRefListIterator* l) {
  if (is_nil_BBRefListIterator(l)) {
    return;
  }

  BasicBlock* sux = data_BBRefListIterator(l);
  if (sux->live_in != NULL) {
    or_BitSet(b->live_out, sux->live_in);
  }

  iter_succs(b, next_BBRefListIterator(l));
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

      iter_succs(b, front_BBRefList(b->succs));

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

static void compute_inst_live_sets(Function* ir) {
  BBListIterator* it = front_BBList(ir->blocks);
  while (!is_nil_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);

    BitSet* live = copy_BitSet(b->live_out);
    for (unsigned j = length_IRInstVec(b->sorted_insts); j > 0; j--) {
      IRInst* inst = get_IRInstVec(b->sorted_insts, j - 1);

      if (inst->live_out != NULL) {
        release_BitSet(inst->live_out);
      }
      inst->live_out = copy_BitSet(live);

      if (inst->rd != NULL) {
        set_BitSet(live, inst->rd->virtual, false);
      }
      for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
        Reg* ra = get_RegVec(inst->ras, i);
        set_BitSet(live, ra->virtual, true);
      }

      if (inst->live_in != NULL) {
        release_BitSet(inst->live_in);
      }
      inst->live_in = copy_BitSet(live);
    }
    assert(equal_to_BitSet(b->live_in, live));

    it = next_BBListIterator(it);
  }
}

static void iter_preds(BasicBlock* b, BBRefListIterator* l) {
  if (is_nil_BBRefListIterator(l)) {
    return;
  }

  BasicBlock* pre = data_BBRefListIterator(l);
  if (pre->reach_out != NULL) {
    or_BitSet(b->reach_in, pre->reach_out);
  }

  iter_preds(b, next_BBRefListIterator(l));
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

      iter_preds(b, front_BBRefList(b->preds));

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

static void compute_inst_reach_sets(Function* ir) {
  BBListIterator* it = front_BBList(ir->blocks);
  while (!is_nil_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);

    BitSet* reach = copy_BitSet(b->reach_in);
    for (unsigned j = 0; j < length_IRInstVec(b->sorted_insts); j++) {
      IRInst* inst = get_IRInstVec(b->sorted_insts, j);

      if (inst->reach_in != NULL) {
        release_BitSet(inst->reach_in);
      }
      inst->reach_in = copy_BitSet(reach);

      if (inst->rd == NULL) {
        continue;
      }
      unsigned id  = inst->local_id;
      BitSet* defs = copy_BitSet(get_BSVec(ir->definitions, inst->rd->virtual));
      set_BitSet(defs, id, false);
      diff_BitSet(reach, defs);
      set_BitSet(reach, id, true);
      release_BitSet(defs);

      if (inst->reach_out != NULL) {
        release_BitSet(inst->reach_out);
      }
      inst->reach_out = copy_BitSet(reach);
    }
    assert(equal_to_BitSet(b->reach_out, reach));

    it = next_BBListIterator(it);
  }
}
