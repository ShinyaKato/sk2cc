#include "cc.h"

Type *type_new() {
  Type *type = (Type *) calloc(1, sizeof(Type));
  return type;
}

Type *type_char() {
  Type *char_type = type_new();
  char_type->type = CHAR;
  char_type->size = 1;
  char_type->align = 1;
  char_type->original_size = 1;
  char_type->array_pointer = false;
  return char_type;
}

Type *type_int() {
  Type *int_type = type_new();
  int_type->type = INT;
  int_type->size = 4;
  int_type->align = 4;
  int_type->original_size = 4;
  int_type->array_pointer = false;
  return int_type;
}

Type *type_pointer_to(Type *type) {
  Type *pointer = type_new();
  pointer->type = POINTER;
  pointer->size = 8;
  pointer->align = 8;
  pointer->pointer_to = type;
  pointer->original_size = 8;
  pointer->array_pointer = false;
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
  return struct_type;
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

bool type_integer(Type *type) {
  return type->type == CHAR || type->type == INT;
}

bool type_pointer(Type *type) {
  return type->type == POINTER;
}

bool type_scalar(Type *type) {
  return type_integer(type) || type_pointer(type);
}
