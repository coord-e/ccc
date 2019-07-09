#ifndef CCC_LIST_H
#define CCC_LIST_H

#include <stdio.h>

// a simple linked list
#define DEFINE_LIST(T, Name)                    \
  typedef struct Name Name;                     \
  struct Name {                                 \
    T head;                                     \
    Name* tail;                                 \
  };                                            \
  Name* init_##Name() {                         \
    return NULL;                                \
  }                                             \
  Name* single_##Name(T value) {                \
    Name* l = malloc(sizeof(Name));             \
    l->head = value;                            \
    l->tail = NULL;                             \
    return l;                                   \
  }                                             \
  Name* cons_##Name(T value, Name* list) {      \
    Name* l = single_##Name(value);             \
    l->tail = list;                             \
    return l;                                   \
  }                                             \
  Name* append_##Name(T value, Name* list) {    \
    if(list->tail == NULL) {                    \
      Name* l = single_##Name(value);           \
      list->tail = l;                           \
      return l;                                 \
    } else {                                    \
      return append_##Name(value, list->tail);  \
    }                                           \
  }

#define DEFINE_LIST_PRINTER(print_data, Name)                          \
  void p_print_##Name(FILE* f, Name* l) {                         \
    print_data(f, l->head);                                       \
    if (l->tail == NULL) {                                        \
      return;                                                     \
    } else {                                                      \
      p_print_##Name(f, l->tail);                                 \
    }                                                             \
  }                                                               \
  void print_##Name(FILE* f, Name* l) { p_print_##Name(f, l); fprintf(f, "\n"); }

#endif
