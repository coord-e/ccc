#ifndef CCC_LIST_H
#define CCC_LIST_H

#include <stdbool.h>
#include <stdio.h>

#include "error.h"

// a simple linked list
#define DEFINE_LIST(T, Name)                                                   \
  typedef struct Name Name;                                                    \
  struct Name {                                                                \
    bool is_nil;                                                               \
    T head;                                                                    \
    Name *tail;                                                                \
  };                                                                           \
  static Name *init_##Name() {                                                 \
    Name *l = malloc(sizeof(Name));                                            \
    l->is_nil = false;                                                         \
    l->tail = NULL;                                                            \
    return l;                                                                  \
  }                                                                            \
  static Name *nil_##Name() {                                                  \
    Name *l = init_##Name();                                                   \
    l->is_nil = true;                                                          \
    l->tail = NULL;                                                            \
    return l;                                                                  \
  }                                                                            \
  static Name *cons_##Name(T value, Name *list) {                              \
    Name *l = init_##Name(value);                                              \
    l->is_nil = false;                                                         \
    l->head = value;                                                           \
    l->tail = list;                                                            \
    return l;                                                                  \
  }                                                                            \
  static Name *single_##Name(T value) {                                        \
    return cons_##Name(value, nil_##Name());                                   \
  }                                                                            \
  static Name *append_##Name(Name *a, Name *b) {                               \
    if (a->is_nil) {                                                           \
      *a = *b;                                                                 \
      return a;                                                                \
    } else {                                                                   \
      return append_##Name(a->tail, b);                                        \
    }                                                                          \
  }                                                                            \
  static Name *scons_##Name(T value, Name *list) {                             \
    return append_##Name(list, single_##Name(value));                          \
  }                                                                            \
  static T head_##Name(Name *list) {                                           \
    if (list->is_nil) {                                                        \
      error("head");                                                           \
    } else {                                                                   \
      return list->head;                                                       \
    }                                                                          \
  }                                                                            \
  static Name *tail_##Name(Name *list) {                                       \
    if (list->is_nil) {                                                        \
      error("tail");                                                           \
    } else {                                                                   \
      return list->tail;                                                       \
    }                                                                          \
  }                                                                            \
  static void release_##Name(Name *list) {                                     \
    if (!list->is_nil) {                                                       \
      release_##Name(list->tail);                                              \
    }                                                                          \
    free(list);                                                                \
  }

#define DEFINE_LIST_PRINTER(print_data, Name)                                  \
  static void p_print_##Name(FILE *f, Name *l) {                               \
    if (l->is_nil) {                                                           \
      return;                                                                  \
    } else {                                                                   \
      print_data(f, l->head);                                                  \
      p_print_##Name(f, l->tail);                                              \
    }                                                                          \
  }                                                                            \
  static void print_##Name(FILE *f, Name *l) {                                 \
    p_print_##Name(f, l);                                                      \
    fprintf(f, "\n");                                                          \
  }

#endif
