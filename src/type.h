#ifndef CCC_TYPE_H
#define CCC_TYPE_H

#include <stdbool.h>
#include <stdio.h>

#include "vector.h"

typedef enum {
  TY_INT,
  TY_LONG,
  TY_PTR,
  TY_FUNC,
  TY_ARRAY,
} TypeKind;

typedef struct Type Type;

DECLARE_VECTOR(Type*, TypeVec)

struct Type {
  TypeKind kind;

  // for TY_PTR
  Type* ptr_to;

  // for TY_FUNC
  Type* ret;
  TypeVec* params;

  // for TY_ARRAY
  Type* element;
  unsigned length;
};

Type* new_Type(TypeKind);
void release_Type(Type*);
void print_Type(FILE*, Type*);
bool equal_to_Type(const Type*, const Type*);
Type* copy_Type(const Type*);

Type* int_ty();
Type* ptr_to_ty(Type*);
Type* func_ty(Type*, TypeVec*);
Type* array_ty(Type*, unsigned);

bool is_arithmetic_ty(const Type*);
bool is_integer_ty(const Type*);
bool is_real_ty(const Type*);
bool is_compatible_ty(const Type*, const Type*);
bool is_pointer_ty(const Type*);
bool is_scalar_ty(const Type*);
bool is_array_ty(const Type*);
bool is_function_ty(const Type*);

unsigned sizeof_ty(const Type*);
unsigned stored_size_ty(const Type*);
Type* int_of_size_ty(unsigned);

#endif
