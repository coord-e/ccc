#include "list.h"

static void release_int(int i) {}
DEFINE_LIST(release_int, int, IntList)

static void print_int(FILE* p, int i) {
  fprintf(p, "%d", i);
}
DEFINE_LIST_PRINTER(print_int, ",", "\n", IntList)
