#ifndef CCC_LIVENESS_H
#define CCC_LIVENESS_H

#include <stdio.h>

#include "ir.h"
#include "list.h"
#include "vector.h"

typedef struct {
  unsigned from;
  unsigned to;
} Interval;

DECLARE_VECTOR(Interval*, RegIntervals)

RegIntervals* liveness(IR*);

void print_Intervals(FILE*, RegIntervals*);

#endif
