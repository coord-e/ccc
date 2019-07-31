#ifndef CCC_MAP_H
#define CCC_MAP_H

#include <stdio.h>

#include "list.h"
#include "util.h"
#include "vector.h"

// hash table from string to `T`
#define DECLARE_MAP(T, Name)                                                                       \
  typedef struct Name##Entry Name##Entry;                                                          \
  DECLARE_LIST(Name##Entry, Name##Entries)                                                         \
  DECLARE_VECTOR(Name##Entries*, Name##Table)                                                      \
  typedef struct Name Name;                                                                        \
  Name* new_##Name(unsigned size);                                                                 \
  Name* copy_##Name(const Name*);                                                                  \
  void insert_##Name(Name*, const char* k, T v);                                                   \
  T get_##Name(Name*, const char* k);                                                              \
  bool lookup_##Name(Name*, const char* k, T* out);                                                \
  void remove_##Name(Name*, const char* k);                                                        \
  void release_##Name(Name*);

#define DEFINE_MAP(release_T, T, Name)                                                             \
  struct Name##Entry {                                                                             \
    unsigned hash;                                                                                 \
    char* key;                                                                                     \
    T value;                                                                                       \
  };                                                                                               \
  static void release_entry(Name##Entry e) {                                                       \
    free(e.key);                                                                                   \
    release_T(e.value);                                                                            \
  }                                                                                                \
  DEFINE_LIST(release_entry, Name##Entry, Name##Entries)                                           \
  DEFINE_VECTOR(release_##Name##Entries, Name##Entries*, Name##Table)                              \
  struct Name {                                                                                    \
    Name##Table* table;                                                                            \
  };                                                                                               \
  Name* new_##Name(unsigned size) {                                                                \
    Name* m  = calloc(1, sizeof(Name));                                                            \
    m->table = new_##Name##Table(size);                                                            \
    for (unsigned i = 0; i < size; i++) {                                                          \
      push_##Name##Table(m->table, nil_##Name##Entries());                                         \
    }                                                                                              \
    return m;                                                                                      \
  }                                                                                                \
  Name##Entries* copy_entries_##Name(const Name##Entries* list) {                                  \
    if (list->is_nil) {                                                                            \
      return nil_##Name##Entries();                                                                \
    }                                                                                              \
    Name##Entry e = list->head;                                                                    \
    e.key         = strdup(e.key);                                                                 \
    return cons_##Name##Entries(e, copy_entries_##Name(list->tail));                               \
  }                                                                                                \
  Name* copy_##Name(const Name* m) {                                                               \
    Name* copy    = calloc(1, sizeof(Name));                                                       \
    unsigned size = length_##Name##Table(m->table);                                                \
    copy->table   = new_##Name##Table(size);                                                       \
    for (unsigned i = 0; i < size; i++) {                                                          \
      Name##Entries* l = get_##Name##Table(m->table, i);                                           \
      push_##Name##Table(copy->table, copy_entries_##Name(l));                                     \
    }                                                                                              \
    return copy;                                                                                   \
  }                                                                                                \
  static Name##Entry make_entry_##Name(const char* k, T v) {                                       \
    unsigned hash = hash_string(k);                                                                \
    Name##Entry e;                                                                                 \
    e.hash  = hash;                                                                                \
    e.value = v;                                                                                   \
    e.key   = strdup(k);                                                                           \
    return e;                                                                                      \
  }                                                                                                \
  void insert_##Name(Name* m, const char* k, T v) {                                                \
    Name##Entry e          = make_entry_##Name(k, v);                                              \
    unsigned idx           = e.hash % length_##Name##Table(m->table);                              \
    Name##Entries* es      = get_##Name##Table(m->table, idx);                                     \
    Name##Entries* chained = cons_##Name##Entries(e, es);                                          \
    set_##Name##Table(m->table, idx, chained);                                                     \
  }                                                                                                \
  T get_##Name(Name* m, const char* k) {                                                           \
    T out;                                                                                         \
    if (lookup_##Name(m, k, &out)) {                                                               \
      return out;                                                                                  \
    }                                                                                              \
    error("key \"%s\" not found", k);                                                              \
  }                                                                                                \
  static bool search_##Name(Name##Entries* es, unsigned hash, T* out) {                            \
    if (is_nil_##Name##Entries(es)) {                                                              \
      return false;                                                                                \
    }                                                                                              \
    Name##Entry e = head_##Name##Entries(es);                                                      \
    if (e.hash == hash) {                                                                          \
      if (out != NULL) {                                                                           \
        *out = e.value;                                                                            \
      }                                                                                            \
      return true;                                                                                 \
    }                                                                                              \
    Name##Entries* t = tail_##Name##Entries(es);                                                   \
    return search_##Name(t, hash, out);                                                            \
  }                                                                                                \
  bool lookup_##Name(Name* m, const char* k, T* out) {                                             \
    unsigned hash     = hash_string(k);                                                            \
    unsigned idx      = hash % length_##Name##Table(m->table);                                     \
    Name##Entries* es = get_##Name##Table(m->table, idx);                                          \
    return search_##Name(es, hash, out);                                                           \
  }                                                                                                \
  static void search_remove_##Name(Name##Entries* es, unsigned hash) {                             \
    if (is_nil_##Name##Entries(es)) {                                                              \
      return;                                                                                      \
    }                                                                                              \
    Name##Entry e    = head_##Name##Entries(es);                                                   \
    Name##Entries* t = tail_##Name##Entries(es);                                                   \
    if (e.hash == hash) {                                                                          \
      *es = *t;                                                                                    \
      return;                                                                                      \
    }                                                                                              \
    search_remove_##Name(t, hash);                                                                 \
  }                                                                                                \
  void remove_##Name(Name* m, const char* k) {                                                     \
    unsigned hash     = hash_string(k);                                                            \
    unsigned idx      = hash % length_##Name##Table(m->table);                                     \
    Name##Entries* es = get_##Name##Table(m->table, idx);                                          \
    search_remove_##Name(es, hash);                                                                \
  }                                                                                                \
  void release_##Name(Name* m) { release_##Name##Table(m->table); }

#define DECLARE_MAP_PRINTER(Name) void print_##Name(FILE*, Name*);

#define DEFINE_MAP_PRINTER(print_T, begin, sep, sep_kv, end, Name)                                 \
  static void print_entries_##Name(FILE* p, Name##Entries* es) {                                   \
    if (is_nil_##Name##Entries(es)) {                                                              \
      return;                                                                                      \
    }                                                                                              \
    Name##Entry e = head_##Name##Entries(es);                                                      \
    fputs(e.key, p);                                                                               \
    fputs(sep_kv, p);                                                                              \
    print_T(p, e.value);                                                                           \
    Name##Entries* t = tail_##Name##Entries(es);                                                   \
    if (!is_nil_##Name##Entries(t)) {                                                              \
      fputs(sep, p);                                                                               \
    }                                                                                              \
    print_entries_##Name(p, t);                                                                    \
  }                                                                                                \
  void print_##Name(FILE* p, Name* m) {                                                            \
    fputs(begin, p);                                                                               \
    for (unsigned i = 0; i < length_##Name##Table(m->table); i++) {                                \
      print_entries_##Name(p, get_##Name##Table(m->table, i));                                     \
    }                                                                                              \
    fputs(end, p);                                                                                 \
  }

unsigned hash_string(const char*);

#endif
