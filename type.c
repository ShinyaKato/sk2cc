#include "cc.h"

Type *type_new() {
  Type *type = (Type *) calloc(1, sizeof(Type));
  return type;
}

Type *type_char() {
  Type *char_type = type_new();
  char_type->type = CHAR;
  char_type->size = 1;
  char_type->original_size = 1;
  return char_type;
}

Type *type_int() {
  Type *int_type = type_new();
  int_type->type = INT;
  int_type->size = 4;
  int_type->original_size = 4;
  return int_type;
}

Type *type_pointer_to(Type *type) {
  Type *pointer = type_new();
  pointer->type = POINTER;
  pointer->pointer_to = type;
  pointer->size = 8;
  pointer->original_size = 8;
  return pointer;
}

Type *type_array_of(Type *type, int array_size) {
  Type *array = type_new();
  array->type = ARRAY;
  array->array_of = type;
  array->array_size = array_size;
  array->size = type->size * array_size;
  array->original_size = array->size;
  return array;
}

Type *type_convert(Type *type) {
  if (type->type == ARRAY) {
    Type *pointer = type_pointer_to(type->array_of);
    pointer->original_size = type->size;
    return pointer;
  }
  return type;
}

bool type_integer(Type *type) {
  return type->type == INT || type->type == CHAR;
}
