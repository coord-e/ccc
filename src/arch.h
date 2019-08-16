#ifndef CCC_ARCH_H
#define CCC_ARCH_H

#include "ir.h"

// convert IR into target-specific form
void arch(IR*);

extern const size_t num_regs;
extern const unsigned rax_reg_id;
unsigned nth_arg_id(unsigned);

extern const char* regs8[];
extern const char* regs16[];
extern const char* regs32[];
extern const char* regs64[];

#endif
