#ifndef CCC_TYPE_H
#define CCC_TYPE_H

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

#endif
