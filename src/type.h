#ifndef CCC_TYPE_H
#define CCC_TYPE_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
  TY_INT,
  TY_PTR,
} TypeKind;

typedef struct Type Type;

struct Type {
  TypeKind kind;
  Type* ptr_to;
};

Type* new_Type(TypeKind);
void release_Type(Type*);
void print_Type(FILE*, Type*);
bool equal_to_Type(const Type*, const Type*);

Type* int_ty();
Type* ptr_to_ty(Type*);

#endif
