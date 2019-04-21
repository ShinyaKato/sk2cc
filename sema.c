#include "cc.h"

typedef struct decl_attribution {
  Type *type;
  bool sp_extern;
  bool sp_static;
  bool sp_noreturn;
} DeclAttribution;

// symbols

static int stack_size;

static void put_variable(DeclAttribution *attr, Symbol *symbol, bool global) {
  if (symbol->prev && symbol->prev->definition) {
    ERROR(symbol->token, "duplicated declaration: %s.", symbol->identifier);
  }

  if (global) {
    if (attr && attr->sp_extern) {
      symbol->link = LN_EXTERNAL;
      symbol->definition = false;
    } else if (attr && attr->sp_static) {
      symbol->link = LN_INTERNAL;
      symbol->definition = symbol->sy_type == SY_VARIABLE && symbol->type->ty_type != TY_FUNCTION;
    } else {
      symbol->link = LN_EXTERNAL;
      symbol->definition = symbol->sy_type == SY_VARIABLE && symbol->type->ty_type != TY_FUNCTION;
    }
  } else {
    if (attr && attr->sp_extern) {
      ERROR(symbol->token, "'extern' specifier for local variable is not supported.");
    } else if (attr && attr->sp_static) {
      ERROR(symbol->token, "'static' specifier for local variable is not supported.");
    } else {
      symbol->link = LN_NONE;
      symbol->definition = symbol->sy_type == SY_VARIABLE && symbol->type->ty_type != TY_FUNCTION;
    }
  }

  if (!global) {
    stack_size += symbol->type->size;
    if (stack_size % 8 != 0) {
      stack_size += 8 - stack_size % 8;
    }

    symbol->offset = stack_size;
  }
}

// tags

static Vector *tag_scopes; // Vector<Map<Type*>*>

static void put_tag(char *tag, Type *type, Token *token) {
  Map *map = vector_last(tag_scopes);
  map_put(map, tag, type);
}

static Type *lookup_tag(char *tag) {
  for (int i = tag_scopes->length - 1; i >= 0; i--) {
    Map *map = tag_scopes->buffer[i];
    Type *type = map_lookup(map, tag);
    if (type) return type;
  }

  return NULL;
}

// types

static Type *type_new(TypeType ty_type, int size, int align, bool complete) {
  Type *type = calloc(1, sizeof(Type));
  type->ty_type = ty_type;
  type->size = size;
  type->align = align;
  type->complete = complete;
  type->original = type;
  return type;
}

static Type *type_void(void) {
  return type_new(TY_VOID, 0, 1, false);
}

static Type *type_char(void) {
  return type_new(TY_CHAR, 1, 1, true);
}

static Type *type_uchar(void) {
  return type_new(TY_UCHAR, 1, 1, true);
}

static Type *type_short(void) {
  return type_new(TY_SHORT, 2, 2, true);
}

static Type *type_ushort(void) {
  return type_new(TY_USHORT, 2, 2, true);
}

static Type *type_int(void) {
  return type_new(TY_INT, 4, 4, true);
}

static Type *type_uint(void) {
  return type_new(TY_UINT, 4, 4, true);
}

static Type *type_long(void) {
  return type_new(TY_LONG, 8, 8, true);
}

static Type *type_ulong(void) {
  return type_new(TY_ULONG, 8, 8, true);
}

static Type *type_bool(void) {
  return type_new(TY_BOOL, 1, 1, true);
}

static Type *type_pointer(Type *pointer_to) {
  Type *type = type_new(TY_POINTER, 8, 8, true);
  type->pointer_to = pointer_to;
  return type;
}

static Type *type_array_incomplete(Type *array_of) {
  Type *type = type_new(TY_ARRAY, 0, array_of->align, false);
  type->array_of = array_of;
  return type;
}

static Type *type_array(Type *type, int length) {
  type->size = type->array_of->size * length;
  type->align = type->array_of->align;
  type->complete = true;
  type->length = length;
  return type;
}

static Type *type_function(Type *returning, Vector *params, bool ellipsis) {
  Type *type = type_new(TY_FUNCTION, 0, 1, true);
  type->returning = returning;
  type->params = params;
  type->ellipsis = ellipsis;
  return type;
}

static Type *type_struct_incomplete(void) {
  return type_new(TY_STRUCT, 0, 1, false);
}

static Type *type_struct(Type *type, Vector *symbols) {
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
static Type *type_va_list(void) {
  Vector *symbols = vector_new();

  Symbol *gp_offset = calloc(1, sizeof(Symbol));
  gp_offset->sy_type = SY_VARIABLE;
  gp_offset->identifier = "gp_offset";
  gp_offset->type = type_int();
  vector_push(symbols, gp_offset);

  Symbol *fp_offset = calloc(1, sizeof(Symbol));
  fp_offset->sy_type = SY_VARIABLE;
  fp_offset->identifier = "fp_offset";
  fp_offset->type = type_int();
  vector_push(symbols, fp_offset);

  Symbol *overflow_arg_area = calloc(1, sizeof(Symbol));
  overflow_arg_area->sy_type = SY_VARIABLE;
  overflow_arg_area->identifier = "overflow_arg_area";
  overflow_arg_area->type = type_pointer(type_void());
  vector_push(symbols, overflow_arg_area);

  Symbol *reg_save_area = calloc(1, sizeof(Symbol));
  reg_save_area->sy_type = SY_VARIABLE;
  reg_save_area->identifier = "reg_save_area";
  reg_save_area->type = type_pointer(type_void());
  vector_push(symbols, reg_save_area);

  Type *type = type_struct_incomplete();
  type = type_struct(type, symbols);
  type = type_array_incomplete(type);
  type = type_array(type, 1);

  return type;
}

static bool check_integer(Type *type) {
  if (type->ty_type == TY_CHAR) return true;
  if (type->ty_type == TY_UCHAR) return true;
  if (type->ty_type == TY_SHORT) return true;
  if (type->ty_type == TY_USHORT) return true;
  if (type->ty_type == TY_INT) return true;
  if (type->ty_type == TY_UINT) return true;
  if (type->ty_type == TY_LONG) return true;
  if (type->ty_type == TY_ULONG) return true;
  if (type->ty_type == TY_BOOL) return true;
  return false;
}

// Originally, floating pointe types are also included
// in arithmetic type, but we do not support them yet.
static bool check_arithmetic(Type *type) {
  return check_integer(type);
}

static bool check_pointer(Type *type) {
  return type->ty_type == TY_POINTER;
}

static bool check_scalar(Type *type) {
  return check_arithmetic(type) || check_pointer(type);
}

// expressions

static Expr *expr_identifier(char *identifier, Symbol *symbol, Token *token) {
  Expr *expr = calloc(1, sizeof(Expr));
  expr->nd_type = ND_IDENTIFIER;
  expr->identifier = identifier;
  expr->symbol = symbol;
  expr->token = token;
  return expr;
}

static Expr *expr_integer(unsigned long long int_value, Token *token) {
  Expr *expr = calloc(1, sizeof(Expr));
  expr->nd_type = ND_INTEGER;
  expr->int_value = int_value;
  expr->token = token;
  return expr;
}

static Expr *expr_dot(Expr *_expr, char *member, Token *token) {
  Expr *expr = calloc(1, sizeof(Expr));
  expr->nd_type = ND_DOT;
  expr->expr = _expr;
  expr->member = member;
  expr->token = token;
  return expr;
}

static Expr *expr_cast(TypeName *type_name, Expr *_expr, Token *token) {
  Expr *expr = calloc(1, sizeof(Expr));
  expr->nd_type = ND_CAST;
  expr->expr = _expr;
  expr->type_name = type_name;
  expr->token = token;
  return expr;
}

static Expr *expr_unary(NodeType nd_type, Expr *_expr, Token *token) {
  Expr *expr = calloc(1, sizeof(Expr));
  expr->nd_type = nd_type;
  expr->expr = _expr;
  expr->token = token;
  return expr;
}

static Expr *expr_binary(NodeType nd_type, Expr *lhs, Expr *rhs, Token *token) {
  Expr *expr = calloc(1, sizeof(Expr));
  expr->nd_type = nd_type;
  expr->lhs = lhs;
  expr->rhs = rhs;
  expr->token = token;
  return expr;
}

// semantics of expression

static bool check_lvalue(Expr *expr) {
  if (expr->nd_type == ND_IDENTIFIER) return true;
  if (expr->nd_type == ND_DOT) return true;
  if (expr->nd_type == ND_ARROW) return true;
  if (expr->nd_type == ND_INDIRECT) return true;

  return false;
}

static bool check_cast(Type *type, Expr *expr) {
  if (check_integer(type) && check_integer(expr->type)) {
    return true;
  }

  if (check_pointer(type)) {
    if (check_pointer(expr->type)) {
      return true;
    }
    if (expr->nd_type == ND_INTEGER && expr->int_value == 0) {
      return true;
    }
  }

  // conversion between pointer and integer
  if (check_pointer(type)) {
    if (expr->type->ty_type == TY_LONG || expr->type->ty_type == TY_ULONG) {
      return true;
    }
  }
  if (type->ty_type == TY_LONG || type->ty_type == TY_ULONG) {
    if (check_pointer(expr->type)) {
      return true;
    }
  }

  return false;
}

static Expr *insert_cast(Type *type, Expr *expr, Token *token) {
  Expr *cast = expr_cast(NULL, expr, token);
  cast->type = type;

  if (!check_cast(cast->type, expr)) {
    ERROR(token, "invalid casting.");
  }

  return cast;
}

// integer promotion
static Type *promote_integer(Expr **expr) {
  TypeType ty_types[] = { TY_BOOL, TY_CHAR, TY_UCHAR, TY_SHORT, TY_USHORT };

  for (int i = 0; i < sizeof(ty_types) / sizeof(TypeType); i++) {
    if ((*expr)->type->ty_type == ty_types[i]) {
      *expr = insert_cast(type_int(), *expr, (*expr)->token);
      break;
    }
  }

  return (*expr)->type;
}

// usual arithmetic conversion
static Type *convert_arithmetic(Expr **lhs, Expr **rhs) {
  promote_integer(lhs);
  promote_integer(rhs);

  Type *ltype = (*lhs)->type;
  Type *rtype = (*rhs)->type;

  Type *type = ltype->ty_type > rtype->ty_type ? ltype : rtype;
  (*lhs) = insert_cast(type, *lhs, (*lhs)->token);
  (*rhs) = insert_cast(type, *rhs, (*rhs)->token);

  return type;
}

static Expr *sema_expr(Expr *expr);
static Type *sema_type_name(TypeName *type_name);

static Expr *comp_assign_post(NodeType nd_type, Expr *lhs, Expr *rhs, Token *token) {
  lhs = sema_expr(lhs);

  Symbol *sym_addr = calloc(1, sizeof(Symbol));
  sym_addr->sy_type = SY_VARIABLE;
  sym_addr->link = LN_NONE;
  sym_addr->type = type_pointer(lhs->type);
  sym_addr->token = token;
  put_variable(NULL, sym_addr, false);

  Symbol *sym_val = calloc(1, sizeof(Symbol));
  sym_val->sy_type = SY_VARIABLE;
  sym_val->link = LN_NONE;
  sym_val->type = lhs->type;
  sym_val->token = token;
  put_variable(NULL, sym_val, false);

  Expr *addr_ident = expr_identifier(NULL, sym_addr, token);
  Expr *addr = expr_unary(ND_ADDRESS, lhs, token);
  Expr *addr_assign = expr_binary(ND_ASSIGN, addr_ident, addr, token);

  Expr *val_ident = expr_identifier(NULL, sym_val, token);
  Expr *val = expr_unary(ND_INDIRECT, addr_ident, token);
  Expr *val_assign = expr_binary(ND_ASSIGN, val_ident, val, token);

  Expr *lvalue = expr_unary(ND_INDIRECT, addr_assign, token);
  Expr *op = expr_binary(nd_type, val_assign, rhs, token);
  Expr *assign = expr_binary(ND_ASSIGN, lvalue, op, token);

  Expr *comma = expr_binary(ND_COMMA, assign, val_ident, token);

  return sema_expr(comma);
}

static Expr *comp_assign_pre(NodeType nd_type, Expr *lhs, Expr *rhs, Token *token) {
  lhs = sema_expr(lhs);

  Symbol *sym_addr = calloc(1, sizeof(Symbol));
  sym_addr->sy_type = SY_VARIABLE;
  sym_addr->link = LN_NONE;
  sym_addr->type = type_pointer(lhs->type);
  sym_addr->token = token;
  put_variable(NULL, sym_addr, false);

  Expr *addr_ident = expr_identifier(NULL, sym_addr, token);
  Expr *addr = expr_unary(ND_ADDRESS, lhs, token);
  Expr *addr_assign = expr_binary(ND_ASSIGN, addr_ident, addr, token);

  Expr *val = expr_unary(ND_INDIRECT, addr_ident, token);

  Expr *lvalue = expr_unary(ND_INDIRECT, addr_assign, token);
  Expr *op = expr_binary(nd_type, val, rhs, token);
  Expr *assign = expr_binary(ND_ASSIGN, lvalue, op, token);

  return sema_expr(assign);
}

static Expr *sema_va_start(Expr *expr) {
  expr->macro_ap = sema_expr(expr->macro_ap);
  if (expr->macro_ap->type->ty_type != TY_POINTER) {
    ERROR(expr->macro_ap->token, "invalid argument of 'va_start'.");
  }

  expr->type = type_void();

  return expr;
}

static Expr *sema_va_arg(Expr *expr) {
  expr->macro_ap = sema_expr(expr->macro_ap);
  if (expr->macro_ap->type->ty_type != TY_POINTER) {
    ERROR(expr->macro_ap->token, "invalid argument of 'va_arg'.");
  }

  expr->type = sema_type_name(expr->macro_type);

  return expr;
}

static Expr *sema_va_end(Expr *expr) {
  expr->macro_ap = sema_expr(expr->macro_ap);
  if (expr->macro_ap->type->ty_type != TY_POINTER) {
    ERROR(expr->macro_ap->token, "invalid argument of 'va_end'.");
  }

  expr->type = type_void();

  return expr;
}

static Expr *sema_identifier(Expr *expr) {
  if (expr->symbol) {
    expr->type = expr->symbol->type;
  } else {
    ERROR(expr->token, "undefined variable: %s.", expr->identifier);
  }

  return expr;
}

static Expr *sema_integer(Expr *expr) {
  if (!expr->int_decimal) {
    if (!expr->int_u) {
      if (!expr->int_l && !expr->int_ll) {
        if (expr->int_value <= 0x7fffffff) {
          expr->type = type_int();
        } else if (expr->int_value <= 0xffffffff) {
          expr->type = type_uint();
        } else if (expr->int_value <= 0x7fffffffffffffff) {
          expr->type = type_long();
        } else {
          expr->type = type_ulong();
        }
      } else {
        if (expr->int_value <= 0x7fffffffffffffff) {
          expr->type = type_long();
        } else {
          expr->type = type_ulong();
        }
      }
    } else {
      if (!expr->int_l && !expr->int_ll) {
        if (expr->int_value <= 0xffffffff) {
          expr->type = type_uint();
        } else {
          expr->type = type_ulong();
        }
      } else {
        expr->type = type_ulong();
      }
    }
  } else {
    if (!expr->int_u) {
      if (!expr->int_l && !expr->int_ll) {
        if (expr->int_value <= 0x7fffffff) {
          expr->type = type_int();
        } else if (expr->int_value <= 0x7fffffffffffffff) {
          expr->type = type_long();
        } else {
          ERROR(expr->token, "can not represents integer-constant.");
        }
      } else {
        if (expr->int_value <= 0x7fffffffffffffff) {
          expr->type = type_long();
        } else {
          ERROR(expr->token, "can not represents integer-constant.");
        }
      }
    } else {
      if (!expr->int_l && !expr->int_ll) {
        if (expr->int_value <= 0xffffffff) {
          expr->type = type_uint();
        } else {
          expr->type = type_ulong();
        }
      } else {
        expr->type = type_ulong();
      }
    }
  }

  return expr;
}

static Expr *sema_enum_const(Expr *expr) {
  return sema_expr(expr_integer(expr->symbol->const_value, expr->token));
}

static Expr *sema_string(Expr *expr) {
  int length = expr->string_literal->length;
  expr->type = type_array(type_array_incomplete(type_char()), length);

  return expr;
}

// convert expr[index] to *(expr + index)
static Expr *sema_subscription(Expr *expr) {
  expr->expr = sema_expr(expr->expr);
  expr->index = sema_expr(expr->index);

  Expr *add = expr_binary(ND_ADD, expr->expr, expr->index, expr->token);
  Expr *ref = expr_unary(ND_INDIRECT, add, expr->token);

  return sema_expr(ref);
}

static Expr *sema_call(Expr *expr) {
  if (expr->expr->nd_type == ND_IDENTIFIER) {
    if (expr->expr->symbol) {
      expr->expr = sema_expr(expr->expr);
    }
  } else {
    ERROR(expr->token, "invalid function call.");
  }

  for (int i = 0; i < expr->args->length; i++) {
    expr->args->buffer[i] = sema_expr(expr->args->buffer[i]);
  }

  if (expr->expr->symbol) {
    Vector *params = expr->expr->type->params;
    Vector *args = expr->args;

    if (!expr->expr->symbol->type->ellipsis) {
      if (args->length != params->length) {
        ERROR(expr->token, "number of parameters should be %d, but got %d.", params->length, args->length);
      }
    } else {
      if (args->length < params->length) {
        ERROR(expr->token, "number of parameters should be %d or more, bug got %d.", params->length, args->length);
      }
    }

    for (int i = 0; i < args->length; i++) {
      promote_integer((Expr **) &args->buffer[i]);
    }
    for (int i = 0; i < params->length; i++) {
      Symbol *param = params->buffer[i];
      args->buffer[i] = insert_cast(param->type, args->buffer[i], expr->token);
    }
  }

  if (expr->expr->symbol) {
    if (expr->expr->type->ty_type != TY_FUNCTION) {
      ERROR(expr->token, "operand should have function type.");
    }
    expr->type = expr->expr->type->returning;
  } else {
    expr->type = type_int();
  }

  return expr;
}

static Expr *sema_dot(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (expr->expr->type->ty_type != TY_STRUCT) {
    ERROR(expr->token, "operand should have struct type.");
  }

  Member *member = map_lookup(expr->expr->type->members, expr->member);
  if (!member) {
    ERROR(expr->token, "undefined struct member: %s.", expr->member);
  }

  expr->type = member->type;
  expr->offset = member->offset;

  return expr;
}

// convert expr->member to (*expr).member
static Expr *sema_arrow(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  Expr *ref = expr_unary(ND_INDIRECT, expr->expr, expr->token);
  Expr *dot = expr_dot(ref, expr->member, expr->token);

  return sema_expr(dot);
}

static Expr *sema_post_inc(Expr *expr) {
  Expr *int_const = expr_integer(1, expr->token);
  return comp_assign_post(ND_ADD, expr->expr, int_const, expr->token);
}

static Expr *sema_post_dec(Expr *expr) {
  Expr *int_const = expr_integer(1, expr->token);
  return comp_assign_post(ND_SUB, expr->expr, int_const, expr->token);
}

static Expr *sema_pre_inc(Expr *expr) {
  Expr *int_const = expr_integer(1, expr->token);
  return comp_assign_pre(ND_ADD, expr->expr, int_const, expr->token);
}

static Expr *sema_pre_dec(Expr *expr) {
  Expr *int_const = expr_integer(1, expr->token);
  return comp_assign_pre(ND_SUB, expr->expr, int_const, expr->token);
}

static Expr *sema_address(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_lvalue(expr->expr)) {
    expr->type = type_pointer(expr->expr->type);
  } else {
    ERROR(expr->token, "operand should be lvalue.");
  }

  return expr;
}

static Expr *sema_indirect(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_pointer(expr->expr->type)) {
    expr->type = expr->expr->type->pointer_to;
  } else {
    ERROR(expr->token, "operand should have pointer type.");
  }

  return expr;
}

static Expr *sema_uplus(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_arithmetic(expr->expr->type)) {
    promote_integer(&expr->expr);
  } else {
    ERROR(expr->token, "operand should have arithmetic type.");
  }

  // we can remove node of unary + before code generation.
  return expr->expr;
}

static Expr *sema_uminus(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_arithmetic(expr->expr->type)) {
    expr->type = promote_integer(&expr->expr);
  } else {
    ERROR(expr->token, "operand should have arithmetic type.");
  }

  return expr;
}

static Expr *sema_not(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_integer(expr->expr->type)) {
    expr->type = promote_integer(&expr->expr);
  } else {
    ERROR(expr->token, "operand should have integer type.");
  }

  return expr;
}

static Expr *sema_lnot(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_scalar(expr->expr->type)) {
    promote_integer(&expr->expr);
    expr->type = type_int();
  } else {
    ERROR(expr->token, "operand should have scalar type.");
  }

  return expr;
}

// convert sizeof to integer-constant
static Expr *sema_sizeof(Expr *expr) {
  Type *type = expr->expr ? sema_expr(expr->expr)->type : sema_type_name(expr->type_name);

  if (type->original->size == 0) {
    ERROR(expr->token, "invalid operand of sizeof operator.");
  }

  return sema_expr(expr_integer(type->original->size, expr->token));
}

// convert alignof to integer-constant
static Expr *sema_alignof(Expr *expr) {
  Type *type = sema_type_name(expr->type_name);

  if (type->align == 0) {
    ERROR(expr->token, "invalid operand of _Alignof operator.");
  }

  return sema_expr(expr_integer(type->align, expr->token));
}

static Expr *sema_cast(Expr *expr) {
  expr->expr = sema_expr(expr->expr);
  expr->type = sema_type_name(expr->type_name);

  if (!check_cast(expr->type, expr->expr)) {
    ERROR(expr->token, "invalid casting.");
  }

  return expr;
}

static Expr *sema_mul(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else {
    ERROR(expr->token, "operands should have intger type.");
  }

  return expr;
}

static Expr *sema_mod(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else {
    ERROR(expr->token, "operands should have intger type.");
  }

  return expr;
}

static Expr *sema_add(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else if (check_pointer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    promote_integer(&expr->rhs);
    expr->type = expr->lhs->type;
  } else if (check_integer(expr->lhs->type) && check_pointer(expr->rhs->type)) {
    Expr *tmp = expr->lhs;
    expr->lhs = expr->rhs;
    expr->rhs = tmp;
    promote_integer(&expr->rhs);
    expr->type = expr->lhs->type;
  } else {
    ERROR(expr->token, "invalid operand types.");
  }

  return expr;
}

static Expr *sema_sub(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else if (check_pointer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    promote_integer(&expr->rhs);
    expr->type = expr->lhs->type;
  } else {
    ERROR(expr->token, "invalid operand types.");
  }

  return expr;
}

static Expr *sema_shift(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else {
    ERROR(expr->token, "invalid operand types.");
  }

  return expr;
}

static Expr *sema_relational(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    convert_arithmetic(&expr->lhs, &expr->rhs);
    expr->type = type_int();
  } else if (check_pointer(expr->lhs->type) && check_pointer(expr->rhs->type)) {
    expr->type = type_int();
  } else {
    ERROR(expr->token, "invalid operand types.");
  }

  // convert '<' to '>', '<=' to '>='
  if (expr->nd_type == ND_GT) {
    Expr *tmp = expr->lhs;
    expr->lhs = expr->rhs;
    expr->rhs = tmp;
    expr->nd_type = ND_LT;
  }
  if (expr->nd_type == ND_GTE) {
    Expr *tmp = expr->lhs;
    expr->lhs = expr->rhs;
    expr->rhs = tmp;
    expr->nd_type = ND_LTE;
  }

  return expr;
}

static Expr *sema_equality(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    convert_arithmetic(&expr->lhs, &expr->rhs);
    expr->type = type_int();
  } else if (check_pointer(expr->lhs->type) && check_pointer(expr->rhs->type)) {
    expr->type = type_int();
  } else {
    ERROR(expr->token, "invalid operand types.");
  }

  return expr;
}

static Expr *sema_bitwise(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else {
    ERROR(expr->token, "invalid operand types.");
  }

  return expr;
}

static Expr *sema_logical(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_scalar(expr->lhs->type) && check_scalar(expr->rhs->type)) {
    promote_integer(&expr->lhs);
    promote_integer(&expr->rhs);
    expr->type = type_int();
  } else {
    ERROR(expr->token, "invalid operand types.");
  }

  return expr;
}

static Expr *sema_condition(Expr *expr) {
  expr->cond = sema_expr(expr->cond);
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_scalar(expr->cond->type)) {
    promote_integer(&expr->cond);
  } else {
    ERROR(expr->token, "control expression should have scalar type.");
  }

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else if (check_pointer(expr->lhs->type) && check_pointer(expr->rhs->type)) {
    expr->type = expr->lhs->type;
  } else {
    ERROR(expr->token, "invalid operand types.");
  }

  return expr;
}

static Expr *sema_assign(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_lvalue(expr->lhs)) {
    expr->rhs = insert_cast(expr->lhs->type, expr->rhs, expr->token);
    expr->type = expr->lhs->type;
  } else {
    ERROR(expr->token, "invalid operands.");
  }

  return expr;
}

static Expr *sema_mul_assign(Expr *expr) {
  return comp_assign_pre(ND_MUL, expr->lhs, expr->rhs, expr->token);
}

static Expr *sema_div_assign(Expr *expr) {
  return comp_assign_pre(ND_DIV, expr->lhs, expr->rhs, expr->token);
}

static Expr *sema_mod_assign(Expr *expr) {
  return comp_assign_pre(ND_MOD, expr->lhs, expr->rhs, expr->token);
}

static Expr *sema_add_assign(Expr *expr) {
  return comp_assign_pre(ND_ADD, expr->lhs, expr->rhs, expr->token);
}

static Expr *sema_sub_assign(Expr *expr) {
  return comp_assign_pre(ND_SUB, expr->lhs, expr->rhs, expr->token);
}

static Expr *sema_comma(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  expr->type = expr->rhs->type;

  return expr;
}

static Expr *sema_expr(Expr *expr) {
  // skip if the expression is already analyzed
  if (expr->type) return expr;

  switch (expr->nd_type) {
    // builtins
    case ND_VA_START:
      expr = sema_va_start(expr);
      break;
    case ND_VA_ARG:
      expr = sema_va_arg(expr);
      break;
    case ND_VA_END:
      expr = sema_va_end(expr);
      break;

    // primary expressions
    case ND_IDENTIFIER:
      expr = sema_identifier(expr);
      break;
    case ND_INTEGER:
      expr = sema_integer(expr);
      break;
    case ND_ENUM_CONST:
      expr = sema_enum_const(expr);
      break;
    case ND_STRING:
      expr = sema_string(expr);
      break;

    // postfix operators
    case ND_SUBSCRIPTION:
      expr = sema_subscription(expr);
      break;
    case ND_CALL:
      expr = sema_call(expr);
      break;
    case ND_DOT:
      expr = sema_dot(expr);
      break;
    case ND_ARROW:
      expr = sema_arrow(expr);
      break;
    case ND_POST_INC:
      expr = sema_post_inc(expr);
      break;
    case ND_POST_DEC:
      expr = sema_post_dec(expr);
      break;

    // unary operators
    case ND_PRE_INC:
      expr = sema_pre_inc(expr);
      break;
    case ND_PRE_DEC:
      expr = sema_pre_dec(expr);
      break;
    case ND_ADDRESS:
      expr = sema_address(expr);
      break;
    case ND_INDIRECT:
      expr = sema_indirect(expr);
      break;
    case ND_UPLUS:
      expr = sema_uplus(expr);
      break;
    case ND_UMINUS:
      expr = sema_uminus(expr);
      break;
    case ND_NOT:
      expr = sema_not(expr);
      break;
    case ND_LNOT:
      expr = sema_lnot(expr);
      break;
    case ND_SIZEOF:
      expr = sema_sizeof(expr);
      break;
    case ND_ALIGNOF:
      expr = sema_alignof(expr);
      break;

    // cast operators
    case ND_CAST:
      expr = sema_cast(expr);
      break;

    // *, /, % operators
    case ND_MUL:
    case ND_DIV:
      expr = sema_mul(expr);
      break;
    case ND_MOD:
      expr = sema_mod(expr);
      break;

    // +, - operators
    case ND_ADD:
      expr = sema_add(expr);
      break;
    case ND_SUB:
      expr = sema_sub(expr);
      break;

    // <<, >> operators
    case ND_LSHIFT:
    case ND_RSHIFT:
      expr = sema_shift(expr);
      break;

    // <, >, <=, >= operators
    case ND_LT:
    case ND_GT:
    case ND_LTE:
    case ND_GTE:
      expr = sema_relational(expr);
      break;

    // ==, != operators
    case ND_EQ:
    case ND_NEQ:
      expr = sema_equality(expr);
      break;

    // &, ^, | operators
    case ND_AND:
    case ND_XOR:
    case ND_OR:
      expr = sema_bitwise(expr);
      break;

    // &&, || operators
    case ND_LAND:
    case ND_LOR:
      expr = sema_logical(expr);
      break;

    // ?: operator
    case ND_CONDITION:
      expr = sema_condition(expr);
      break;

    // assignment operators
    case ND_ASSIGN:
      expr = sema_assign(expr);
      break;
    case ND_MUL_ASSIGN:
      expr = sema_mul_assign(expr);
      break;
    case ND_DIV_ASSIGN:
      expr = sema_div_assign(expr);
      break;
    case ND_MOD_ASSIGN:
      expr = sema_mod_assign(expr);
      break;
    case ND_ADD_ASSIGN:
      expr = sema_add_assign(expr);
      break;
    case ND_SUB_ASSIGN:
      expr = sema_sub_assign(expr);
      break;

    // comma operator
    case ND_COMMA:
      expr = sema_comma(expr);
      break;

    // unreachable
    default:
      internal_error("unknown expression type.");
  }

  // lvalue promotion (convert array to pointer)
  if (expr->type->ty_type == TY_ARRAY) {
    Type *pointer = type_pointer(expr->type->array_of);
    pointer->original = expr->type;
    expr->type = pointer;
  }

  return expr;
}

static Expr *sema_const_expr(Expr *expr) {
  expr = sema_expr(expr);

  if (expr->nd_type != ND_INTEGER) {
    ERROR(expr->token, "invalid integer constant expression.");
  }

  return expr;
}

// semantics of declaration

static void sema_decl(Decl *decl, bool global);
static DeclAttribution *sema_specs(Vector *specs, Token *token);
static Type *sema_struct(Specifier *spec);
static Type *sema_enum(Specifier *spec);
static Type *sema_typedef_name(Specifier *spec);
static Type *sema_declarator(Declarator *decl, Type *type);
static Type *sema_type_name(TypeName *type_name);
static void sema_initializer(Initializer *init, Type *type, bool global);

static void sema_decl(Decl *decl, bool global) {
  DeclAttribution *attr = sema_specs(decl->specs, decl->token);

  for (int i = 0; i < decl->symbols->length; i++) {
    Symbol *symbol = decl->symbols->buffer[i];
    symbol->type = sema_declarator(symbol->decl, attr->type);

    if (symbol->init) {
      sema_initializer(symbol->init, symbol->type, global);

      if (symbol->type->ty_type == TY_ARRAY) {
        if (symbol->type->complete) {
          if (symbol->type->length < symbol->init->list->length) {
            ERROR(symbol->token, "too many initializer items.");
          }
        } else {
          type_array(symbol->type, symbol->init->list->length);
        }
      }
    }

    if (symbol->sy_type == SY_VARIABLE) {
      put_variable(attr, symbol, global);
    }
  }
}

static DeclAttribution *sema_specs(Vector *specs, Token *token) {
  int sp_storage = 0;
  bool sp_extern = false;
  bool sp_static = false;

  int sp_type = 0;
  int sp_void = 0;
  int sp_char = 0;
  int sp_short = 0;
  int sp_int = 0;
  int sp_long = 0;
  int sp_signed = 0;
  int sp_unsigned = 0;
  int sp_bool = 0;
  Vector *sp_struct = vector_new();
  Vector *sp_enum = vector_new();
  Vector *sp_typedef = vector_new();

  bool sp_noreturn = false;

  for (int i = 0; i < specs->length; i++) {
    Specifier *spec = specs->buffer[i];
    if (spec->sp_type == SP_TYPEDEF) {
      sp_storage++;
    } else if (spec->sp_type == SP_EXTERN) {
      sp_storage++;
      sp_extern = true;
    } else if (spec->sp_type == SP_STATIC) {
      sp_storage++;
      sp_static = true;
    } else if (spec->sp_type == SP_VOID) {
      sp_type++;
      sp_void++;
    } else if (spec->sp_type == SP_CHAR) {
      sp_type++;
      sp_char++;
    } else if (spec->sp_type == SP_SHORT) {
      sp_type++;
      sp_short++;
    } else if (spec->sp_type == SP_INT) {
      sp_type++;
      sp_int++;
    } else if (spec->sp_type == SP_LONG) {
      sp_type++;
      sp_long++;
    } else if (spec->sp_type == SP_SIGNED) {
      sp_type++;
      sp_signed++;
    } else if (spec->sp_type == SP_UNSIGNED) {
      sp_type++;
      sp_unsigned++;
    } else if (spec->sp_type == SP_BOOL) {
      sp_type++;
      sp_bool++;
    } else if (spec->sp_type == SP_STRUCT) {
      sp_type++;
      vector_push(sp_struct, sema_struct(spec));
    } else if (spec->sp_type == SP_ENUM) {
      sp_type++;
      vector_push(sp_enum, sema_enum(spec));
    } else if (spec->sp_type == SP_TYPEDEF_NAME) {
      sp_type++;
      vector_push(sp_typedef, sema_typedef_name(spec));
    } else if (spec->sp_type == SP_NORETURN) {
      sp_noreturn = true;
    }
  }

  if (sp_storage > 1) {
    ERROR(token, "too many storage-class-specifiers.");
  }

  Type *type;
  if (sp_type == 1 && sp_void == 1) {
    type = type_void();
  } else if (sp_type == 1 && sp_char == 1) {
    type = type_char();
  } else if (sp_type == 2 && sp_signed == 1 && sp_char) {
    type = type_char();
  } else if (sp_type == 2 && sp_unsigned == 1 && sp_char == 1) {
    type = type_uchar();
  } else if (sp_type == 1 && sp_short == 1) {
    type = type_short();
  } else if (sp_type == 2 && sp_signed == 1 && sp_short == 1) {
    type = type_short();
  } else if (sp_type == 2 && sp_short == 1 && sp_int == 1) {
    type = type_short();
  } else if (sp_type == 3 && sp_signed == 1 && sp_short == 1 && sp_int == 1) {
    type = type_short();
  } else if (sp_type == 2 && sp_unsigned == 1 && sp_short == 1) {
    type = type_ushort();
  } else if (sp_type == 3 && sp_unsigned == 1 && sp_short == 1 && sp_int == 1) {
    type = type_ushort();
  } else if (sp_type == 1 && sp_int == 1) {
    type = type_int();
  } else if (sp_type == 1 && sp_signed == 1) {
    type = type_int();
  } else if (sp_type == 2 && sp_signed == 1 && sp_int == 1) {
    type = type_int();
  } else if (sp_type == 1 && sp_unsigned == 1) {
    type = type_uint();
  } else if (sp_type == 2 && sp_unsigned == 1 && sp_int == 1) {
    type = type_uint();
  } else if (sp_type == 1 && sp_long == 1) {
    type = type_long();
  } else if (sp_type == 2 && sp_signed == 1 && sp_long == 1) {
    type = type_long();
  } else if (sp_type == 2 && sp_long == 1 && sp_int == 1) {
    type = type_long();
  } else if (sp_type == 3 && sp_signed == 1 && sp_long == 1 && sp_int == 1) {
    type = type_long();
  } else if (sp_type == 2 && sp_unsigned == 1 && sp_long == 1) {
    type = type_ulong();
  } else if (sp_type == 3 && sp_unsigned == 1 && sp_long == 1 && sp_int == 1) {
    type = type_ulong();
  } else if (sp_type == 2 && sp_long == 2) {
    type = type_long();
  } else if (sp_type == 3 && sp_signed == 1 && sp_long == 2) {
    type = type_long();
  } else if (sp_type == 3 && sp_long == 2 && sp_int == 1) {
    type = type_long();
  } else if (sp_type == 4 && sp_signed == 1 && sp_long == 2 && sp_int == 1) {
    type = type_long();
  } else if (sp_type == 3 && sp_unsigned == 1 && sp_long == 2) {
    type = type_ulong();
  } else if (sp_type == 4 && sp_unsigned == 1 && sp_long == 2 && sp_int == 1) {
    type = type_ulong();
  } else if (sp_type == 1 && sp_bool == 1) {
    type = type_bool();
  } else if (sp_type == 1 && sp_struct->length == 1) {
    type = sp_struct->buffer[0];
  } else if (sp_type == 1 && sp_enum->length == 1) {
    type = sp_enum->buffer[0];
  } else if (sp_type == 1 && sp_typedef->length == 1) {
    type = sp_typedef->buffer[0];
  } else {
    ERROR(token, "invalid type-specifiers.");
  }

  DeclAttribution *attr = calloc(1, sizeof(DeclAttribution));
  attr->type = type;
  attr->sp_extern = sp_extern;
  attr->sp_static = sp_static;
  attr->sp_noreturn = sp_noreturn;
  return attr;
}

static Type *sema_struct(Specifier *spec) {
  if (spec->struct_decls) {
    Type *type = NULL;
    if (spec->struct_tag) {
      type = lookup_tag(spec->struct_tag);
      if (!type) {
        type = type_struct_incomplete();
        put_tag(spec->struct_tag, type, spec->token);
      }
    } else {
      type = type_struct_incomplete();
    }

    Vector *symbols = vector_new();
    for (int i = 0; i < spec->struct_decls->length; i++) {
      Decl *decl = spec->struct_decls->buffer[i];
      DeclAttribution *attr = sema_specs(decl->specs, decl->token);

      for (int j = 0; j < decl->symbols->length; j++) {
        Symbol *symbol = decl->symbols->buffer[j];
        symbol->type = sema_declarator(symbol->decl, attr->type);

        if (!symbol->type->complete) {
          ERROR(symbol->token, "declaration of struct member with incomplete type.");
        }

        vector_push(symbols, symbol);
      }
    }

    return type_struct(type, symbols);
  }

  Type *type = lookup_tag(spec->struct_tag);
  if (!type) {
    type = type_struct_incomplete();
    put_tag(spec->struct_tag, type, spec->token);
  }
  return type;
}

static Type *sema_enum(Specifier *spec) {
  if (spec->enums) {
    int const_value = 0;
    for (int i = 0; i < spec->enums->length; i++) {
      Symbol *symbol = spec->enums->buffer[i];
      if (symbol->const_expr) {
        symbol->const_expr = sema_const_expr(symbol->const_expr);
        const_value = symbol->const_expr->int_value;
      }
      symbol->const_value = const_value++;
    }

    if (spec->enum_tag) {
      if (lookup_tag(spec->enum_tag)) {
        ERROR(spec->token, "redeclaration of enum tag.");
      }

      Type *type = type_int();
      put_tag(spec->enum_tag, type, spec->token);
      return type;
    }

    return type_int();
  }

  Type *type = lookup_tag(spec->enum_tag);
  if (!type) {
    ERROR(spec->token, "undefined enum tag.");
  }
  return type;
}

static Type *sema_typedef_name(Specifier *spec) {
  Symbol *symbol = spec->typedef_symbol;

  if (strcmp(symbol->identifier, "__builtin_va_list") == 0) {
    return type_va_list();
  }

  return symbol->type;
}

static Type *sema_declarator(Declarator *decl, Type *type) {
  if (!decl) return type;

  if (decl->decl_type == DECL_POINTER) {
    Type *pointer = type_pointer(type);
    return sema_declarator(decl->decl, pointer);
  }

  if (decl->decl_type == DECL_ARRAY) {
    Type *array = type_array_incomplete(type);
    if (decl->size) {
      if (decl->size->nd_type != ND_INTEGER) {
        ERROR(decl->size->token, "only integer constant is supported for array size.");
      }
      array = type_array(array, decl->size->int_value);
    }
    return sema_declarator(decl->decl, array);
  }

  if (decl->decl_type == DECL_FUNCTION) {
    Vector *params = vector_new();
    for (int i = 0; i < decl->params->length; i++) {
      Decl *param = decl->params->buffer[i];
      DeclAttribution *attr = sema_specs(param->specs, param->token);

      Symbol *symbol = param->symbol;
      symbol->type = sema_declarator(symbol->decl, attr->type);

      // If the declarator includes only 'void', the function takes no parameter.
      if (decl->params->length == 1 && !symbol->identifier && symbol->type->ty_type == TY_VOID) {
        break;
      }

      if (symbol->type->ty_type == TY_ARRAY) {
        symbol->type = type_pointer(symbol->type->array_of);
      }

      vector_push(params, symbol);
    }

    Type *func = type_function(type, params, decl->ellipsis);
    return sema_declarator(decl->decl, func);
  }

  internal_error("invalid declarator");
}

static Type *sema_type_name(TypeName *type_name) {
  DeclAttribution *attr = sema_specs(type_name->specs, type_name->token);

  return sema_declarator(type_name->decl, attr->type);
}

static void sema_initializer(Initializer *init, Type *type, bool global) {
  init->type = type;

  if (init->list) {
    for (int i = 0; i < init->list->length; i++) {
      sema_initializer(init->list->buffer[i], type->array_of, global);
    }
  } else {
    if (global) {
      if (init->expr->nd_type != ND_INTEGER && init->expr->nd_type != ND_STRING) {
        ERROR(init->expr->token, "initializer expression should be integer constant or string literal.");
      }
    }
    init->expr = sema_expr(init->expr);
    init->expr = insert_cast(type, init->expr, init->token);
  }
}

// semantics of statement

static Vector *label_stmts;
static Vector *goto_stmts;
static Vector *switch_stmts;
static Vector *continue_targets;
static Vector *break_targets;
static Func *ret_func;

static void switch_begin(Stmt *stmt) {
  stmt->switch_cases = vector_new();
  vector_push(switch_stmts, stmt);
  vector_push(break_targets, stmt);
}

static void switch_end(void) {
  vector_pop(switch_stmts);
  vector_pop(break_targets);
}

static void loop_begin(Stmt *stmt) {
  vector_push(tag_scopes, map_new());
  vector_push(continue_targets, stmt);
  vector_push(break_targets, stmt);
}

static void loop_end(void) {
  vector_pop(tag_scopes);
  vector_pop(continue_targets);
  vector_pop(break_targets);
}

static void sema_stmt(Stmt *stmt);

static void sema_label(Stmt *stmt) {
  for (int i = 0; i < label_stmts->length; i++) {
    Stmt *label_stmt = label_stmts->buffer[i];
    char *label_ident = label_stmt->label_ident;
    if (strcmp(stmt->label_ident, label_ident) == 0) {
      ERROR(stmt->token, "duplicated label declaration: %s.", stmt->label_ident);
    }
  }
  vector_push(label_stmts, stmt);

  sema_stmt(stmt->label_stmt);
}

static void sema_case(Stmt *stmt) {
  if (switch_stmts->length > 0) {
    Stmt *switch_stmt = vector_last(switch_stmts);
    vector_push(switch_stmt->switch_cases, stmt);
  } else {
    ERROR(stmt->token, "'case' should appear in switch statement.");
  }

  stmt->case_const = sema_expr(stmt->case_const);

  sema_stmt(stmt->case_stmt);
}

static void sema_default(Stmt *stmt) {
  if (switch_stmts->length > 0) {
    Stmt *switch_stmt = vector_last(switch_stmts);
    vector_push(switch_stmt->switch_cases, stmt);
  } else {
    ERROR(stmt->token, "'default' should appear in switch statement.");
  }

  sema_stmt(stmt->default_stmt);
}

static void sema_if(Stmt *stmt) {
  stmt->if_cond = sema_expr(stmt->if_cond);
  if (check_scalar(stmt->if_cond->type)) {
    promote_integer(&stmt->if_cond);
  } else {
    ERROR(stmt->token, "control expression should have scalar type.");
  }

  sema_stmt(stmt->then_body);

  if (stmt->else_body) {
    sema_stmt(stmt->else_body);
  }
}

static void sema_switch(Stmt *stmt) {
  switch_begin(stmt);

  stmt->switch_cond = sema_expr(stmt->switch_cond);
  if (check_scalar(stmt->switch_cond->type)) {
    promote_integer(&stmt->switch_cond);
  } else {
    ERROR(stmt->token, "control expression should have scalar type.");
  }

  sema_stmt(stmt->switch_body);

  switch_end();
}

static void sema_while(Stmt *stmt) {
  loop_begin(stmt);

  stmt->while_cond = sema_expr(stmt->while_cond);
  if (check_scalar(stmt->while_cond->type)) {
    promote_integer(&stmt->while_cond);
  } else {
    ERROR(stmt->token, "control expression should have scalar type.");
  }

  sema_stmt(stmt->while_body);

  loop_end();
}

static void sema_do(Stmt *stmt) {
  loop_begin(stmt);

  stmt->do_cond = sema_expr(stmt->do_cond);
  if (check_scalar(stmt->do_cond->type)) {
    promote_integer(&stmt->do_cond);
  } else {
    ERROR(stmt->token, "control expression should have scalar type.");
  }

  sema_stmt(stmt->do_body);

  loop_end();
}

static void sema_for(Stmt *stmt) {
  loop_begin(stmt);

  if (stmt->for_init) {
    if (stmt->for_init->nd_type == ND_DECL) {
      // TODO: check storage-class-specifier
      sema_decl((Decl *) stmt->for_init, false);
    } else {
      stmt->for_init = (Node *) sema_expr((Expr *) stmt->for_init);
    }
  }

  if (stmt->for_cond) {
    stmt->for_cond = sema_expr(stmt->for_cond);
    if (check_scalar(stmt->for_cond->type)) {
      promote_integer(&stmt->for_cond);
    } else {
      ERROR(stmt->token, "control expression should have scalar type.");
    }
  }

  if (stmt->for_after) {
    stmt->for_after = sema_expr(stmt->for_after);
  }

  sema_stmt(stmt->for_body);

  loop_end();
}

static void sema_goto(Stmt *stmt) {
  vector_push(goto_stmts, stmt);
}

static void sema_continue(Stmt *stmt) {
  if (continue_targets->length > 0) {
    stmt->continue_target = vector_last(continue_targets);
  } else {
    ERROR(stmt->token, "continue statement should appear in loops.");
  }
}

static void sema_break(Stmt *stmt) {
  if (break_targets->length > 0) {
    stmt->break_target = vector_last(break_targets);
  } else {
    ERROR(stmt->token, "break statement should appear in loops.");
  }
}

static void sema_return(Stmt *stmt) {
  Type *type = ret_func->symbol->type->returning;
  if (type->ty_type != TY_VOID) {
    if (stmt->ret_expr) {
      stmt->ret_expr = insert_cast(type, sema_expr(stmt->ret_expr), stmt->token);
    } else {
      ERROR(stmt->token, "'return' without expression in function returning non-void.");
    }
  } else {
    if (stmt->ret_expr) {
      ERROR(stmt->token, "'return' with an expression in function returning void.");
    }
  }

  stmt->ret_func = ret_func;
}

static void sema_stmt(Stmt *stmt) {
  switch (stmt->nd_type) {
    case ND_LABEL:
      sema_label(stmt);
      break;
    case ND_CASE:
      sema_case(stmt);
      break;
    case ND_DEFAULT:
      sema_default(stmt);
      break;

    case ND_COMP:
      vector_push(tag_scopes, map_new());
      for (int i = 0; i < stmt->block_items->length; i++) {
        Node *item = stmt->block_items->buffer[i];
        if (item->nd_type == ND_DECL) {
          sema_decl((Decl *) item, false);
        } else {
          sema_stmt((Stmt *) item);
        }
      }
      vector_pop(tag_scopes);
      break;

    case ND_EXPR:
      if (stmt->expr) {
        stmt->expr = sema_expr(stmt->expr);
      }
      break;

    case ND_IF:
      sema_if(stmt);
      break;
    case ND_SWITCH:
      sema_switch(stmt);
      break;

    case ND_WHILE:
      sema_while(stmt);
      break;
    case ND_DO:
      sema_do(stmt);
      break;
    case ND_FOR:
      sema_for(stmt);
      break;

    case ND_GOTO:
      sema_goto(stmt);
      break;
    case ND_CONTINUE:
      sema_continue(stmt);
      break;
    case ND_BREAK:
      sema_break(stmt);
      break;
    case ND_RETURN:
      sema_return(stmt);
      break;

    // unreachable
    default:
      internal_error("unknown statement type.");
  }
}

static void sema_func(Func *func) {
  DeclAttribution *attr = sema_specs(func->specs, func->token);
  func->symbol->type = sema_declarator(func->symbol->decl, attr->type);
  put_variable(attr, func->symbol, true);
  func->symbol->definition = true;

  if (func->symbol->type->returning->ty_type == TY_ARRAY) {
    ERROR(func->symbol->token, "definition of function returning array.");
  }

  func->symbol->definition = true;
  if (func->symbol->prev && func->symbol->prev->definition) {
    ERROR(func->symbol->token, "duplicated function definition: %s.", func->symbol->identifier);
  }

  stack_size = func->symbol->type->ellipsis ? 176 : 0;

  // initialize statements
  label_stmts = vector_new();
  goto_stmts = vector_new();
  switch_stmts = vector_new();
  continue_targets = vector_new();
  break_targets = vector_new();
  ret_func = func;

  // begin function scope
  vector_push(tag_scopes, map_new());

  for (int i = 0; i < func->symbol->type->params->length; i++) {
    Symbol *param = func->symbol->type->params->buffer[i];
    put_variable(NULL, param, false);
  }

  sema_stmt(func->body);

  // end function scope
  vector_pop(tag_scopes);

  // check goto statements
  for (int i = 0; i < goto_stmts->length; i++) {
    Stmt *stmt = goto_stmts->buffer[i];
    for (int j = 0; j < label_stmts->length; j++) {
      Stmt *label_stmt = label_stmts->buffer[j];
      char *label_ident = label_stmt->label_ident;
      if (strcmp(stmt->goto_ident, label_ident) == 0) {
        stmt->goto_target = label_stmt;
      }
    }
    if (!stmt->goto_target) {
      ERROR(stmt->token, "label: %s is not found.", stmt->goto_ident);
    }
  }

  func->stack_size = stack_size;
  func->label_stmts = label_stmts;
}

static void sema_trans_unit(TransUnit *trans_unit) {
  tag_scopes = vector_new();
  vector_push(tag_scopes, map_new());

  for (int i = 0; i < trans_unit->decls->length; i++) {
    Node *decl = trans_unit->decls->buffer[i];
    if (decl->nd_type == ND_DECL) {
      sema_decl((Decl *) decl, true);
    } else if (decl->nd_type == ND_FUNC) {
      sema_func((Func *) decl);
    }
  }

  vector_pop(tag_scopes);
}

void sema(TransUnit *trans_unit) {
  sema_trans_unit(trans_unit);
}
