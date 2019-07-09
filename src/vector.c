#include "vector.h"

DEFINE_VECTOR(int, IntVec)

static void print_int(FILE* p, int i) { fprintf(p, "%d", i); }
DEFINE_VECTOR_PRINTER(print_int, ",", "\n", IntVec)
