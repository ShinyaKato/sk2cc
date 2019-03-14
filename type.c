#include "sk2cc.h"

Type *type_new(TypeType ty_type, int size, int align, bool complete) {
  Type *type = calloc(1, sizeof(Type));
  type->ty_type = ty_type;
  type->size = size;
  type->align = align;
  type->complete = complete;
  type->original = type;
  return type;
}

Type *type_void() {
  return type_new(TY_VOID, 0, 1, false);
}

Type *type_char() {
  return type_new(TY_CHAR, 1, 1, true);
}

Type *type_uchar() {
  return type_new(TY_UCHAR, 1, 1, true);
}

Type *type_short() {
  return type_new(TY_SHORT, 2, 2, true);
}

Type *type_ushort() {
  return type_new(TY_USHORT, 2, 2, true);
}

Type *type_int() {
  return type_new(TY_INT, 4, 4, true);
}

Type *type_uint() {
  return type_new(TY_UINT, 4, 4, true);
}

Type *type_bool() {
  return type_new(TY_BOOL, 1, 1, true);
}

Type *type_pointer(Type *pointer_to) {
  Type *type = type_new(TY_POINTER, 8, 8, true);
  type->pointer_to = pointer_to;
  return type;
}

Type *type_array_incomplete(Type *array_of) {
  Type *type = type_new(TY_ARRAY, 0, array_of->align, false);
  type->array_of = array_of;
  return type;
}

Type *type_array(Type *type, int length) {
  type->size = type->array_of->size * length;
  type->align = type->array_of->align;
  type->complete = true;
  type->length = length;
  return type;
}

Type *type_function(Type *returning, Vector *params, bool ellipsis) {
  Type *type = type_new(TY_FUNCTION, 0, 1, true);
  type->returning = returning;
  type->params = params;
  type->ellipsis = ellipsis;
  return type;
}

Type *type_struct_incomplete() {
  return type_new(TY_STRUCT, 0, 1, false);
}

Type *type_struct(Type *type, Vector *symbols) {
  Map *members = map_new();
  int size = 0;
  int align = 0;

  for (int i = 0; i < symbols->length; i++) {
    Symbol *symbol = symbols->buffer[i];
    Type *type = symbol->type;

    // add padding for the member alignment
    if (size % type->align != 0) {
      size = size / type->align * type->align + type->align;
    }

    Member *member = calloc(1, sizeof(Member));
    member->type = type;
    member->offset = size;
    map_put(members, symbol->identifier, member);

    // add member size
    size += type->size;

    // align of the struct is max value of member's aligns
    if (align < type->align) {
      align = type->align;
    }
  }

  // add padding for the struct alignment
  if (size % align != 0) {
    size = size / align * align + align;
  }

  type->size = size;
  type->align = align;
  type->complete = true;
  type->members = members;
  return type;
}

// typedef struct {
//   int gp_offset;
//   int fp_offset;
//   void *overflow_arg_area;
//   void *reg_save_area;
// } va_list[1];
Type *type_va_list() {
  Vector *symbols = vector_new();

  Symbol *gp_offset = symbol_variable("gp_offset", NULL);
  gp_offset->type = type_int();
  vector_push(symbols, gp_offset);

  Symbol *fp_offset = symbol_variable("fp_offset", NULL);
  fp_offset->type = type_int();
  vector_push(symbols, fp_offset);

  Symbol *overflow_arg_area = symbol_variable("overflow_arg_area", NULL);
  overflow_arg_area->type = type_pointer(type_void());
  vector_push(symbols, overflow_arg_area);

  Symbol *reg_save_area = symbol_variable("reg_save_area", NULL);
  reg_save_area->type = type_pointer(type_void());
  vector_push(symbols, reg_save_area);

  Type *type = type_struct_incomplete();
  type = type_struct(type, symbols);
  type = type_array_incomplete(type);
  type = type_array(type, 1);

  return type;
}

bool check_integer(Type *type) {
  if (type->ty_type == TY_CHAR) return true;
  if (type->ty_type == TY_UCHAR) return true;
  if (type->ty_type == TY_SHORT) return true;
  if (type->ty_type == TY_USHORT) return true;
  if (type->ty_type == TY_INT) return true;
  if (type->ty_type == TY_UINT) return true;
  if (type->ty_type == TY_BOOL) return true;
  return false;
}

// Originally, floating pointe types are also included
// in arithmetic type, but we do not support them yet.
bool check_arithmetic(Type *type) {
  return check_integer(type);
}

bool check_pointer(Type *type) {
  return type->ty_type == TY_POINTER;
}

bool check_scalar(Type *type) {
  return check_arithmetic(type) || check_pointer(type);
}

bool check_function(Type *type) {
  return type->ty_type == TY_FUNCTION;
}

bool check_struct(Type *type) {
  return type->ty_type == TY_STRUCT;
}

bool check_same(Type *type1, Type *type2) {
  return type1->ty_type == type2->ty_type;
}
