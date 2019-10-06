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

static void collect_defs(Function* f) {
  BSVec* defs = new_BSVec(f->reg_count);
  for (unsigned i = 0; i < f->reg_count; i++) {
    push_BSVec(defs, zero_BitSet(f->inst_count));
  }

  for (IRInstListIterator* it = front_IRInstList(f->instructions); !is_nil_IRInstListIterator(it);
       it                     = next_IRInstListIterator(it)) {
    IRInst* inst = data_IRInstListIterator(it);
    if (inst->rd != NULL) {
      set_BitSet(get_BSVec(defs, inst->rd->virtual), inst->local_id, true);
    }
  }

  f->definitions = defs;
}

static void iter_insts_forward(BasicBlock* b, IRInstRange* insts) {
  for (IRInstRangeIterator* it = front_IRInstRange(insts); !is_nil_IRInstRangeIterator(it);
       it                      = next_IRInstRangeIterator(it)) {
    IRInst* inst = data_IRInstRangeIterator(it);

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

static void iter_insts_backward(BSVec* defs, BasicBlock* b, IRInstRange* insts) {
  for (IRInstRangeIterator* it = back_IRInstRange(insts); !is_nil_IRInstRangeIterator(it);
       it                      = prev_IRInstRangeIterator(it)) {
    IRInst* inst = data_IRInstRangeIterator(it);
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
  for (BBListIterator* it = front_BBList(ir->blocks); !is_nil_BBListIterator(it);
       it                 = next_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);

    b->live_gen  = zero_BitSet(ir->reg_count);
    b->live_kill = zero_BitSet(ir->reg_count);

    iter_insts_forward(b, b->instructions);
  }
}

static void compute_local_reach_sets(Function* ir) {
  for (BBListIterator* it = front_BBList(ir->blocks); !is_nil_BBListIterator(it);
       it                 = next_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);

    b->reach_gen  = zero_BitSet(ir->inst_count);
    b->reach_kill = zero_BitSet(ir->inst_count);

    iter_insts_backward(ir->definitions, b, b->instructions);
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
  // temporary vector to detect changes in `live_in`
  BSVec* lasts = new_BSVec(ir->bb_count);
  for (unsigned i = 0; i < ir->bb_count; i++) {
    push_BSVec(lasts, zero_BitSet(ir->reg_count));
  }

  for (BBListIterator* it = front_BBList(ir->blocks); !is_nil_BBListIterator(it);
       it                 = next_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);
    if (b->live_in != NULL) {
      release_BitSet(b->live_in);
    }
    b->live_in = zero_BitSet(ir->reg_count);
  }

  bool changed;
  bool is_first_loop = true;  // TODO: Improve the control flow
  do {
    // first two loops are performed unconditionally
    changed       = is_first_loop;
    is_first_loop = false;

    // reverse order
    for (BBListIterator* it = back_BBList(ir->blocks); !is_nil_BBListIterator(it);
         it                 = prev_BBListIterator(it)) {
      BasicBlock* b = data_BBListIterator(it);

      if (b->live_out != NULL) {
        release_BitSet(b->live_out);
      }
      b->live_out = zero_BitSet(ir->reg_count);

      iter_succs(b, front_BBRefList(b->succs));

      copy_to_BitSet(b->live_in, b->live_out);
      diff_BitSet(b->live_in, b->live_kill);
      or_BitSet(b->live_in, b->live_gen);

      BitSet* last = get_BSVec(lasts, b->local_id);
      changed      = changed || !equal_to_BitSet(b->live_in, last);
      copy_to_BitSet(last, b->live_in);
    }
  } while (changed);

  release_BSVec(lasts);
}

static void compute_inst_live_sets(Function* ir) {
  for (BBListIterator* it1 = front_BBList(ir->blocks); !is_nil_BBListIterator(it1);
       it1                 = next_BBListIterator(it1)) {
    BasicBlock* b = data_BBListIterator(it1);

    BitSet* live = copy_BitSet(b->live_out);
    for (IRInstRangeIterator* it2              = back_IRInstRange(b->instructions);
         !is_nil_IRInstRangeIterator(it2); it2 = prev_IRInstRangeIterator(it2)) {
      IRInst* inst = data_IRInstRangeIterator(it2);

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
  // temporary vector to detect changes in `reach_out`
  BSVec* lasts = new_BSVec(ir->bb_count);
  for (unsigned i = 0; i < ir->bb_count; i++) {
    push_BSVec(lasts, zero_BitSet(ir->inst_count));
  }

  for (BBListIterator* it = front_BBList(ir->blocks); !is_nil_BBListIterator(it);
       it                 = next_BBListIterator(it)) {
    BasicBlock* b = data_BBListIterator(it);
    if (b->reach_out != NULL) {
      release_BitSet(b->reach_out);
    }
    b->reach_out = zero_BitSet(ir->inst_count);
  }

  bool changed;
  bool is_first_loop = true;  // TODO: Improve the control flow
  do {
    // first two loops are performed unconditionally
    changed       = is_first_loop;
    is_first_loop = false;

    // straight order
    for (BBListIterator* it = front_BBList(ir->blocks); !is_nil_BBListIterator(it);
         it                 = next_BBListIterator(it)) {
      BasicBlock* b = data_BBListIterator(it);

      if (b->reach_in != NULL) {
        release_BitSet(b->reach_in);
      }
      b->reach_in = zero_BitSet(ir->inst_count);

      iter_preds(b, front_BBRefList(b->preds));

      copy_to_BitSet(b->reach_out, b->reach_in);
      diff_BitSet(b->reach_out, b->reach_kill);
      or_BitSet(b->reach_out, b->reach_gen);

      BitSet* last = get_BSVec(lasts, b->local_id);
      changed      = changed || !equal_to_BitSet(b->reach_out, last);
      copy_to_BitSet(last, b->reach_out);
    }
  } while (changed);

  release_BSVec(lasts);
}

static IRInst* obtain_definition(Function* f, BitSet* reach, Reg* r) {
  BitSet* defs = copy_BitSet(get_BSVec(f->definitions, r->virtual));
  and_BitSet(defs, reach);

  if (count_BitSet(defs) != 1) {
    return NULL;
  }
  for (unsigned i = 0; i < length_BitSet(defs); i++) {
    if (!get_BitSet(defs, i)) {
      continue;
    }

    IRInst* def = get_IRInstList(f->instructions, i);
    assert(def->rd->virtual == r->virtual);

    release_BitSet(defs);
    return def;
  }
  CCC_UNREACHABLE;
}

static void compute_one_defs(Function* f, IRInst* inst) {
  for (unsigned i = 0; i < length_RegVec(inst->ras); i++) {
    Reg* r     = get_RegVec(inst->ras, i);
    r->one_def = obtain_definition(f, inst->reach_in, r);
  }
}

static void compute_inst_reach_sets(Function* ir) {
  for (BBListIterator* it1 = front_BBList(ir->blocks); !is_nil_BBListIterator(it1);
       it1                 = next_BBListIterator(it1)) {
    BasicBlock* b = data_BBListIterator(it1);

    BitSet* reach = copy_BitSet(b->reach_in);
    for (IRInstRangeIterator* it2              = front_IRInstRange(b->instructions);
         !is_nil_IRInstRangeIterator(it2); it2 = next_IRInstRangeIterator(it2)) {
      IRInst* inst = data_IRInstRangeIterator(it2);

      if (inst->reach_in != NULL) {
        release_BitSet(inst->reach_in);
      }
      inst->reach_in = copy_BitSet(reach);

      compute_one_defs(ir, inst);

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
  }
}
