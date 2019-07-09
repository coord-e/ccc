#include "list.h"

DEFINE_LIST(int, IntList)

static void print_int(FILE* p, int i) { fprintf(p, "%d", i); }
DEFINE_LIST_PRINTER(print_int, ",", "\n", IntList)

