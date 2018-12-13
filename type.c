#include "sk2cc.h"

Type *type_new() {
  Type *type = (Type *) calloc(1, sizeof(Type));
  return type;
}

StructMember *struct_member_new(Type *type, int offset) {
  StructMember *member = (StructMember *) calloc(1, sizeof(StructMember));
  member->type = type;
  member->offset = offset;
  return member;
}

Type *type_void() {
  Type *void_type = type_new();
  void_type->ty_type = VOID;
  void_type->array_pointer = false;
  void_type->definition = false;
  void_type->incomplete = true;
  return void_type;
}

Type *type_bool() {
  Type *bool_type = type_new();
  bool_type->ty_type = BOOL;
  bool_type->size = 1;
  bool_type->align = 1;
  bool_type->original_size = 1;
  bool_type->array_pointer = false;
  bool_type->definition = false;
  bool_type->incomplete = false;
  return bool_type;
}

Type *type_char() {
  Type *char_type = type_new();
  char_type->ty_type = CHAR;
  char_type->size = 1;
  char_type->align = 1;
  char_type->original_size = 1;
  char_type->array_pointer = false;
  char_type->definition = false;
  char_type->incomplete = false;
  return char_type;
}

Type *type_uchar() {
  Type *uchar_type = type_new();
  uchar_type->ty_type = UCHAR;
  uchar_type->size = 1;
  uchar_type->align = 1;
  uchar_type->original_size = 1;
  uchar_type->array_pointer = false;
  uchar_type->definition = false;
  uchar_type->incomplete = false;
  return uchar_type;
}

Type *type_short() {
  Type *short_type = type_new();
  short_type->ty_type = SHORT;
  short_type->size = 2;
  short_type->align = 2;
  short_type->original_size = 2;
  short_type->array_pointer = false;
  short_type->definition = false;
  short_type->incomplete = false;
  return short_type;
}

Type *type_ushort() {
  Type *ushort_type = type_new();
  ushort_type->ty_type = USHORT;
  ushort_type->size = 2;
  ushort_type->align = 2;
  ushort_type->original_size = 2;
  ushort_type->array_pointer = false;
  ushort_type->definition = false;
  ushort_type->incomplete = false;
  return ushort_type;
}

Type *type_int() {
  Type *int_type = type_new();
  int_type->ty_type = INT;
  int_type->size = 4;
  int_type->align = 4;
  int_type->original_size = 4;
  int_type->array_pointer = false;
  int_type->definition = false;
  int_type->incomplete = false;
  return int_type;
}

Type *type_uint() {
  Type *uint_type = type_new();
  uint_type->ty_type = UINT;
  uint_type->size = 4;
  uint_type->align = 4;
  uint_type->original_size = 4;
  uint_type->array_pointer = false;
  uint_type->definition = false;
  uint_type->incomplete = false;
  return uint_type;
}

Type *type_double() {
  Type *double_type = type_new();
  double_type->ty_type = DOUBLE;
  double_type->size = 8;
  double_type->align = 8;
  double_type->original_size = 8;
  double_type->array_pointer = false;
  double_type->definition = false;
  double_type->incomplete = false;
  return double_type;
}

Type *type_pointer_to(Type *type) {
  Type *pointer = type_new();
  pointer->ty_type = POINTER;
  pointer->size = 8;
  pointer->align = 8;
  pointer->pointer_to = type;
  pointer->original_size = 8;
  pointer->array_pointer = false;
  pointer->definition = false;
  pointer->incomplete = false;
  return pointer;
}

Type *type_array_of(Type *type, int array_size) {
  Type *array = type_new();
  array->ty_type = ARRAY;
  array->size = type->size * array_size;
  array->align = type->align;
  array->array_of = type;
  array->array_size = array_size;
  array->original_size = array->size;
  array->array_pointer = false;
  array->definition = false;
  array->incomplete = false;
  return array;
}

Type *type_incomplete_array_of(Type *type) {
  Type *array = type_new();
  array->ty_type = ARRAY;
  array->align = type->align;
  array->array_of = type;
  array->array_pointer = false;
  array->definition = false;
  array->incomplete = true;
  return array;
}

Type *type_function_returning(Type *returning, Vector *params, bool ellipsis) {
  Type *function_type = type_new();
  function_type->ty_type = FUNCTION;
  function_type->function_returning = returning;
  function_type->params = params;
  function_type->array_pointer = false;
  function_type->definition = false;
  function_type->incomplete = false;
  function_type->ellipsis = ellipsis;
  return function_type;
}

Type *type_struct(Vector *symbols) {
  Map *members = map_new();
  int align = 0, size = 0;

  for (int i = 0; i < symbols->length; i++) {
    Symbol *symbol = symbols->buffer[i];
    Type *type = symbol->type;

    if (size % type->align != 0) {
      size = size / type->align * type->align + type->align;
    }

    StructMember *member = struct_member_new(type, size);
    map_put(members, symbol->identifier, member);

    size += type->size;
    if (align < type->align) {
      align = type->align;
    }
  }

  if (size % align != 0) {
    size = size / align * align + align;
  }

  Type *struct_type = type_new();
  struct_type->ty_type = STRUCT;
  struct_type->align = align;
  struct_type->size = size;
  struct_type->members = members;
  struct_type->original_size = size;
  struct_type->array_pointer = false;
  struct_type->definition = false;
  struct_type->incomplete = false;
  return struct_type;
}

Type *type_convert(Type *type) {
  if (type->ty_type == ARRAY) {
    Type *pointer = type_pointer_to(type->array_of);
    pointer->original_size = type->size;
    pointer->array_pointer = true;
    return pointer;
  }
  return type;
}

void type_copy(Type *dest, Type *src) {
  dest->ty_type = src->ty_type;
  dest->size = src->size;
  dest->align = src->align;
  dest->pointer_to = src->pointer_to;
  dest->array_of = src->array_of;
  dest->array_size = src->array_size;
  dest->members = src->members;
  dest->original_size = src->original_size;
  dest->array_pointer = src->array_pointer;
  dest->definition = src->definition;
  dest->incomplete = src->incomplete;
}

bool type_integer(Type *type) {
  if (type->ty_type == BOOL) return true;
  if (type->ty_type == CHAR) return true;
  if (type->ty_type == UCHAR) return true;
  if (type->ty_type == SHORT) return true;
  if (type->ty_type == USHORT) return true;
  if (type->ty_type == INT) return true;
  if (type->ty_type == UINT) return true;
  return false;
}

bool type_pointer(Type *type) {
  return type->ty_type == POINTER;
}

bool type_scalar(Type *type) {
  return type_integer(type) || type_pointer(type);
}

bool type_same(Type *type1, Type *type2) {
  if (type_integer(type1) && type_integer(type2)) return true;
  if (type_pointer(type1) && type_pointer(type2)) return true;
  if (type1->ty_type == DOUBLE && type2->ty_type == DOUBLE) return true;
  return false;
}
