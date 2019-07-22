#include <string.h>

#include "map.h"

unsigned hash_string(const char* s) {
  unsigned hash = 0;
  char c;

  while ((c = *s++)) {
    hash = c + (hash << 6) + (hash << 16) - hash;
  }

  return hash;
}
