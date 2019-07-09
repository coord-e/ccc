#ifndef CCC_LIST_H
#define CCC_LIST_H

#include <stdbool.h>
#include <stdio.h>

#include "error.h"

// a simple linked list
#define DECLARE_LIST(T, Name)                                                  \
  typedef struct Name Name;                                                    \
  Name *init_##Name();                                                         \
  Name *nil_##Name();                                                          \
  Name *cons_##Name(T value, Name *list);                                      \
  Name *single_##Name(T value);                                                \
  Name *append_##Name(Name *a, Name *b);                                       \
  Name *scons_##Name(T value, Name *list);                                     \
  T head_##Name(Name *list);                                                   \
  Name *tail_##Name(Name *list);                                               \
  bool is_nil_##Name(Name *list);                                              \
  unsigned length_##Name(Name *list);                                          \
  void release_##Name(Name *list);

#define DEFINE_LIST(T, Name)                                                   \
  struct Name {                                                                \
    bool is_nil;                                                               \
    T head;                                                                    \
    Name *tail;                                                                \
  };                                                                           \
  Name *init_##Name() {                                                        \
    Name *l = malloc(sizeof(Name));                                            \
    l->is_nil = false;                                                         \
    l->tail = NULL;                                                            \
    return l;                                                                  \
  }                                                                            \
  Name *nil_##Name() {                                                         \
    Name *l = init_##Name();                                                   \
    l->is_nil = true;                                                          \
    l->tail = NULL;                                                            \
    return l;                                                                  \
  }                                                                            \
  Name *cons_##Name(T value, Name *list) {                                     \
    Name *l = init_##Name(value);                                              \
    l->is_nil = false;                                                         \
    l->head = value;                                                           \
    l->tail = list;                                                            \
    return l;                                                                  \
  }                                                                            \
  Name *single_##Name(T value) { return cons_##Name(value, nil_##Name()); }    \
  Name *append_##Name(Name *a, Name *b) {                                      \
    if (a->is_nil) {                                                           \
      *a = *b;                                                                 \
      return a;                                                                \
    } else {                                                                   \
      return append_##Name(a->tail, b);                                        \
    }                                                                          \
  }                                                                            \
  Name *scons_##Name(T value, Name *list) {                                    \
    return append_##Name(list, single_##Name(value));                          \
  }                                                                            \
  T head_##Name(Name *list) {                                                  \
    if (list->is_nil) {                                                        \
      error("head");                                                           \
    } else {                                                                   \
      return list->head;                                                       \
    }                                                                          \
  }                                                                            \
  Name *tail_##Name(Name *list) {                                              \
    if (list->is_nil) {                                                        \
      error("tail");                                                           \
    } else {                                                                   \
      return list->tail;                                                       \
    }                                                                          \
  }                                                                            \
  bool is_nil_##Name(Name *list) { return list->is_nil; }                      \
  unsigned length_##Name(Name *list) {                                         \
    if (list->is_nil) {                                                        \
      return 0;                                                                \
    } else {                                                                   \
      return 1 + length_##Name(list->tail);                                    \
    }                                                                          \
  }                                                                            \
  void release_##Name(Name *list) {                                            \
    if (!list->is_nil) {                                                       \
      release_##Name(list->tail);                                              \
    }                                                                          \
    free(list);                                                                \
  }

#define DECLARE_LIST_PRINTER(print_data, Name)                                 \
  void print_##Name(FILE *f, Name *l);

#define DEFINE_LIST_PRINTER(print_data, Name)                                  \
  void p_print_##Name(FILE *f, Name *l) {                                      \
    if (l->is_nil) {                                                           \
      return;                                                                  \
    } else {                                                                   \
      print_data(f, l->head);                                                  \
      p_print_##Name(f, l->tail);                                              \
    }                                                                          \
  }                                                                            \
  void print_##Name(FILE *f, Name *l) {                                        \
    p_print_##Name(f, l);                                                      \
    fprintf(f, "\n");                                                          \
  }

#endif
