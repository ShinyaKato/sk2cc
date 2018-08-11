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
  return type->type == CHAR || type->type == INT;
}

bool type_pointer(Type *type) {
  return type->type == POINTER;
}

bool type_scalar(Type *type) {
  return type_integer(type) || type_pointer(type);
}
