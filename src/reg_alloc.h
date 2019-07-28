#ifndef CCC_REG_ALLOC_H
#define CCC_REG_ALLOC_H

#include "ir.h"
#include "liveness.h"

IR* reg_alloc(unsigned num_regs, RegIntervals* ivs, IR* ir);

#endif
