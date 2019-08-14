#ifndef CCC_TYPE_H
#define CCC_TYPE_H

#include <stdbool.h>
#include <stdio.h>

#include "vector.h"

typedef enum {
  SIZE_BYTE  = 1,  // 1 byte, 8  bits
  SIZE_WORD  = 2,  // 2 byte, 16 bits
  SIZE_DWORD = 4,  // 4 byte, 32 bits
  SIZE_QWORD = 8,  // 8 byte, 64 bits
} DataSize;

DataSize to_data_size(unsigned);
unsigned from_data_size(DataSize);

typedef enum {
  TY_INT,  // not `int`, but all integer types
  TY_VOID,
  TY_PTR,
  TY_FUNC,
  TY_ARRAY,
} TypeKind;

typedef struct Type Type;

DECLARE_VECTOR(Type*, TypeVec)

struct Type {
  TypeKind kind;

  // for TY_INT
  DataSize size;
  bool is_signed;

  // for TY_PTR
  Type* ptr_to;

  // for TY_FUNC
  Type* ret;
  TypeVec* params;

  // for TY_ARRAY
  Type* element;
  bool is_length_known;
  unsigned length;
};

Type* new_Type(TypeKind);
Type* new_int_Type(DataSize size, bool is_signed);
void release_Type(Type*);
void print_Type(FILE*, Type*);
bool equal_to_Type(const Type*, const Type*);
Type* copy_Type(const Type*);

Type* char_ty();
Type* int_ty();
Type* long_ty();
Type* short_ty();
Type* void_ty();
Type* ptr_to_ty(Type*);
Type* func_ty(Type*, TypeVec*);
Type* array_ty(Type*, bool is_length_known);

Type* size_t_ty();

void make_signed_ty(Type*);
void make_unsigned_ty(Type*);
Type* to_signed_ty(Type*);
Type* to_unsigned_ty(Type*);
Type* into_signed_ty(Type*);
Type* into_unsigned_ty(Type*);

bool is_arithmetic_ty(const Type*);
bool is_integer_ty(const Type*);
bool is_character_ty(const Type*);
bool is_real_ty(const Type*);
bool is_compatible_ty(const Type*, const Type*);
bool is_pointer_ty(const Type*);
bool is_scalar_ty(const Type*);
bool is_array_ty(const Type*);
bool is_function_ty(const Type*);
bool is_complete_ty(const Type*);
unsigned length_of_ty(const Type*);

unsigned sizeof_ty(const Type*);
Type* int_of_size_ty(unsigned);

#endif
