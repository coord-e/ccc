#ifndef CCC_LIST_H
#define CCC_LIST_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

// a simple linked list
#define DECLARE_LIST(T, Name)                                                                      \
  typedef struct Name Name;                                                                        \
  Name* nil_##Name();                                                                              \
  Name* cons_##Name(T value, const Name* list);                                                    \
  Name* single_##Name(T value);                                                                    \
  Name* append_##Name(Name* a, const Name* b);                                                     \
  Name* snoc_##Name(T value, Name* list);                                                          \
  T head_##Name(const Name* list);                                                                 \
  Name* tail_##Name(const Name* list);                                                             \
  T last_##Name(const Name* list);                                                                 \
  bool is_nil_##Name(const Name* list);                                                            \
  unsigned length_##Name(const Name* list);                                                        \
  void remove_##Name(Name* list);                                                                  \
  void insert_##Name(T value, Name* list);                                                         \
  void release_##Name(Name* list);

#define DEFINE_LIST(release_data, T, Name)                                                         \
  struct Name {                                                                                    \
    bool is_nil;                                                                                   \
    T head;                                                                                        \
    Name* tail;                                                                                    \
  };                                                                                               \
  Name* init_##Name() {                                                                            \
    Name* l   = malloc(sizeof(Name));                                                              \
    l->is_nil = false;                                                                             \
    l->tail   = NULL;                                                                              \
    return l;                                                                                      \
  }                                                                                                \
  Name* nil_##Name() {                                                                             \
    Name* l   = init_##Name();                                                                     \
    l->is_nil = true;                                                                              \
    l->tail   = NULL;                                                                              \
    return l;                                                                                      \
  }                                                                                                \
  Name* cons_##Name(T value, const Name* list) {                                                   \
    Name* l   = init_##Name(value);                                                                \
    l->is_nil = false;                                                                             \
    l->head   = value;                                                                             \
    l->tail   = (Name*)list;                                                                       \
    return l;                                                                                      \
  }                                                                                                \
  Name* single_##Name(T value) { return cons_##Name(value, nil_##Name()); }                        \
  Name* append_##Name(Name* a, const Name* b) {                                                    \
    if (a->is_nil) {                                                                               \
      *a = *b;                                                                                     \
      return a;                                                                                    \
    } else {                                                                                       \
      return append_##Name(a->tail, b);                                                            \
    }                                                                                              \
  }                                                                                                \
  Name* snoc_##Name(T value, Name* list) { return append_##Name(list, single_##Name(value)); }     \
  T head_##Name(const Name* list) {                                                                \
    if (list->is_nil) {                                                                            \
      error("head");                                                                               \
    }                                                                                              \
    return list->head;                                                                             \
  }                                                                                                \
  Name* tail_##Name(const Name* list) {                                                            \
    if (list->is_nil) {                                                                            \
      error("tail");                                                                               \
    }                                                                                              \
    return list->tail;                                                                             \
  }                                                                                                \
  T last_##Name(const Name* list) {                                                                \
    if (list->is_nil) {                                                                            \
      error("last");                                                                               \
    }                                                                                              \
    if (list->tail->is_nil) {                                                                      \
      return list->head;                                                                           \
    }                                                                                              \
    return last_##Name(list->tail);                                                                \
  }                                                                                                \
  bool is_nil_##Name(const Name* list) { return list->is_nil; }                                    \
  unsigned length_##Name(const Name* list) {                                                       \
    if (list->is_nil) {                                                                            \
      return 0;                                                                                    \
    } else {                                                                                       \
      return 1 + length_##Name(list->tail);                                                        \
    }                                                                                              \
  }                                                                                                \
  void remove_##Name(Name* list) {                                                                 \
    if (list->is_nil) {                                                                            \
      error("can't remove nil");                                                                   \
    }                                                                                              \
    *list = *(list->tail);                                                                         \
  }                                                                                                \
  void insert_##Name(T value, Name* list) {                                                        \
    Name* t    = list->tail;                                                                       \
    Name* new  = init_##Name();                                                                    \
    new->head  = value;                                                                            \
    new->tail  = t;                                                                                \
    list->tail = new;                                                                              \
  }                                                                                                \
  void release_##Name(Name* list) {                                                                \
    if (!list->is_nil) {                                                                           \
      release_data(list->head);                                                                    \
      release_##Name(list->tail);                                                                  \
    }                                                                                              \
    free(list);                                                                                    \
  }

#define DECLARE_LIST_PRINTER(Name) void print_##Name(FILE* f, Name* l);

#define DEFINE_LIST_PRINTER(print_data, sep, end, Name)                                            \
  void print_##Name(FILE* f, Name* l) {                                                            \
    if (l->is_nil) {                                                                               \
      fputs(end, f);                                                                               \
      return;                                                                                      \
    }                                                                                              \
    print_data(f, l->head);                                                                        \
    if (l->tail->is_nil) {                                                                         \
      fputs(end, f);                                                                               \
    } else {                                                                                       \
      fputs(sep, f);                                                                               \
      print_##Name(f, l->tail);                                                                    \
    }                                                                                              \
  }

// often used common definitions
DECLARE_LIST(int, IntList)
DECLARE_LIST_PRINTER(IntList)

#endif
