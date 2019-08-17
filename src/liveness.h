#ifndef CCC_LIVENESS_H
#define CCC_LIVENESS_H

#include <stdio.h>

#include "ir.h"
#include "list.h"
#include "vector.h"

typedef enum {
  IV_UNSET,
  IV_VIRTUAL,
  IV_FIXED,
} IntervalKind;

typedef struct {
  IntervalKind kind;

  // -1 for undefined
  // TODO: stop using -1 to indicate undefined value
  unsigned from;
  unsigned to;

  unsigned fixed_real;  // for IV_FIXED
} Interval;

DECLARE_VECTOR(Interval*, RegIntervals)

void liveness(IR*);

void print_Intervals(FILE*, RegIntervals*);

#endif
