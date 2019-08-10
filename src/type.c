#include "type.h"
#include "error.h"

#include <stdlib.h>

Type* new_Type(TypeKind kind) {
  Type* ty    = calloc(1, sizeof(Type));
  ty->kind    = kind;
  ty->ptr_to  = NULL;
  ty->ret     = NULL;
  ty->params  = NULL;
  ty->element = NULL;
  ty->length  = 0;
  return ty;
}

void release_Type(Type* ty) {
  if (ty == NULL) {
    return;
  }

  release_Type(ty->ptr_to);
  release_Type(ty->ret);
  release_TypeVec(ty->params);
  release_Type(ty->element);
  free(ty);
}

Type* copy_Type(const Type* ty) {
  Type* new = new_Type(ty->kind);
  if (ty->ptr_to != NULL) {
    new->ptr_to = copy_Type(ty->ptr_to);
  }
  if (ty->ret != NULL) {
    new->ret = copy_Type(ty->ret);
  }
  if (ty->params != NULL) {
    unsigned len = length_TypeVec(ty->params);
    new->params  = new_TypeVec(len);
    for (unsigned i = 0; i < len; i++) {
      Type* t = get_TypeVec(ty->params, i);
      push_TypeVec(new->params, copy_Type(t));
    }
  }
  if (ty->element != NULL) {
    new->element = copy_Type(ty->element);
  }
  new->length = ty->length;

  return new;
}

DEFINE_VECTOR(release_Type, Type*, TypeVec)
DECLARE_VECTOR_PRINTER(TypeVec)

void print_Type(FILE* p, Type* ty) {
  switch (ty->kind) {
    case TY_CHAR:
      fprintf(p, "char");
      break;
    case TY_INT:
      fprintf(p, "int");
      break;
    case TY_LONG:
      fprintf(p, "long");
      break;
    case TY_PTR:
      fprintf(p, "*");
      print_Type(p, ty->ptr_to);
      break;
    case TY_FUNC:
      fprintf(p, "func(");
      print_TypeVec(p, ty->params);
      fprintf(p, ") ");
      print_Type(p, ty->ret);
      break;
    case TY_ARRAY:
      print_Type(p, ty->element);
      fprintf(p, "[%d]", ty->length);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

DEFINE_VECTOR_PRINTER(print_Type, ", ", "", TypeVec)

bool equal_to_Type(const Type* a, const Type* b) {
  if (a->kind != b->kind) {
    return false;
  }

  switch (a->kind) {
    case TY_CHAR:
    case TY_INT:
    case TY_LONG:
      return true;
    case TY_PTR:
      return equal_to_Type(a->ptr_to, b->ptr_to);
    case TY_FUNC:
      if (!equal_to_Type(a->ret, b->ret)) {
        return false;
      }

      if (length_TypeVec(a->params) != length_TypeVec(b->params)) {
        return false;
      }

      for (unsigned i = 0; i < length_TypeVec(a->params); i++) {
        Type* p1 = get_TypeVec(a->params, i);
        Type* p2 = get_TypeVec(b->params, i);
        if (!equal_to_Type(p1, p2)) {
          return false;
        }
      }

      return true;
    case TY_ARRAY:
      if (a->length != b->length) {
        return false;
      }
      return equal_to_Type(a->element, b->element);
    default:
      CCC_UNREACHABLE;
  }
}

Type* char_ty() {
  return new_Type(TY_CHAR);
}

Type* int_ty() {
  return new_Type(TY_INT);
}

Type* long_ty() {
  return new_Type(TY_LONG);
}

Type* ptr_to_ty(Type* ty) {
  Type* t   = new_Type(TY_PTR);
  t->ptr_to = ty;
  return t;
}

Type* func_ty(Type* ret, TypeVec* params) {
  Type* t   = new_Type(TY_FUNC);
  t->ret    = ret;
  t->params = params;
  return t;
}

Type* array_ty(Type* element, unsigned length) {
  Type* t    = new_Type(TY_ARRAY);
  t->element = element;
  t->length  = length;
  return t;
}

bool is_arithmetic_ty(const Type* ty) {
  switch (ty->kind) {
    case TY_CHAR:
      return true;
    case TY_INT:
      return true;
    case TY_LONG:
      return true;
    case TY_PTR:
      return false;
    case TY_FUNC:
      return false;
    case TY_ARRAY:
      return false;
    default:
      CCC_UNREACHABLE;
  }
}

bool is_integer_ty(const Type* ty) {
  switch (ty->kind) {
    case TY_CHAR:
      return true;
    case TY_INT:
      return true;
    case TY_LONG:
      return true;
    case TY_PTR:
      return false;
    case TY_FUNC:
      return false;
    case TY_ARRAY:
      return false;
    default:
      CCC_UNREACHABLE;
  }
}

bool is_real_ty(const Type* ty) {
  return is_integer_ty(ty);
}

bool is_compatible_ty(const Type* t1, const Type* t2) {
  return equal_to_Type(t1, t2);
}

bool is_pointer_ty(const Type* t) {
  return t->kind == TY_PTR;
}

bool is_array_ty(const Type* t) {
  return t->kind == TY_ARRAY;
}

bool is_function_ty(const Type* t) {
  return t->kind == TY_FUNC;
}

bool is_scalar_ty(const Type* ty) {
  return is_arithmetic_ty(ty) || is_pointer_ty(ty);
}

unsigned sizeof_ty(const Type* t) {
  switch (t->kind) {
    case TY_CHAR:
      return 1;
    case TY_INT:
      return 4;
    case TY_LONG:
      return 8;
    case TY_PTR:
      return 8;
    case TY_FUNC:
      error("attempt to obtain the size of function type");
    case TY_ARRAY:
      return t->length;
    default:
      CCC_UNREACHABLE;
  }
}

unsigned stored_size_ty(const Type* t) {
  switch (t->kind) {
    case TY_CHAR:
      return 1;
    case TY_INT:
      return 4;
    case TY_LONG:
      return 8;
    case TY_PTR:
      return 8;
    case TY_FUNC:
      error("attempt to obtain the size of function type");
    case TY_ARRAY:
      return t->length * stored_size_ty(t->element);
    default:
      CCC_UNREACHABLE;
  }
}

Type* int_of_size_ty(unsigned size) {
  switch (size) {
    case 1:
      return char_ty();
    case 8:
      return long_ty();
    case 4:
      return int_ty();
    default:
      CCC_UNREACHABLE;
  }
}
