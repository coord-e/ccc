#ifndef CCC_TYPE_H
#define CCC_TYPE_H

#include <stdbool.h>
#include <stdio.h>

#include "vector.h"

typedef enum {
  TY_INT,
  TY_PTR,
  TY_FUNC,
} TypeKind;

typedef struct Type Type;

DECLARE_VECTOR(Type*, TypeVec)

struct Type {
  TypeKind kind;
  Type* ptr_to;

  // for TY_FUNC
  Type* ret;
  TypeVec* params;
};

Type* new_Type(TypeKind);
void release_Type(Type*);
void print_Type(FILE*, Type*);
bool equal_to_Type(const Type*, const Type*);
Type* copy_Type(const Type*);

Type* int_ty();
Type* ptr_to_ty(Type*);
Type* func_ty(Type*, TypeVec*);

bool is_arithmetic_ty(const Type*);
bool is_integer_ty(const Type*);
bool is_real_ty(const Type*);
bool is_compatible_ty(const Type*, const Type*);
bool is_pointer_ty(const Type*);

#endif
