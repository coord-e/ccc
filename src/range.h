#ifndef CCC_RANGE_H
#define CCC_RANGE_H

#include <assert.h>
#include <stdlib.h>

#define DECLARE_RANGE(T, TList, Name)                                                              \
  typedef struct {                                                                                 \
    TList##Iterator* from;                                                                         \
    TList##Iterator* to;                                                                           \
  } Name;                                                                                          \
  typedef TList##Iterator Name##Iterator;                                                          \
  Name* new_##Name(TList##Iterator* from, TList##Iterator* to);                                    \
  Name##Iterator* front_##Name(const Name* list);                                                  \
  Name##Iterator* back_##Name(const Name* list);                                                   \
  T head_##Name(const Name* list);                                                                 \
  T last_##Name(const Name* list);                                                                 \
  Name* copy_##Name(const Name* list);                                                             \
  void release_##Name(Name* list);

#define DEFINE_RANGE(T, TList, Name)                                                               \
  Name* new_##Name(TList##Iterator* from, TList##Iterator* to) {                                   \
    assert(!is_nil_##TList##Iterator(from));                                                       \
    assert(!is_nil_##TList##Iterator(to));                                                         \
    Name* new = malloc(sizeof(Name));                                                              \
    new->from = from;                                                                              \
    new->to   = to;                                                                                \
    return new;                                                                                    \
  }                                                                                                \
  Name##Iterator* front_##Name(const Name* list) {                                                 \
    assert(!is_nil_##TList##Iterator(list->from));                                                 \
    return list->from;                                                                             \
  }                                                                                                \
  Name##Iterator* back_##Name(const Name* list) {                                                  \
    assert(!is_nil_##TList##Iterator(list->to));                                                   \
    return list->to;                                                                               \
  }                                                                                                \
  T head_##Name(const Name* list) {                                                                \
    assert(!is_nil_##TList##Iterator(list->from));                                                 \
    return data_##TList##Iterator(list->from);                                                     \
  }                                                                                                \
  T last_##Name(const Name* list) {                                                                \
    assert(!is_nil_##TList##Iterator(list->to));                                                   \
    return data_##TList##Iterator(list->to);                                                       \
  }                                                                                                \
  Name* copy_##Name(const Name* list) { return new_##Name(list->from, list->to); }                 \
  void release_##Name(Name* list) { free(list); }

#endif
