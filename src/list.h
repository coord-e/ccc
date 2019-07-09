#ifndef CCC_LIST_H
#define CCC_LIST_H

// a simple linked list
#define DEFINE_LIST(T, Name)                    \
  typedef struct Name Name;                     \
  struct Name {                                 \
    T head;                                     \
    Name* tail;                                 \
  }                                             \
  Name* init_##Name() {                         \
    return NULL;                                \
  }                                             \
  Name* single_##Name(T value) {                \
    Name* l = malloc(sizeof(Name));             \
    l->head = value;                            \
    l->next = NULL;                             \
    return l;                                   \
  }                                             \
  Name* cons_##Name(T value, Name* list) {      \
    Name* l = single_##Name(value);             \
    l->tail = list;                             \
    return l;                                   \
  }                                             \
  void append_##Name(T value, Name* list) {     \
    list->tail = single_##Name(value);          \
  }
