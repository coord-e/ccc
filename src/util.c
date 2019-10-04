#include <stdlib.h>
#include <string.h>

#include "util.h"

size_t strnlen(const char* s, size_t n) {
  char* cur = memchr(s, '\n', n);
  return cur ? cur - s : n;
}

char* strdup(const char* s) {
  size_t size = strlen(s) + 1;
  char* m     = malloc(size);
  strcpy(m, s);
  return m;
}

char* strndup(const char* s, size_t n) {
  size_t size = n + 1;
  char* m     = malloc(size);
  memcpy(m, s, n);
  m[n] = '\0';
  return m;
}

void release_dummy(void* p) {}
