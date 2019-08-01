#include "type.h"
#include "error.h"

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

void print_Type(FILE* p, Type* ty) {
  switch (ty->kind) {
    case TY_INT:
      fprintf(p, "int");
      break;
    case TY_PTR:
      print_Type(p, ty->ptr_to);
      fprintf(p, "*");
      break;
    default:
      CCC_UNREACHABLE;
  }
}

bool equal_to_Type(const Type* a, const Type* b) {
  if (a->kind != b->kind) {
    return false;
  }

  switch (a->kind) {
    case TY_INT:
      return true;
    case TY_PTR:
      return equal_to_Type(a->ptr_to, b->ptr_to);
    default:
      CCC_UNREACHABLE;
  }
}

Type* int_ty() {
  return new_Type(TY_INT);
}

Type* ptr_to_ty(Type* ty) {
  Type* t   = new_Type(TY_PTR);
  t->ptr_to = ty;
  return t;
}
