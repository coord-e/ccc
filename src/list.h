#ifndef CCC_LIST_H
#define CCC_LIST_H

#include <stdbool.h>
#include <stdio.h>

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
  static Name *append_##Name(T value, Name *list) {                            \
    if (list->is_nil) {                                                        \
      Name *l = single_##Name(value);                                          \
      *list = *l;                                                              \
      return list;                                                             \
    } else {                                                                   \
      return append_##Name(value, list->tail);                                 \
    }                                                                          \
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
