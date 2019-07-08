#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "error.h"

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "ccc error: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}
