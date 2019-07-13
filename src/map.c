#include "map.h"

DECLARE_MAP(int, IntMap)

static void release_int(int i) {}
DEFINE_MAP(release_int, int, IntMap)

unsigned hash_string(const char* s) {
  unsigned hash = 0;
  char c;

  while ((c = *s++)) {
    hash = c + (hash << 6) + (hash << 16) - hash;
  }

  return hash;
}
