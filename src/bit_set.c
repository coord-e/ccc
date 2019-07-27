#include "bit_set.h"
#include "vector.h"

#include <stdint.h>

DECLARE_VECTOR(uint64_t, U64Vec)
static void release_u64(uint64_t l) {}
DEFINE_VECTOR(release_u64, uint64_t, U64Vec)

struct BitSet {
  U64Vec* data;
  unsigned length;
};

const size_t block_size = sizeof(uint64_t);

static BitSet* init_BitSet() {
  return calloc(1, sizeof(BitSet));
}

BitSet* new_BitSet(unsigned length) {
  BitSet* s     = init_BitSet();
  unsigned size = (length + block_size - 1) / block_size;
  s->data       = new_U64Vec(size);
  resize_U64Vec(s->data, size);
  s->length = length;
  return s;
}

unsigned length_BitSet(const BitSet* s) {
  return s->length;
}

void or_BitSet(BitSet* s1, const BitSet* s2) {
  assert(s1->length == s2->length);
  for (unsigned i = 0; i < length_U64Vec(s1->data); i++) {
    uint64_t d1 = get_U64Vec(s1->data, i);
    uint64_t d2 = get_U64Vec(s2->data, i);
    set_U64Vec(s1->data, i, d1 | d2);
  }
}

void and_BitSet(BitSet* s1, const BitSet* s2) {
  assert(s1->length == s2->length);
  for (unsigned i = 0; i < length_U64Vec(s1->data); i++) {
    uint64_t d1 = get_U64Vec(s1->data, i);
    uint64_t d2 = get_U64Vec(s2->data, i);
    set_U64Vec(s1->data, i, d1 & d2);
  }
}

bool get_BitSet(const BitSet* s, unsigned idx) {
  uint64_t data = get_U64Vec(s->data, idx / block_size);
  unsigned pos  = idx % block_size;
  return (data >> pos) & UINT64_C(1);
}

void set_BitSet(BitSet* s, unsigned idx, bool b) {
  uint64_t data = get_U64Vec(s->data, idx / block_size);
  unsigned pos  = idx % block_size;
  if (b) {
    data |= UINT64_C(1) << pos;
  } else {
    data &= ~(UINT64_C(1) << pos);
  }
  set_U64Vec(s->data, idx / block_size, data);
}

BitSet* copy_BitSet(const BitSet* s) {
  BitSet* new = init_BitSet();
  new->length = s->length;
  new->data   = copy_U64Vec(s->data);
  return new;
}

void print_BitSet(FILE* p, const BitSet* s) {
  fputs("{", p);
  for (unsigned i = 0; i < s->length; i++) {
    if (get_BitSet(s, i)) {
      fprintf(p, "%d, ", i);
    }
  }
  fputs("}", p);
}

void release_BitSet(BitSet* s) {
  if (s == NULL) {
    return;
  }

  release_U64Vec(s->data);
  free(s);
}
