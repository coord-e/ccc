#include "type.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>

DataSize to_data_size(unsigned i) {
  switch (i) {
    case 1:
      return SIZE_BYTE;
    case 2:
      return SIZE_WORD;
    case 4:
      return SIZE_DWORD;
    case 8:
      return SIZE_QWORD;
    default:
      error("invalid data size %d", i);
  }
}

unsigned from_data_size(DataSize i) {
  return i;
}

static void release_long(long l) {}
static long copy_long(long l) {
  return l;
}
DEFINE_MAP(copy_long, release_long, long, EnumMap)

static void release_Field(Field* field) {
  if (field == NULL) {
    return;
  }

  release_Type(field->type);
  free(field);
}

static Field* copy_Field(Field* field) {
  return new_Field(copy_Type(field->type), field->offset);
}

DEFINE_MAP(copy_Field, release_Field, Field*, FieldMap)

Field* new_Field(Type* ty, unsigned offset) {
  Field* f  = calloc(1, sizeof(Field));
  f->type   = ty;
  f->offset = offset;
  return f;
}

static bool equal_to_Field(Field* f1, Field* f2) {
  if (f1->offset != f2->offset) {
    return false;
  }

  return equal_to_Type(f1->type, f2->type);
}

static void release_string(char* s) {
  free(s);
}
DEFINE_VECTOR(release_string, char*, StringVec)

Type* new_Type(TypeKind kind) {
  Type* ty = calloc(1, sizeof(Type));
  ty->kind = kind;
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
  release_StringVec(ty->fields);
  release_FieldMap(ty->field_map);
  release_StringVec(ty->enums);
  release_EnumMap(ty->enum_map);
  free(ty->tag);
  free(ty);
}

Type* copy_Type(const Type* ty) {
  Type* new = new_Type(ty->kind);
  *new      = *ty;
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
  if (ty->tag != NULL) {
    new->tag = strdup(ty->tag);
  }
  if (ty->fields != NULL) {
    unsigned len = length_StringVec(ty->fields);
    new->fields  = new_StringVec(len);
    for (unsigned i = 0; i < len; i++) {
      char* t = get_StringVec(ty->fields, i);
      push_StringVec(new->fields, strdup(t));
    }
  }
  if (ty->field_map != NULL) {
    new->field_map = copy_FieldMap(ty->field_map);
  }
  if (ty->enums != NULL) {
    unsigned len = length_StringVec(ty->enums);
    new->enums   = new_StringVec(len);
    for (unsigned i = 0; i < len; i++) {
      char* t = get_StringVec(ty->enums, i);
      push_StringVec(new->enums, strdup(t));
    }
  }
  if (ty->enum_map != NULL) {
    new->enum_map = copy_EnumMap(ty->enum_map);
  }

  return new;
}

DEFINE_VECTOR(release_Type, Type*, TypeVec)
DECLARE_VECTOR_PRINTER(TypeVec)

static void print_Field(FILE* p, Field* f) {
  print_Type(p, f->type);
  fprintf(p, ", offset %d;\n", f->offset);
}

DECLARE_MAP_PRINTER(FieldMap)
DEFINE_MAP_PRINTER(print_Field, "{\n", ";\n", ": ", "}\n", FieldMap)

static void print_enumerator(FILE* p, long l) {
  fprintf(p, "%ld", l);
}

DECLARE_MAP_PRINTER(EnumMap)
DEFINE_MAP_PRINTER(print_enumerator, "{\n", ",\n", " = ", "}\n", EnumMap)

void print_Type(FILE* p, Type* ty) {
  switch (ty->kind) {
    case TY_VOID:
      fprintf(p, "void");
      break;
    case TY_BOOL:
      fprintf(p, "_Bool");
      break;
    case TY_INT:
      if (ty->is_signed) {
        fprintf(p, "signed ");
      } else {
        fprintf(p, "unsigned ");
      }
      switch (ty->size) {
        case SIZE_BYTE:
          fprintf(p, "char");
          break;
        case SIZE_WORD:
          fprintf(p, "short");
          break;
        case SIZE_DWORD:
          fprintf(p, "int");
          break;
        case SIZE_QWORD:
          fprintf(p, "long");
          break;
        default:
          CCC_UNREACHABLE;
      }
      break;
    case TY_PTR:
      fprintf(p, "* ");
      print_Type(p, ty->ptr_to);
      break;
    case TY_FUNC:
      fprintf(p, "func(");
      print_TypeVec(p, ty->params);
      if (ty->is_vararg) {
        fprintf(p, ", ...");
      }
      fprintf(p, ") ");
      print_Type(p, ty->ret);
      break;
    case TY_STRUCT:
      fputs("struct ", p);
      if (ty->tag != NULL) {
        fprintf(p, "%s ", ty->tag);
      }
      if (ty->field_map != NULL) {
        print_FieldMap(p, ty->field_map);
      }
      break;
    case TY_ENUM:
      fputs("enum ", p);
      if (ty->tag != NULL) {
        fprintf(p, "%s ", ty->tag);
      }
      if (ty->enum_map != NULL) {
        print_EnumMap(p, ty->enum_map);
      }
      break;
    case TY_ARRAY:
      if (ty->is_length_known) {
        fprintf(p, "[%d] ", ty->length);
      } else {
        fprintf(p, "[] ");
      }
      print_Type(p, ty->element);
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

  if ((bool)a->tag != (bool)b->tag) {
    return false;
  }
  // if a->tag == NULL, b->tag would be NULL
  if (a->tag != NULL && strcmp(a->tag, b->tag) != 0) {
    return false;
  }

  switch (a->kind) {
    case TY_VOID:
    case TY_BOOL:
      return true;
    case TY_STRUCT:
      if ((bool)a->fields != (bool)b->fields) {
        return false;
      }
      if (a->fields == NULL) {
        return true;
      }
      if (length_StringVec(a->fields) != length_StringVec(b->fields)) {
        return false;
      }
      for (unsigned i = 0; i < length_StringVec(a->fields); i++) {
        char* k1 = get_StringVec(a->fields, i);
        char* k2 = get_StringVec(b->fields, i);
        if (strcmp(k1, k2) != 0) {
          return false;
        }
        Field* f1 = get_FieldMap(a->field_map, k1);
        Field* f2 = get_FieldMap(b->field_map, k2);
        if (!equal_to_Field(f1, f2)) {
          return false;
        }
      }
      return true;
    case TY_ENUM:
      if ((bool)a->enums != (bool)b->enums) {
        return false;
      }
      if (a->enums == NULL) {
        return true;
      }
      if (length_StringVec(a->enums) != length_StringVec(b->enums)) {
        return false;
      }
      for (unsigned i = 0; i < length_StringVec(a->enums); i++) {
        char* k1 = get_StringVec(a->enums, i);
        char* k2 = get_StringVec(b->enums, i);
        if (strcmp(k1, k2) != 0) {
          return false;
        }
        if (get_EnumMap(a->enum_map, k1) != get_EnumMap(b->enum_map, k2)) {
          return false;
        }
      }
      return true;
    case TY_INT:
      return a->size == b->size && a->is_signed == b->is_signed;
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
      if (a->is_length_known != b->is_length_known) {
        return false;
      }
      if (a->length != b->length) {
        return false;
      }
      return equal_to_Type(a->element, b->element);
    default:
      CCC_UNREACHABLE;
  }
}

Type* new_int_Type(DataSize size, bool is_signed) {
  Type* ty      = new_Type(TY_INT);
  ty->size      = size;
  ty->is_signed = is_signed;
  return ty;
}

Type* char_ty() {
  return new_int_Type(SIZE_BYTE, false);
}

Type* int_ty() {
  return new_int_Type(SIZE_DWORD, true);
}

Type* long_ty() {
  return new_int_Type(SIZE_QWORD, true);
}

Type* short_ty() {
  return new_int_Type(SIZE_WORD, true);
}

Type* void_ty() {
  return new_Type(TY_VOID);
}

Type* bool_ty() {
  return new_Type(TY_BOOL);
}

Type* struct_ty(char* tag, StringVec* fields, FieldMap* field_map) {
  Type* t      = new_Type(TY_STRUCT);
  t->field_map = field_map;
  t->fields    = fields;
  t->tag       = tag;
  return t;
}

Type* enum_ty(char* tag, StringVec* enums, EnumMap* enum_map) {
  Type* t     = new_Type(TY_ENUM);
  t->enum_map = enum_map;
  t->enums    = enums;
  t->tag      = tag;
  return t;
}

Type* ptr_to_ty(Type* ty) {
  Type* t   = new_Type(TY_PTR);
  t->ptr_to = ty;
  return t;
}

Type* func_ty(Type* ret, TypeVec* params, bool is_vararg) {
  Type* t      = new_Type(TY_FUNC);
  t->ret       = ret;
  t->params    = params;
  t->is_vararg = is_vararg;
  return t;
}

Type* array_ty(Type* element, bool is_length_known) {
  Type* t            = new_Type(TY_ARRAY);
  t->element         = element;
  t->is_length_known = is_length_known;
  return t;
}

Type* size_t_ty() {
  return into_unsigned_ty(long_ty());
}

Type* ptrdiff_t_ty() {
  return long_ty();
}

Type* enum_underlying_ty() {
  return long_ty();
}

void make_signed_ty(Type* t) {
  assert(t->kind == TY_INT);
  t->is_signed = true;
}

void make_unsigned_ty(Type* t) {
  assert(t->kind == TY_INT);
  t->is_signed = false;
}

Type* to_signed_ty(Type* t) {
  Type* new = copy_Type(t);
  make_signed_ty(new);
  return new;
}

Type* to_unsigned_ty(Type* t) {
  Type* new = copy_Type(t);
  make_unsigned_ty(new);
  return new;
}

Type* into_signed_ty(Type* t) {
  Type* new = to_signed_ty(t);
  release_Type(t);
  return new;
}

Type* into_unsigned_ty(Type* t) {
  Type* new = to_unsigned_ty(t);
  release_Type(t);
  return new;
}

bool is_arithmetic_ty(const Type* ty) {
  return is_integer_ty(ty);  // || is_floating_ty(ty);
}

bool is_real_ty(const Type* ty) {
  return is_integer_ty(ty);  // || is_real_floating_ty(ty);
}

bool is_compatible_ty(const Type* a, const Type* b) {
  if (a->kind == TY_ENUM && equal_to_Type(enum_underlying_ty(), b)) {
    return true;
  }
  if (b->kind == TY_ENUM && equal_to_Type(enum_underlying_ty(), a)) {
    return true;
  }

  if (a->kind != b->kind) {
    return false;
  }

  if ((bool)a->tag != (bool)b->tag) {
    return false;
  }
  // if a->tag == NULL, b->tag would be NULL
  if (a->tag != NULL && strcmp(a->tag, b->tag) != 0) {
    return false;
  }

  switch (a->kind) {
    case TY_STRUCT:
      if (is_complete_ty(a) && is_complete_ty(b)) {
        if (length_StringVec(a->fields) != length_StringVec(b->fields)) {
          return false;
        }
        for (unsigned i = 0; i < length_StringVec(a->fields); i++) {
          char* k1 = get_StringVec(a->fields, i);
          char* k2 = get_StringVec(b->fields, i);
          if (strcmp(k1, k2) != 0) {
            return false;
          }
          Field* f1 = get_FieldMap(a->field_map, k1);
          Field* f2 = get_FieldMap(b->field_map, k2);
          if (!equal_to_Field(f1, f2)) {
            return false;
          }
        }
      }
      return true;
    case TY_ENUM:
      if (is_complete_ty(a) && is_complete_ty(b)) {
        if (length_StringVec(a->enums) != length_StringVec(b->enums)) {
          return false;
        }
        for (unsigned i = 0; i < length_StringVec(a->enums); i++) {
          char* k1 = get_StringVec(a->enums, i);
          char* k2 = get_StringVec(b->enums, i);
          if (strcmp(k1, k2) != 0) {
            return false;
          }
          if (get_EnumMap(a->enum_map, k1) != get_EnumMap(b->enum_map, k2)) {
            return false;
          }
        }
      }
      return true;
    default:
      return equal_to_Type(a, b);
  }
}

bool is_integer_ty(const Type* t) {
  return t->kind == TY_INT || t->kind == TY_BOOL || t->kind == TY_ENUM;
}

bool is_pointer_ty(const Type* t) {
  return t->kind == TY_PTR;
}

bool is_array_ty(const Type* t) {
  return t->kind == TY_ARRAY;
}

bool is_character_ty(const Type* t) {
  return t->kind == TY_INT && t->size == SIZE_BYTE;
}

bool is_function_ty(const Type* t) {
  return t->kind == TY_FUNC;
}

bool is_scalar_ty(const Type* ty) {
  return is_arithmetic_ty(ty) || is_pointer_ty(ty);
}

bool is_complete_ty(const Type* ty) {
  switch (ty->kind) {
    case TY_VOID:
      return false;
    case TY_INT:
      return true;
    case TY_BOOL:
      return true;
    case TY_STRUCT:
      return ty->fields;
    case TY_ENUM:
      return ty->enums;
    case TY_PTR:
      return true;
    case TY_FUNC:
      return true;
    case TY_ARRAY:
      return ty->is_length_known;
    default:
      CCC_UNREACHABLE;
  }
}

unsigned length_of_ty(const Type* ty) {
  assert(ty->kind == TY_ARRAY);
  if (!ty->is_length_known) {
    error("attempt to obtain the length of incomplete array type");
  }
  return ty->length;
}

void set_length_ty(Type* ty, unsigned length) {
  assert(ty->kind == TY_ARRAY);
  assert(!ty->is_length_known);
  ty->is_length_known = true;
  ty->length          = length;
}

unsigned sizeof_ty(const Type* t) {
  // TODO: forbid void
  if (t->kind != TY_VOID && !is_complete_ty(t)) {
    error("unable to obtain the size of incomplete type");
  }
  switch (t->kind) {
    case TY_VOID:
      return 1;
    case TY_BOOL:
      return 1;
    case TY_INT:
      return from_data_size(t->size);
    case TY_PTR:
      return 8;
    case TY_FUNC:
      error("attempt to obtain the size of function type");
    case TY_ARRAY:
      return length_of_ty(t) * sizeof_ty(t->element);
    case TY_STRUCT: {
      char* last = get_StringVec(t->fields, length_StringVec(t->fields) - 1);
      Field* f   = get_FieldMap(t->field_map, last);
      return f->offset + sizeof_ty(f->type);
    }
    case TY_ENUM:
      return sizeof_ty(enum_underlying_ty());
    default:
      CCC_UNREACHABLE;
  }
}

Type* int_of_size_ty(unsigned size) {
  return new_int_Type(to_data_size(size), true);
}

// integer conversions

// t1 > t2 -> positive
// t1 < t2 -> negative
// t1 = t2 -> zero
int compare_rank_ty(const Type* t1, const Type* t2) {
  assert(is_integer_ty(t1));
  assert(is_integer_ty(t2));

  if (t1->kind == TY_BOOL && t2->kind == TY_BOOL) {
    return 0;
  } else if (t1->kind == TY_BOOL && t2->kind != TY_BOOL) {
    return -1;
  } else if (t1->kind != TY_BOOL && t2->kind == TY_BOOL) {
    return 1;
  }

  return sizeof_ty(t1) - sizeof_ty(t2);
}

bool is_representable_in_ty(const Type* t1, const Type* t2) {
  assert(is_integer_ty(t1));
  assert(is_integer_ty(t2));

  if (equal_to_Type(t1, t2)) {
    return true;
  }

  if (t1->kind == TY_BOOL && t2->kind != TY_BOOL) {
    return true;
  } else if (t1->kind != TY_BOOL && t2->kind == TY_BOOL) {
    return false;
  }

  if (t1->kind == TY_ENUM && t2->kind != TY_ENUM) {
    t1 = enum_underlying_ty();
  } else if (t1->kind != TY_ENUM && t2->kind == TY_ENUM) {
    return false;
  }

  if (t1->is_signed && !t2->is_signed) {
    return false;
  } else if (!t2->is_signed && t2->is_signed) {
    return t1->size * 2 <= t2->size;
  } else {
    return t1->size <= t2->size;
  }
}
