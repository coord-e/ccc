#include "vector.h"

static void release_int(int i) {}
DEFINE_VECTOR(release_int, int, IntVec)

static void print_int(FILE* p, int i) {
  fprintf(p, "%d", i);
}
DEFINE_VECTOR_PRINTER(print_int, ",", "\n", IntVec)

static void release_unsigned(unsigned i) {}
DEFINE_VECTOR(release_unsigned, unsigned, UIVec)

static void print_unsigned(FILE* p, unsigned i) {
  fprintf(p, "%u", i);
}
DEFINE_VECTOR_PRINTER(print_unsigned, ",", "\n", UIVec)
