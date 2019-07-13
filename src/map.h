#ifndef CCC_MAP_H
#define CCC_MAP_H

#include "list.h"
#include "vector.h"

// hash table from string to `T`
#define DECLARE_MAP(T, Name)                                                                       \
  typedef struct Name##Entry Name##Entry;                                                          \
  DECLARE_LIST(Name##Entry, Name##Entries)                                                         \
  DECLARE_VECTOR(Name##Entries*, Name##Table)                                                      \
  typedef struct Name Name;                                                                        \
  Name* new_##Name(unsigned size);                                                                 \
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

unsigned hash_string(const char*);

#endif
