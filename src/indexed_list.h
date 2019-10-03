#ifndef CCC_INDEXED_LIST_H
#define CCC_INDEXED_LIST_H

#include <stdbool.h>
#include <stdlib.h>

#include "double_list.h"

#define DECLARE_INDEXED_LIST(T, Name)                                                              \
  typedef struct Name Name;                                                                        \
  typedef struct Name##ListIterator Name##Iterator;                                                \
  Name* new_##Name(unsigned size);                                                                 \
  T data_##Name##Iterator(const Name##Iterator* iter);                                             \
  T get_##Name(const Name*, unsigned idx);                                                         \
  Name##Iterator* get_iterator_##Name(const Name*, unsigned idx);                                  \
  Name##Iterator* prev_##Name##Iterator(const Name##Iterator* iter);                               \
  Name##Iterator* next_##Name##Iterator(const Name##Iterator* iter);                               \
  bool is_nil_##Name##Iterator(const Name##Iterator* iter);                                        \
  Name##Iterator* remove_##Name##Iterator(Name*, unsigned idx, Name##Iterator* iter);              \
  Name##Iterator* remove_by_idx_##Name##Iterator(Name*, unsigned idx);                             \
  Name##Iterator* insert_##Name##Iterator(Name*, unsigned idx, Name##Iterator* iter, T value);     \
  Name##Iterator* front_##Name(Name* list);                                                        \
  Name##Iterator* back_##Name(Name* list);                                                         \
  bool is_empty_##Name(Name* list);                                                                \
  T head_##Name(Name* list);                                                                       \
  T last_##Name(Name* list);                                                                       \
  void push_front_##Name(Name* list, unsigned idx, T value);                                       \
  void push_back_##Name(Name* list, unsigned idx, T value);                                        \
  Name* shallow_copy_##Name(const Name* list);                                                     \
  void release_##Name(Name* list);

#define DEFINE_INDEXED_LIST(release_data, T, Name)                                                 \
  DECLARE_DLIST(T, Name##List)                                                                     \
  DEFINE_DLIST(release_data, T, Name##List)                                                        \
  DECLARE_VECTOR(Name##ListIterator*, Name##IterRefVec)                                            \
  static void release_dummy(void* d) {}                                                            \
  DEFINE_VECTOR(release_dummy, Name##ListIterator*, Name##IterRefVec)                              \
  struct Name {                                                                                    \
    Name##List* list;                                                                              \
    Name##IterRefVec* iterators;                                                                   \
  };                                                                                               \
  Name* new_##Name(unsigned size) {                                                                \
    Name* l      = malloc(sizeof(Name));                                                           \
    l->list      = new_##Name##List();                                                             \
    l->iterators = new_##Name##IterRefVec(size);                                                   \
    resize_##Name##IterRefVec(l->iterators, size);                                                 \
    fill_##Name##IterRefVec(l->iterators, NULL);                                                   \
    return l;                                                                                      \
  }                                                                                                \
  void push_front_##Name(Name* list, unsigned idx, T value) {                                      \
    assert(get_##Name##IterRefVec(list->iterators, idx) == NULL);                                  \
    push_front_##Name##List(list->list, value);                                                    \
    set_##Name##IterRefVec(list->iterators, idx, front_##Name##List(list->list));                  \
  }                                                                                                \
  void push_back_##Name(Name* list, unsigned idx, T value) {                                       \
    assert(get_##Name##IterRefVec(list->iterators, idx) == NULL);                                  \
    push_back_##Name##List(list->list, value);                                                     \
    set_##Name##IterRefVec(list->iterators, idx, back_##Name##List(list->list));                   \
  }                                                                                                \
  T data_##Name##Iterator(const Name##Iterator* iter) { return data_##Name##ListIterator(iter); }  \
  Name##Iterator* prev_##Name##Iterator(const Name##Iterator* iter) {                              \
    return prev_##Name##ListIterator(iter);                                                        \
  }                                                                                                \
  Name##Iterator* next_##Name##Iterator(const Name##Iterator* iter) {                              \
    return next_##Name##ListIterator(iter);                                                        \
  }                                                                                                \
  Name##Iterator* front_##Name(Name* list) { return front_##Name##List(list->list); }              \
  Name##Iterator* back_##Name(Name* list) { return back_##Name##List(list->list); }                \
  bool is_nil_##Name##Iterator(const Name##Iterator* iter) {                                       \
    return is_nil_##Name##ListIterator(iter);                                                      \
  }                                                                                                \
  Name##Iterator* remove_##Name##Iterator(Name* list, unsigned idx, Name##Iterator* iter) {        \
    assert(get_##Name##IterRefVec(list->iterators, idx) != NULL);                                  \
    Name##Iterator* next = remove_##Name##ListIterator(iter);                                      \
    set_##Name##IterRefVec(list->iterators, idx, NULL);                                            \
    return next;                                                                                   \
  }                                                                                                \
  Name##Iterator* insert_##Name##Iterator(Name* list, unsigned idx, Name##Iterator* iter,          \
                                          T value) {                                               \
    assert(get_##Name##IterRefVec(list->iterators, idx) == NULL);                                  \
    Name##ListIterator* new = insert_##Name##ListIterator(iter, value);                            \
    set_##Name##IterRefVec(list->iterators, idx, new);                                             \
    return new;                                                                                    \
  }                                                                                                \
  Name##Iterator* get_iterator_##Name(const Name* list, unsigned idx) {                            \
    return get_##Name##IterRefVec(list->iterators, idx);                                           \
  }                                                                                                \
  T get_##Name(const Name* list, unsigned idx) {                                                   \
    Name##Iterator* it = get_##Name##IterRefVec(list->iterators, idx);                             \
    assert(it != NULL);                                                                            \
    return data_##Name##ListIterator(it);                                                          \
  }                                                                                                \
  Name##Iterator* remove_by_idx_##Name##Iterator(Name* list, unsigned idx) {                       \
    Name##Iterator* it = get_##Name##IterRefVec(list->iterators, idx);                             \
    assert(it != NULL);                                                                            \
    return remove_##Name##Iterator(list, idx, it);                                                 \
  }                                                                                                \
  bool is_empty_##Name(Name* list) { return is_empty_##Name##List(list->list); }                   \
  T head_##Name(Name* list) { return head_##Name##List(list->list); }                              \
  T last_##Name(Name* list) { return last_##Name##List(list->list); }                              \
  Name* shallow_copy_##Name(const Name* list) {                                                    \
    Name* l      = malloc(sizeof(Name));                                                           \
    l->list      = shallow_copy_##Name##List(list->list);                                          \
    l->iterators = /*shallow_*/ copy_##Name##IterRefVec(list->iterators);                          \
    return l;                                                                                      \
  }                                                                                                \
  void release_##Name(Name* list) {                                                                \
    release_##Name##List(list->list);                                                              \
    release_##Name##IterRefVec(list->iterators);                                                   \
    free(list);                                                                                    \
  }

#endif
