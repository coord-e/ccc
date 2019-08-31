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

static RegIntervals* build_intervals(Function*);

static void liveness_functions(FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  Function* f  = head_FunctionList(l);
  f->intervals = build_intervals(f);

  liveness_functions(tail_FunctionList(l));
}

void liveness(IR* ir) {
  liveness_functions(ir->functions);
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
}

static void build_intervals_insts(RegIntervals* ivs, IRInstVec* v, unsigned block_from) {
  // reverse order
  for (unsigned ii = length_IRInstVec(v); ii > 0; ii--) {
    IRInst* inst = get_IRInstVec(v, ii - 1);

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
    unsigned block_from = get_IRInstVec(is, 0)->local_id;
    unsigned block_to   = get_IRInstVec(is, length_IRInstVec(is) - 1)->local_id;

    for (unsigned vi = 0; vi < length_BitSet(b->live_out); vi++) {
      if (get_BitSet(b->live_out, vi)) {
        Interval* iv = get_RegIntervals(ivs, vi);
        add_range(iv, block_from, block_to);
      }
    }

    build_intervals_insts(ivs, is, block_from);
  }

  return ivs;
}
