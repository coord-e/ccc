#ifndef CCC_DOUBLE_LIST_H
#define CCC_DOUBLE_LIST_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

// a doubly linked list
#define DECLARE_DLIST(T, Name)                                                                     \
  typedef struct Name Name;                                                                        \
  typedef struct Name##Iterator Name##Iterator;                                                    \
  Name* new_##Name();                                                                              \
  Name* single_##Name(T value);                                                                    \
  T data_##Name##Iterator(const Name##Iterator* iter);                                             \
  Name##Iterator* prev_##Name##Iterator(const Name##Iterator* iter);                               \
  Name##Iterator* next_##Name##Iterator(const Name##Iterator* iter);                               \
  bool is_nil_##Name##Iterator(const Name##Iterator* iter);                                        \
  void remove_##Name##Iterator(Name##Iterator* iter);                                              \
  Name##Iterator* insert_##Name##Iterator(Name##Iterator* iter, T value);                          \
  Name##Iterator* front_##Name(Name* list);                                                        \
  Name##Iterator* back_##Name(Name* list);                                                         \
  bool is_empty_##Name(Name* list);                                                                \
  T head_##Name(Name* list);                                                                       \
  T last_##Name(Name* list);                                                                       \
  void push_front_##Name(Name* list, T value);                                                     \
  void push_back_##Name(Name* list, T value);                                                      \
  Name* shallow_copy_##Name(const Name* list);                                                     \
  void release_##Name(Name* list);

#define DEFINE_DLIST(release_data, T, Name)                                                        \
  struct Name##Iterator {                                                                          \
    bool is_nil;                                                                                   \
    T data;                                                                                        \
    Name##Iterator* prev;                                                                          \
    Name##Iterator* next;                                                                          \
  };                                                                                               \
  struct Name {                                                                                    \
    Name##Iterator* init;                                                                          \
    Name##Iterator* last;                                                                          \
  };                                                                                               \
  Name##Iterator* new_##Name##Iterator(Name##Iterator* prev, Name##Iterator* next) {               \
    Name##Iterator* l = malloc(sizeof(Name##Iterator));                                            \
    l->is_nil         = false;                                                                     \
    l->prev           = prev;                                                                      \
    l->next           = next;                                                                      \
    return l;                                                                                      \
  }                                                                                                \
  void release_##Name##Iterator(Name##Iterator* it) {                                              \
    if (!it->is_nil) {                                                                             \
      release_data(it->data);                                                                      \
    }                                                                                              \
    free(it);                                                                                      \
  }                                                                                                \
  Name* new_##Name() {                                                                             \
    Name* l         = malloc(sizeof(Name));                                                        \
    l->init         = new_##Name##Iterator(NULL, NULL);                                            \
    l->init->is_nil = true;                                                                        \
    l->last         = new_##Name##Iterator(NULL, NULL);                                            \
    l->last->is_nil = true;                                                                        \
    l->init->next   = l->last;                                                                     \
    l->last->prev   = l->init;                                                                     \
    return l;                                                                                      \
  }                                                                                                \
  void push_front_##Name(Name* list, T value) {                                                    \
    Name##Iterator* l = new_##Name##Iterator(list->init, list->init->next);                        \
    l->is_nil         = false;                                                                     \
    l->data           = value;                                                                     \
    l->next->prev     = l;                                                                         \
    l->prev->next     = l;                                                                         \
  }                                                                                                \
  void push_back_##Name(Name* list, T value) {                                                     \
    Name##Iterator* l = new_##Name##Iterator(list->last->prev, list->last);                        \
    l->is_nil         = false;                                                                     \
    l->data           = value;                                                                     \
    l->next->prev     = l;                                                                         \
    l->prev->next     = l;                                                                         \
  }                                                                                                \
  Name* single_##Name(T value) {                                                                   \
    Name* l = new_##Name();                                                                        \
    push_front_##Name(l, value);                                                                   \
    return l;                                                                                      \
  }                                                                                                \
  T data_##Name##Iterator(const Name##Iterator* iter) {                                            \
    if (iter->is_nil) {                                                                            \
      error("data");                                                                               \
    }                                                                                              \
    return iter->data;                                                                             \
  }                                                                                                \
  Name##Iterator* prev_##Name##Iterator(const Name##Iterator* iter) {                              \
    if (iter->is_nil) {                                                                            \
      error("prev");                                                                               \
    }                                                                                              \
    return iter->prev;                                                                             \
  }                                                                                                \
  Name##Iterator* next_##Name##Iterator(const Name##Iterator* iter) {                              \
    if (iter->is_nil) {                                                                            \
      error("next");                                                                               \
    }                                                                                              \
    return iter->next;                                                                             \
  }                                                                                                \
  Name##Iterator* front_##Name(Name* list) { return list->init->next; }                            \
  Name##Iterator* back_##Name(Name* list) { return list->last->prev; }                             \
  bool is_nil_##Name##Iterator(const Name##Iterator* iter) { return iter->is_nil; }                \
  void remove_##Name##Iterator(Name##Iterator* iter) {                                             \
    if (iter->is_nil) {                                                                            \
      error("removing nil");                                                                       \
    }                                                                                              \
    iter->prev->next = iter->next;                                                                 \
    iter->next->prev = iter->prev;                                                                 \
    iter->prev       = NULL;                                                                       \
    iter->next       = NULL;                                                                       \
    release_##Name##Iterator(iter);                                                                \
  }                                                                                                \
  Name##Iterator* insert_##Name##Iterator(Name##Iterator* iter, T value) {                         \
    if (iter->prev == NULL) {                                                                      \
      error("inserting to the head");                                                              \
    }                                                                                              \
    Name##Iterator* it = new_##Name##Iterator(iter->prev, iter);                                   \
    it->is_nil         = false;                                                                    \
    it->data           = value;                                                                    \
    iter->prev->next   = it;                                                                       \
    iter->prev         = it;                                                                       \
    return it;                                                                                     \
  }                                                                                                \
  bool is_empty_##Name(Name* list) {                                                               \
    assert(list->init->next->is_nil == list->last->prev->is_nil);                                  \
    return list->init->next->is_nil;                                                               \
  }                                                                                                \
  T head_##Name(Name* list) {                                                                      \
    if (list->init->next->is_nil) {                                                                \
      error("head");                                                                               \
    }                                                                                              \
    return list->init->next->data;                                                                 \
  }                                                                                                \
  T last_##Name(Name* list) {                                                                      \
    if (list->last->prev->is_nil) {                                                                \
      error("last");                                                                               \
    }                                                                                              \
    return list->last->prev->data;                                                                 \
  }                                                                                                \
  Name* shallow_copy_##Name(const Name* list) {                                                    \
    Name* new          = new_##Name();                                                             \
    Name##Iterator* it = front_##Name((Name*)list);                                                \
    while (!is_nil_##Name##Iterator(it)) {                                                         \
      push_front_##Name(new, it->data);                                                            \
      it = next_##Name##Iterator(it);                                                              \
    }                                                                                              \
    return new;                                                                                    \
  }                                                                                                \
  void release_##Name(Name* list) {                                                                \
    Name##Iterator* it = front_##Name(list);                                                       \
    while (!is_nil_##Name##Iterator(it)) {                                                         \
      Name##Iterator* next = next_##Name##Iterator(it);                                            \
      release_##Name##Iterator(it);                                                                \
      it = next;                                                                                   \
    }                                                                                              \
    release_##Name##Iterator(list->init);                                                          \
    release_##Name##Iterator(list->last);                                                          \
  }

#endif
