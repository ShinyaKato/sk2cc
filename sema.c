#include "sk2cc.h"

typedef struct decl_attribution {
  Type *type;
  bool sp_extern;
  bool sp_noreturn;
} DeclAttribution;

Vector *tag_scopes; // Vector<Map<Type*>*>

Symbol *func_symbol;
int stack_size;

Vector *label_names;
Vector *goto_stmts;

int continue_level;
int break_level;

void put_variable(DeclAttribution *attr, Symbol *symbol, bool global) {
  if (symbol->prev && symbol->prev->definition) {
    error(symbol->token, "duplicated declaration: %s.", symbol->identifier);
  }

  symbol->link = global ? LN_EXTERNAL : LN_NONE;
  symbol->definition = symbol->sy_type == SY_VARIABLE && symbol->type->ty_type != TY_FUNCTION;
  if (attr && attr->sp_extern) {
    symbol->definition = false;
  }

  if (!global) {
    stack_size += symbol->type->size;
    if (stack_size % 8 != 0) {
      stack_size += 8 - stack_size % 8;
    }

    symbol->offset = stack_size;
  }
}

void put_tag(char *tag, Type *type, Token *token) {
  Map *map = tag_scopes->buffer[tag_scopes->length - 1];

  map_put(map, tag, type);
}

Type *lookup_tag(char *tag) {
  for (int i = tag_scopes->length - 1; i >= 0; i--) {
    Map *map = tag_scopes->buffer[i];
    Type *type = map_lookup(map, tag);
    if (type) return type;
  }

  return NULL;
}

// semantics of expression

Expr *sema_expr(Expr *expr);
Type *sema_type_name(TypeName *type_name);

bool check_lvalue(Expr *expr) {
  if (expr->nd_type == ND_IDENTIFIER) return true;
  if (expr->nd_type == ND_DOT) return true;
  if (expr->nd_type == ND_ARROW) return true;
  if (expr->nd_type == ND_INDIRECT) return true;

  return false;
}

bool check_cast(Type *type, Expr *expr) {
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

  return false;
}

Expr *insert_cast(Type *type, Expr *expr, Token *token) {
  Expr *cast = expr_cast(NULL, expr, token);
  cast->type = type;

  if (!check_cast(cast->type, expr)) {
    error(token, "invalid casting.");
  }

  return cast;
}

// integer promotion
Type *promote_integer(Expr **expr) {
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
Type *convert_arithmetic(Expr **lhs, Expr **rhs) {
  promote_integer(lhs);
  promote_integer(rhs);

  Type *ltype = (*lhs)->type;
  Type *rtype = (*rhs)->type;

  Type *type = ltype->ty_type > rtype->ty_type ? ltype : rtype;
  (*lhs) = insert_cast(type, *lhs, (*lhs)->token);
  (*rhs) = insert_cast(type, *rhs, (*rhs)->token);

  return type;
}

Expr *comp_assign_post(NodeType nd_type, Expr *lhs, Expr *rhs, Token *token) {
  lhs = sema_expr(lhs);

  Symbol *sym_addr = symbol_variable(NULL, token);
  sym_addr->type = type_pointer(lhs->type);
  sym_addr->link = LN_NONE;
  put_variable(NULL, sym_addr, false);

  Symbol *sym_val = symbol_variable(NULL, token);
  sym_val->type = lhs->type;
  sym_val->link = LN_NONE;
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

Expr *comp_assign_pre(NodeType nd_type, Expr *lhs, Expr *rhs, Token *token) {
  lhs = sema_expr(lhs);

  Symbol *sym_addr = symbol_variable(NULL, token);
  sym_addr->type = type_pointer(lhs->type);
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

Expr *sema_identifier(Expr *expr) {
  if (expr->symbol) {
    expr->type = expr->symbol->type;
  } else {
    error(expr->token, "undefined variable: %s.", expr->identifier);
  }

  return expr;
}

Expr *sema_integer(Expr *expr) {
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
          error(expr->token, "can not represents integer-constant.");
        }
      } else {
        if (expr->int_value <= 0x7fffffffffffffff) {
          expr->type = type_long();
        } else {
          error(expr->token, "can not represents integer-constant.");
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

Expr *sema_enum_const(Expr *expr) {
  return sema_expr(expr_integer(expr->symbol->const_value, false, false, false, false, expr->token));
}

Expr *sema_string(Expr *expr) {
  int length = expr->string_literal->length;
  expr->type = type_array(type_array_incomplete(type_char()), length);

  return expr;
}

// convert expr[index] to *(expr + index)
Expr *sema_subscription(Expr *expr) {
  expr->expr = sema_expr(expr->expr);
  expr->index = sema_expr(expr->index);

  Expr *add = expr_binary(ND_ADD, expr->expr, expr->index, expr->token);
  Expr *ref = expr_unary(ND_INDIRECT, add, expr->token);

  return sema_expr(ref);
}

Expr *sema_call(Expr *expr) {
  if (expr->expr->nd_type == ND_IDENTIFIER) {
    if (expr->expr->symbol) {
      expr->expr = sema_expr(expr->expr);
    }
  } else {
    error(expr->token, "invalid function call.");
  }

  if (expr->args->length <= 6) {
    for (int i = 0; i < expr->args->length; i++) {
      expr->args->buffer[i] = sema_expr(expr->args->buffer[i]);
    }
  } else {
    error(expr->token, "too many arguments.");
  }

  if (expr->expr->symbol) {
    Vector *params = expr->expr->type->params;
    Vector *args = expr->args;

    if (!expr->expr->symbol->type->ellipsis) {
      if (args->length != params->length) {
        error(expr->token, "number of parameters should be %d, but got %d.", params->length, args->length);
      }
    } else {
      if (args->length < params->length) {
        error(expr->token, "number of parameters should be %d or more, bug got %d.", params->length, args->length);
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
      error(expr->token, "operand should have function type.");
    }
    expr->type = expr->expr->type->returning;
  } else {
    expr->type = type_int();
  }

  return expr;
}

Expr *sema_dot(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (expr->expr->type->ty_type != TY_STRUCT) {
    error(expr->token, "operand should have struct type.");
  }

  Member *member = map_lookup(expr->expr->type->members, expr->member);
  if (!member) {
    error(expr->token, "undefined struct member: %s.", expr->member);
  }

  expr->type = member->type;
  expr->offset = member->offset;

  return expr;
}

// convert expr->member to (*expr).member
Expr *sema_arrow(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  Expr *ref = expr_unary(ND_INDIRECT, expr->expr, expr->token);
  Expr *dot = expr_dot(ref, expr->member, expr->token);

  return sema_expr(dot);
}

Expr *sema_post_inc(Expr *expr) {
  Expr *int_const = expr_integer(1, false, false, false, false, expr->token);
  return comp_assign_post(ND_ADD, expr->expr, int_const, expr->token);
}

Expr *sema_post_dec(Expr *expr) {
  Expr *int_const = expr_integer(1, false, false, false, false, expr->token);
  return comp_assign_post(ND_SUB, expr->expr, int_const, expr->token);
}

Expr *sema_pre_inc(Expr *expr) {
  Expr *int_const = expr_integer(1, false, false, false, false, expr->token);
  return comp_assign_pre(ND_ADD, expr->expr, int_const, expr->token);
}

Expr *sema_pre_dec(Expr *expr) {
  Expr *int_const = expr_integer(1, false, false, false, false, expr->token);
  return comp_assign_pre(ND_SUB, expr->expr, int_const, expr->token);
}

Expr *sema_address(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_lvalue(expr->expr)) {
    expr->type = type_pointer(expr->expr->type);
  } else {
    error(expr->token, "operand should be lvalue.");
  }

  return expr;
}

Expr *sema_indirect(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_pointer(expr->expr->type)) {
    expr->type = expr->expr->type->pointer_to;
  } else {
    error(expr->token, "operand should have pointer type.");
  }

  return expr;
}

Expr *sema_uplus(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_arithmetic(expr->expr->type)) {
    promote_integer(&expr->expr);
  } else {
    error(expr->token, "operand should have arithmetic type.");
  }

  // we can remove node of unary + before code generation.
  return expr->expr;
}

Expr *sema_uminus(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_arithmetic(expr->expr->type)) {
    expr->type = promote_integer(&expr->expr);
  } else {
    error(expr->token, "operand should have arithmetic type.");
  }

  return expr;
}

Expr *sema_not(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_integer(expr->expr->type)) {
    expr->type = promote_integer(&expr->expr);
  } else {
    error(expr->token, "operand should have integer type.");
  }

  return expr;
}

Expr *sema_lnot(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_scalar(expr->expr->type)) {
    promote_integer(&expr->expr);
    expr->type = type_int();
  } else {
    error(expr->token, "operand should have scalar type.");
  }

  return expr;
}

// convert sizeof to integer-constant
Expr *sema_sizeof(Expr *expr) {
  if (expr->expr) {
    Type *type = sema_expr(expr->expr)->type;
    return sema_expr(expr_integer(type->original->size, false, false, false, false, expr->token));
  }

  Type *type = sema_type_name(expr->type_name);
  return sema_expr(expr_integer(type->original->size, false, false, false, false, expr->token));
}

// convert alignof to integer-constant
Expr *sema_alignof(Expr *expr) {
  Type *type = sema_type_name(expr->type_name);
  return sema_expr(expr_integer(type->align, false, false, false, false, expr->token));
}

Expr *sema_cast(Expr *expr) {
  expr->expr = sema_expr(expr->expr);
  expr->type = sema_type_name(expr->type_name);

  if (!check_cast(expr->type, expr->expr)) {
    error(expr->token, "invalid casting.");
  }

  return expr;
}

Expr *sema_mul(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else {
    error(expr->token, "operands should have intger type.");
  }

  return expr;
}

Expr *sema_mod(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else {
    error(expr->token, "operands should have intger type.");
  }

  return expr;
}

Expr *sema_add(Expr *expr) {
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
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_sub(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else if (check_pointer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    promote_integer(&expr->rhs);
    expr->type = expr->lhs->type;
  } else {
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_shift(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else {
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_relational(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    convert_arithmetic(&expr->lhs, &expr->rhs);
    expr->type = type_int();
  } else if (check_pointer(expr->lhs->type) && check_pointer(expr->rhs->type)) {
    expr->type = type_int();
  } else {
    error(expr->token, "invalid operand types.");
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

Expr *sema_equality(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    convert_arithmetic(&expr->lhs, &expr->rhs);
    expr->type = type_int();
  } else if (check_pointer(expr->lhs->type) && check_pointer(expr->rhs->type)) {
    expr->type = type_int();
  } else {
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_bitwise(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else {
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_logical(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_scalar(expr->lhs->type) && check_scalar(expr->rhs->type)) {
    promote_integer(&expr->lhs);
    promote_integer(&expr->rhs);
    expr->type = type_int();
  } else {
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_condition(Expr *expr) {
  expr->cond = sema_expr(expr->cond);
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_scalar(expr->cond->type)) {
    promote_integer(&expr->cond);
  } else {
    error(expr->token, "control expression should have scalar type.");
  }

  if (check_arithmetic(expr->lhs->type) && check_arithmetic(expr->rhs->type)) {
    expr->type = convert_arithmetic(&expr->lhs, &expr->rhs);
  } else if (check_pointer(expr->lhs->type) && check_pointer(expr->rhs->type)) {
    expr->type = expr->lhs->type;
  } else {
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_assign(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_lvalue(expr->lhs)) {
    expr->rhs = insert_cast(expr->lhs->type, expr->rhs, expr->token);
    expr->type = expr->lhs->type;
  } else {
    error(expr->token, "invalid operands.");
  }

  return expr;
}

Expr *sema_mul_assign(Expr *expr) {
  return comp_assign_pre(ND_MUL, expr->lhs, expr->rhs, expr->token);
}

Expr *sema_div_assign(Expr *expr) {
  return comp_assign_pre(ND_DIV, expr->lhs, expr->rhs, expr->token);
}

Expr *sema_mod_assign(Expr *expr) {
  return comp_assign_pre(ND_MOD, expr->lhs, expr->rhs, expr->token);
}

Expr *sema_add_assign(Expr *expr) {
  return comp_assign_pre(ND_ADD, expr->lhs, expr->rhs, expr->token);
}

Expr *sema_sub_assign(Expr *expr) {
  return comp_assign_pre(ND_SUB, expr->lhs, expr->rhs, expr->token);
}

Expr *sema_comma(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  expr->type = expr->rhs->type;

  return expr;
}

Expr *sema_expr(Expr *expr) {
  if (expr->type) {
    return expr;
  }

  if (expr->nd_type == ND_IDENTIFIER) {
    expr = sema_identifier(expr);
  } else if (expr->nd_type == ND_INTEGER) {
    expr = sema_integer(expr);
  } else if (expr->nd_type == ND_ENUM_CONST) {
    expr = sema_enum_const(expr);
  } else if (expr->nd_type == ND_STRING) {
    expr = sema_string(expr);
  } else if (expr->nd_type == ND_SUBSCRIPTION) {
    expr = sema_subscription(expr);
  } else if (expr->nd_type == ND_CALL) {
    expr = sema_call(expr);
  } else if (expr->nd_type == ND_DOT) {
    expr = sema_dot(expr);
  } else if (expr->nd_type == ND_ARROW) {
    expr = sema_arrow(expr);
  } else if (expr->nd_type == ND_POST_INC) {
    expr = sema_post_inc(expr);
  } else if (expr->nd_type == ND_POST_DEC) {
    expr = sema_post_dec(expr);
  } else if (expr->nd_type == ND_PRE_INC) {
    expr = sema_pre_inc(expr);
  } else if (expr->nd_type == ND_PRE_DEC) {
    expr = sema_pre_dec(expr);
  } else if (expr->nd_type == ND_ADDRESS) {
    expr = sema_address(expr);
  } else if (expr->nd_type == ND_INDIRECT) {
    expr = sema_indirect(expr);
  } else if (expr->nd_type == ND_UPLUS) {
    expr = sema_uplus(expr);
  } else if (expr->nd_type == ND_UMINUS) {
    expr = sema_uminus(expr);
  } else if (expr->nd_type == ND_NOT) {
    expr = sema_not(expr);
  } else if (expr->nd_type == ND_LNOT) {
    expr = sema_lnot(expr);
  } else if (expr->nd_type == ND_SIZEOF) {
    expr = sema_sizeof(expr);
  } else if (expr->nd_type == ND_ALIGNOF) {
    expr = sema_alignof(expr);
  } else if (expr->nd_type == ND_CAST) {
    expr = sema_cast(expr);
  } else if (expr->nd_type == ND_MUL || expr->nd_type == ND_DIV) {
    expr = sema_mul(expr);
  } else if (expr->nd_type == ND_MOD) {
    expr = sema_mod(expr);
  } else if (expr->nd_type == ND_ADD) {
    expr = sema_add(expr);
  } else if (expr->nd_type == ND_SUB) {
    expr = sema_sub(expr);
  } else if (expr->nd_type == ND_LSHIFT || expr->nd_type == ND_RSHIFT) {
    expr = sema_shift(expr);
  } else if (expr->nd_type == ND_LT || expr->nd_type == ND_GT || expr->nd_type == ND_LTE || expr->nd_type == ND_GTE) {
    expr = sema_relational(expr);
  } else if (expr->nd_type == ND_EQ || expr->nd_type == ND_NEQ) {
    expr = sema_equality(expr);
  } else if (expr->nd_type == ND_AND || expr->nd_type == ND_XOR || expr->nd_type == ND_OR) {
    expr = sema_bitwise(expr);
  } else if (expr->nd_type == ND_LAND || expr->nd_type == ND_LOR) {
    expr = sema_logical(expr);
  } else if (expr->nd_type == ND_CONDITION) {
    expr = sema_condition(expr);
  } else if (expr->nd_type == ND_ASSIGN) {
    expr = sema_assign(expr);
  } else if (expr->nd_type == ND_MUL_ASSIGN) {
    expr = sema_mul_assign(expr);
  } else if (expr->nd_type == ND_DIV_ASSIGN) {
    expr = sema_div_assign(expr);
  } else if (expr->nd_type == ND_MOD_ASSIGN) {
    expr = sema_mod_assign(expr);
  } else if (expr->nd_type == ND_ADD_ASSIGN) {
    expr = sema_add_assign(expr);
  } else if (expr->nd_type == ND_SUB_ASSIGN) {
    expr = sema_sub_assign(expr);
  } else if (expr->nd_type == ND_COMMA) {
    expr = sema_comma(expr);
  } else {
    // unreachable
    internal_error("unknown expression type.");
  }

  // convert array to pointer
  if (expr->type->ty_type == TY_ARRAY) {
    Type *pointer = type_pointer(expr->type->array_of);
    pointer->original = expr->type;
    expr->type = pointer;
  }

  return expr;
}

Expr *sema_const_expr(Expr *expr) {
  expr = sema_expr(expr);

  if (expr->nd_type != ND_INTEGER) {
    error(expr->token, "invalid integer constant expression.");
  }

  return expr;
}

// semantics of declaration

void sema_decl(Decl *decl, bool global);
DeclAttribution *sema_specs(Vector *specs, Token *token);
Type *sema_struct(Specifier *spec);
Type *sema_enum(Specifier *spec);
Type *sema_typedef_name(Specifier *spec);
Type *sema_declarator(Declarator *decl, Type *type);
Type *sema_type_name(TypeName *type_name);
void sema_initializer(Initializer *init, Type *type, bool global);

void sema_decl(Decl *decl, bool global) {
  DeclAttribution *attr = sema_specs(decl->specs, decl->token);

  for (int i = 0; i < decl->symbols->length; i++) {
    Symbol *symbol = decl->symbols->buffer[i];
    symbol->type = sema_declarator(symbol->decl, attr->type);

    if (symbol->init) {
      sema_initializer(symbol->init, symbol->type, global);

      if (symbol->type->ty_type == TY_ARRAY) {
        if (symbol->type->complete) {
          if (symbol->type->length < symbol->init->list->length) {
            error(symbol->token, "too many initializer items.");
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

DeclAttribution *sema_specs(Vector *specs, Token *token) {
  int sp_storage = 0;
  bool sp_extern = false;

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
    error(token, "too many storage-class-specifiers.");
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
    error(token, "invalid type-specifiers.");
  }

  DeclAttribution *attr = calloc(1, sizeof(DeclAttribution));
  attr->type = type;
  attr->sp_extern = sp_extern;
  attr->sp_noreturn = sp_noreturn;
  return attr;
}

Type *sema_struct(Specifier *spec) {
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
          error(symbol->token, "declaration of struct member with incomplete type.");
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

Type *sema_enum(Specifier *spec) {
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
        error(spec->token, "redeclaration of enum tag.");
      }

      Type *type = type_int();
      put_tag(spec->enum_tag, type, spec->token);
      return type;
    }

    return type_int();
  }

  Type *type = lookup_tag(spec->enum_tag);
  if (!type) {
    error(spec->token, "undefined enum tag.");
  }
  return type;
}

Type *sema_typedef_name(Specifier *spec) {
  Symbol *symbol = spec->typedef_symbol;

  if (strcmp(symbol->identifier, "__builtin_va_list") == 0) {
    return type_va_list();
  }

  return symbol->type;
}

Type *sema_declarator(Declarator *decl, Type *type) {
  if (decl && decl->decl_type == DECL_POINTER) {
    Type *pointer_to = sema_declarator(decl->decl, type);
    return type_pointer(pointer_to);
  }

  if (decl && decl->decl_type == DECL_ARRAY) {
    Type *array_of = sema_declarator(decl->decl, type);
    Type *type = type_array_incomplete(array_of);
    if (decl->size) {
      if (decl->size->nd_type != ND_INTEGER) {
        error(decl->size->token, "only integer constant is supported for array size.");
      }
      type = type_array(type, decl->size->int_value);
    }
    return type;
  }

  if (decl && decl->decl_type == DECL_FUNCTION) {
    Type *returning = sema_declarator(decl->decl, type);
    Vector *params = vector_new();
    for (int i = 0; i < decl->params->length; i++) {
      Decl *param = decl->params->buffer[i];
      DeclAttribution *attr = sema_specs(param->specs, param->token);
      param->symbol->type = sema_declarator(param->symbol->decl, attr->type);
      if (param->symbol->type->ty_type == TY_ARRAY) {
        param->symbol->type = type_pointer(param->symbol->type->array_of);
      }
      vector_push(params, param->symbol);
    }
    return type_function(returning, params, decl->ellipsis);
  }

  return type;
}

Type *sema_type_name(TypeName *type_name) {
  DeclAttribution *attr = sema_specs(type_name->specs, type_name->token);

  return sema_declarator(type_name->decl, attr->type);
}

void sema_initializer(Initializer *init, Type *type, bool global) {
  init->type = type;

  if (init->list) {
    for (int i = 0; i < init->list->length; i++) {
      sema_initializer(init->list->buffer[i], type->array_of, global);
    }
  } else {
    if (global) {
      if (init->expr->nd_type != ND_INTEGER && init->expr->nd_type != ND_STRING) {
        error(init->expr->token, "initializer expression should be integer constant or string literal.");
      }
    }
    init->expr = sema_expr(init->expr);
    init->expr = insert_cast(type, init->expr, init->token);
  }
}

// semantics of statement

void begin_loop() {
  continue_level++;
  break_level++;
}

void end_loop() {
  continue_level--;
  break_level--;
}

void sema_stmt(Stmt *stmt);

void sema_label(Stmt *stmt) {
  for (int i = 0; i < label_names->length; i++) {
    char *label_name = label_names->buffer[i];
    if (strcmp(stmt->label_name, label_name) == 0) {
      error(stmt->token, "duplicated label declaration: %s.", stmt->label_name);
    }
  }
  vector_push(label_names, stmt->label_name);

  sema_stmt(stmt->label_stmt);
}

void sema_if(Stmt *stmt) {
  stmt->if_cond = sema_expr(stmt->if_cond);
  if (check_scalar(stmt->if_cond->type)) {
    promote_integer(&stmt->if_cond);
  } else {
    error(stmt->token, "control expression should have scalar type.");
  }

  sema_stmt(stmt->then_body);

  if (stmt->else_body) {
    sema_stmt(stmt->else_body);
  }
}

void sema_while(Stmt *stmt) {
  begin_loop();

  stmt->while_cond = sema_expr(stmt->while_cond);
  if (check_scalar(stmt->while_cond->type)) {
    promote_integer(&stmt->while_cond);
  } else {
    error(stmt->token, "control expression should have scalar type.");
  }

  sema_stmt(stmt->while_body);

  end_loop();
}

void sema_do(Stmt *stmt) {
  begin_loop();

  stmt->do_cond = sema_expr(stmt->do_cond);
  if (check_scalar(stmt->do_cond->type)) {
    promote_integer(&stmt->do_cond);
  } else {
    error(stmt->token, "control expression should have scalar type.");
  }

  sema_stmt(stmt->do_body);

  end_loop();
}

void sema_for(Stmt *stmt) {
  vector_push(tag_scopes, map_new());
  begin_loop();

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
      error(stmt->token, "control expression should have scalar type.");
    }
  }

  if (stmt->for_after) {
    stmt->for_after = sema_expr(stmt->for_after);
  }

  sema_stmt(stmt->for_body);

  vector_pop(tag_scopes);
  end_loop();
}

void sema_goto(Stmt *stmt) {
  vector_push(goto_stmts, stmt);
}

void sema_continue(Stmt *stmt) {
  if (continue_level == 0) {
    error(stmt->token, "continue statement should appear in loops.");
  }
}

void sema_break(Stmt *stmt) {
  if (break_level == 0) {
    error(stmt->token, "break statement should appear in loops.");
  }
}

void sema_return(Stmt *stmt) {
  Type *type = func_symbol->type->returning;
  if (type->ty_type != TY_VOID) {
    if (stmt->ret) {
      stmt->ret = insert_cast(type, sema_expr(stmt->ret), stmt->token);
    } else {
      error(stmt->token, "'return' without expression in function returning non-void.");
    }
  } else {
    if (stmt->ret) {
      error(stmt->token, "'return' with an expression in function returning void.");
    }
  }
}

void sema_stmt(Stmt *stmt) {
  if (stmt->nd_type == ND_LABEL) {
    sema_label(stmt);
  } else if (stmt->nd_type == ND_COMP) {
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
  } else if (stmt->nd_type == ND_EXPR) {
    if (stmt->expr) {
      stmt->expr = sema_expr(stmt->expr);
    }
  } else if (stmt->nd_type == ND_IF) {
    sema_if(stmt);
  } else if (stmt->nd_type == ND_WHILE) {
    sema_while(stmt);
  } else if (stmt->nd_type == ND_DO) {
    sema_do(stmt);
  } else if (stmt->nd_type == ND_FOR) {
    sema_for(stmt);
  } else if (stmt->nd_type == ND_GOTO) {
    sema_goto(stmt);
  } else if (stmt->nd_type == ND_CONTINUE) {
    sema_continue(stmt);
  } else if (stmt->nd_type == ND_BREAK) {
    sema_break(stmt);
  } else if (stmt->nd_type == ND_RETURN) {
    sema_return(stmt);
  } else {
    internal_error("unknown statement type.");
  }
}

void sema_func(Func *func) {
  DeclAttribution *attr = sema_specs(func->specs, func->token);

  func->symbol->type = sema_declarator(func->symbol->decl, attr->type);

  if (func->symbol->type->params->length > 6) {
    error(func->symbol->token, "too many parameters.");
  }
  if (func->symbol->type->returning->ty_type == TY_ARRAY) {
    error(func->symbol->token, "definition of function returning array.");
  }

  func->symbol->definition = true;
  if (func->symbol->prev && func->symbol->prev->definition) {
    error(func->symbol->token, "duplicated function definition: %s.", func->symbol->identifier);
  }

  func_symbol = func->symbol;
  stack_size = func->symbol->type->ellipsis ? 176 : 0;
  label_names = vector_new();
  goto_stmts = vector_new();

  for (int i = 0; i < func->symbol->type->params->length; i++) {
    Symbol *param = func->symbol->type->params->buffer[i];
    put_variable(attr, param, false);
  }

  vector_push(tag_scopes, map_new());
  sema_stmt(func->body);
  vector_pop(tag_scopes);

  // check goto statements
  for (int i = 0; i < goto_stmts->length; i++) {
    Stmt *stmt = goto_stmts->buffer[i];
    bool found = false;
    for (int j = 0; j < label_names->length; j++) {
      char *label_name = label_names->buffer[j];
      if (strcmp(stmt->goto_label, label_name) == 0) {
        found = true;
      }
    }
    if (!found) {
      error(stmt->token, "label: %s is not found.", stmt->goto_label);
    }
  }

  func->stack_size = stack_size;
  func->label_names = label_names;
}

void sema_trans_unit(TransUnit *trans_unit) {
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
  tag_scopes = vector_new();

  continue_level = 0;
  break_level = 0;

  sema_trans_unit(trans_unit);
}
