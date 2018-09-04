#include "sk2cc.h"

Type *type_new() {
  Type *type = (Type *) calloc(1, sizeof(Type));
  return type;
}

Type *type_void() {
  Type *void_type = type_new();
  void_type->type = VOID;
  void_type->array_pointer = false;
  void_type->definition = false;
  void_type->incomplete = true;
  return void_type;
}

Type *type_bool() {
  Type *bool_type = type_new();
  bool_type->type = BOOL;
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
  char_type->type = CHAR;
  char_type->size = 1;
  char_type->align = 1;
  char_type->original_size = 1;
  char_type->array_pointer = false;
  char_type->definition = false;
  char_type->incomplete = false;
  return char_type;
}

Type *type_int() {
  Type *int_type = type_new();
  int_type->type = INT;
  int_type->size = 4;
  int_type->align = 4;
  int_type->original_size = 4;
  int_type->array_pointer = false;
  int_type->definition = false;
  int_type->incomplete = false;
  return int_type;
}

Type *type_double() {
  Type *double_type = type_new();
  double_type->type = DOUBLE;
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
  pointer->type = POINTER;
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
  array->type = ARRAY;
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
  array->type = ARRAY;
  array->align = type->align;
  array->array_of = type;
  array->array_pointer = false;
  array->definition = false;
  array->incomplete = true;
  return array;
}

Type *type_struct(Vector *identifiers, Map *members) {
  Map *offsets = map_new();
  int align = 0, size = 0;

  for (int i = 0; i < identifiers->length; i++) {
    char *identifier = identifiers->array[i];
    Type *type = map_lookup(members, identifier);

    if (size % type->align != 0) {
      size = size / type->align * type->align + type->align;
    }

    int *offset = (int *) calloc(1, sizeof(int));
    *offset = size;
    map_put(offsets, identifier, offset);

    size += type->size;
    if (align < type->align) {
      align = type->align;
    }
  }

  if (size % align != 0) {
    size = size / align * align + align;
  }

  Type *struct_type = type_new();
  struct_type->type = STRUCT;
  struct_type->align = align;
  struct_type->size = size;
  struct_type->members = members;
  struct_type->offsets = offsets;
  struct_type->original_size = size;
  struct_type->array_pointer = false;
  struct_type->definition = false;
  struct_type->incomplete = false;
  return struct_type;
}

Type *type_function_returning(Type *returning, Vector *params, bool ellipsis) {
  Type *function_type = type_new();
  function_type->type = FUNCTION;
  function_type->function_returning = returning;
  function_type->params = params;
  function_type->array_pointer = false;
  function_type->definition = false;
  function_type->incomplete = false;
  function_type->ellipsis = ellipsis;
  return function_type;
}

Type *type_convert(Type *type) {
  if (type->type == ARRAY) {
    Type *pointer = type_pointer_to(type->array_of);
    pointer->original_size = type->size;
    pointer->array_pointer = true;
    return pointer;
  }
  return type;
}

void type_copy(Type *dest, Type *src) {
  dest->type = src->type;
  dest->size = src->size;
  dest->align = src->align;
  dest->pointer_to = src->pointer_to;
  dest->array_of = src->array_of;
  dest->array_size = src->array_size;
  dest->members = src->members;
  dest->offsets = src->offsets;
  dest->original_size = src->original_size;
  dest->array_pointer = src->array_pointer;
  dest->definition = src->definition;
  dest->incomplete = src->incomplete;
}

bool type_integer(Type *type) {
  return type->type == CHAR || type->type == INT || type->type == BOOL;
}

bool type_pointer(Type *type) {
  return type->type == POINTER;
}

bool type_scalar(Type *type) {
  return type_integer(type) || type_pointer(type);
}

bool type_same(Type *type1, Type *type2) {
  if (type_integer(type1) && type_integer(type2)) return true;
  if (type_pointer(type1) && type_pointer(type2)) return true;
  if (type1->type == DOUBLE && type2->type == DOUBLE) return true;
  return false;
}
