#ifndef CCC_BIT_SET_H
#define CCC_BIT_SET_H

#include <stdbool.h>
#include <stdio.h>

typedef struct BitSet BitSet;

BitSet* new_BitSet(unsigned length);
unsigned length_BitSet(const BitSet*);

void or_BitSet(BitSet*, const BitSet*);
void and_BitSet(BitSet*, const BitSet*);

bool get_BitSet(const BitSet*, unsigned);
void set_BitSet(BitSet*, unsigned, bool);

BitSet* copy_BitSet(const BitSet*);

void print_BitSet(FILE*, const BitSet*);
void release_BitSet(BitSet*);

#endif
