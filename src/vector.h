#ifndef CCC_VECTOR_H
#define CCC_VECTOR_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

#define DECLARE_VECTOR(T, Name)                                                \
  typedef struct Name Name;                                                    \
  Name *new_##Name(unsigned capacity);                                         \
  T get_##Name(const Name *, unsigned idx);                                    \
  void push_##Name(Name *, T value);                                           \
  void set_##Name(Name *, unsigned idx, T value);                              \
  unsigned length_##Name(const Name *);                                        \
  unsigned capacity_##Name(const Name *);                                      \
  void resize_##Name(Name *, unsigned size);                                   \
  void reserve_##Name(Name *, unsigned size);                                  \
  T *data_##Name(Name *);                                                      \
  void release_##Name(Name *);

#define DEFINE_VECTOR(T, Name)                                                 \
  struct Name {                                                                \
    T *data;                                                                   \
    unsigned capacity;                                                         \
    unsigned length;                                                           \
  };                                                                           \
  Name *new_##Name(unsigned c) {                                               \
    Name *v = calloc(1, sizeof(Name));                                         \
    v->data = calloc(c, sizeof(T));                                            \
    v->capacity = c;                                                           \
    v->length = 0;                                                             \
    return v;                                                                  \
  }                                                                            \
  T get_##Name(const Name *a, unsigned idx) {                                  \
    assert(a->length > idx);                                                   \
    return a->data[idx];                                                       \
  }                                                                            \
  void set_##Name(Name *a, unsigned idx, T value) {                            \
    assert(a->length > idx);                                                   \
    a->data[idx] = value;                                                      \
  }                                                                            \
  void reserve_##Name(Name *v, unsigned size) {                                \
    v->capacity = size;                                                        \
    v->data = realloc(v->data, sizeof(T) * size);                              \
  }                                                                            \
  void resize_##Name(Name *v, unsigned size) {                                 \
    if (v->capacity < size) {                                                  \
      reserve_##Name(v, size);                                                 \
    }                                                                          \
    v->length = size;                                                          \
  }                                                                            \
  void push_##Name(Name *a, T value) {                                         \
    if (a->length == a->capacity) {                                            \
      reserve_##Name(a, a->capacity * 2);                                      \
    }                                                                          \
    a->data[a->length++] = value;                                              \
  }                                                                            \
  unsigned length_##Name(const Name *a) { return a->length; }                  \
  unsigned capacity_##Name(const Name *a) { return a->capacity; }              \
  T *data_##Name(Name *a) { return a->data; }                                  \
  void release_##Name(Name *a) {                                               \
    free(a->data);                                                             \
    free(a);                                                                   \
  }

#endif
