#ifndef CCC_LIVENESS_H
#define CCC_LIVENESS_H

#include "ir.h"
#include "list.h"
#include "vector.h"

typedef struct {
  unsigned from;
  unsigned to;
} Range;

DECLARE_LIST(Range*, RangeList)

typedef struct {
  RangeList* ranges;
} Interval;

DECLARE_VECTOR(Interval*, RegIntervals)

RegIntervals* liveness(IR*);

#endif
