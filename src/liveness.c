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

RegIntervals* liveness(IR* ir) {
  compute_global_live_sets(ir);
  compute_local_live_sets(ir);
  return build_intervals(ir);
}
