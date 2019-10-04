#ifndef CCC_RANGE_H
#define CCC_RANGE_H

#include <assert.h>
#include <stdlib.h>

#define DECLARE_RANGE(T, TList, Name)                                                              \
  typedef struct {                                                                                 \
    TList##Iterator* from;                                                                         \
    TList##Iterator* to;                                                                           \
  } Name;                                                                                          \
  typedef struct Name##Iterator Name##Iterator;                                                    \
  Name* new_unchecked_##Name(TList##Iterator* from, TList##Iterator* to);                          \
  Name* new_##Name(TList##Iterator* from, TList##Iterator* to);                                    \
  Name##Iterator* front_##Name(const Name* list);                                                  \
  Name##Iterator* back_##Name(const Name* list);                                                   \
  T head_##Name(const Name* list);                                                                 \
  T last_##Name(const Name* list);                                                                 \
  T data_##Name##Iterator(const Name##Iterator* iter);                                             \
  Name##Iterator* prev_##Name##Iterator(const Name##Iterator* iter);                               \
  Name##Iterator* next_##Name##Iterator(const Name##Iterator* iter);                               \
  bool is_nil_##Name##Iterator(const Name##Iterator* iter);                                        \
  Name* copy_##Name(const Name* list);                                                             \
  void release_##Name(Name* list);

#define DEFINE_RANGE(T, TList, Name)                                                               \
  struct Name##Iterator {                                                                          \
    TList##Iterator* inner;                                                                        \
    Name* range;                                                                                   \
  };                                                                                               \
  static Name##Iterator* new_##Name##Iterator(TList##Iterator* inner, const Name* range) {         \
    Name##Iterator* it = malloc(sizeof(Name##Iterator));                                           \
    it->inner          = inner;                                                                    \
    it->range          = (Name*)range;                                                             \
    return it;                                                                                     \
  }                                                                                                \
  Name* new_unchecked_##Name(TList##Iterator* from, TList##Iterator* to) {                         \
    Name* new = malloc(sizeof(Name));                                                              \
    new->from = from;                                                                              \
    new->to   = to;                                                                                \
    return new;                                                                                    \
  }                                                                                                \
  Name* new_##Name(TList##Iterator* from, TList##Iterator* to) {                                   \
    assert(!is_nil_##TList##Iterator(from));                                                       \
    assert(!is_nil_##TList##Iterator(to));                                                         \
    return new_unchecked_##Name(from, to);                                                         \
  }                                                                                                \
  Name##Iterator* front_##Name(const Name* list) {                                                 \
    assert(!is_nil_##TList##Iterator(list->from));                                                 \
    return new_##Name##Iterator(list->from, list);                                                 \
  }                                                                                                \
  Name##Iterator* back_##Name(const Name* list) {                                                  \
    assert(!is_nil_##TList##Iterator(list->to));                                                   \
    return new_##Name##Iterator(list->to, list);                                                   \
  }                                                                                                \
  T head_##Name(const Name* list) {                                                                \
    assert(!is_nil_##TList##Iterator(list->from));                                                 \
    return data_##TList##Iterator(list->from);                                                     \
  }                                                                                                \
  T last_##Name(const Name* list) {                                                                \
    assert(!is_nil_##TList##Iterator(list->to));                                                   \
    return data_##TList##Iterator(list->to);                                                       \
  }                                                                                                \
  T data_##Name##Iterator(const Name##Iterator* iter) {                                            \
    return data_##TList##Iterator(iter->inner);                                                    \
  }                                                                                                \
  Name##Iterator* prev_##Name##Iterator(const Name##Iterator* iter) {                              \
    return new_##Name##Iterator(prev_##TList##Iterator(iter->inner), iter->range);                 \
  }                                                                                                \
  Name##Iterator* next_##Name##Iterator(const Name##Iterator* iter) {                              \
    return new_##Name##Iterator(next_##TList##Iterator(iter->inner), iter->range);                 \
  }                                                                                                \
  bool is_nil_##Name##Iterator(const Name##Iterator* iter) {                                       \
    return iter->inner == prev_##TList##Iterator(iter->range->from) ||                             \
           iter->inner == next_##TList##Iterator(iter->range->to);                                 \
  }                                                                                                \
  Name* copy_##Name(const Name* list) { return new_##Name(list->from, list->to); }                 \
  void release_##Name(Name* list) { free(list); }

#endif
