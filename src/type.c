#include "type.h"

#include <stdlib.h>

Type* new_Type(TypeKind kind) {
  Type* ty   = calloc(1, sizeof(Type));
  ty->kind   = kind;
  ty->ptr_to = NULL;
  return ty;
}

void release_Type(Type* ty) {
  if (ty == NULL) {
    return;
  }

  release_Type(ty->ptr_to);
  free(ty);
}
