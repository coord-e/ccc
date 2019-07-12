#include "vector.h"

static void release_int(int i) {}
DEFINE_VECTOR(release_int, int, IntVec)

static void print_int(FILE* p, int i) {
  fprintf(p, "%d", i);
}
DEFINE_VECTOR_PRINTER(print_int, ",", "\n", IntVec)
