#include "sk2cc.h"

typedef struct decl_attribution {
  Type *type;
  bool sp_extern;
  bool sp_noreturn;
} DeclAttribution;

Vector *tag_scopes; // Vector<Map<Type*>*>

int stack_size;

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
    expr->type = type_convert(expr->symbol->type);
  } else {
    error(expr->token, "undefined variable: %s.", expr->identifier);
  }

  return expr;
}

Expr *sema_integer(Expr *expr) {
  expr->type = type_int();

  return expr;
}

Expr *sema_enum_const(Expr *expr) {
  return sema_expr(expr_integer(expr->symbol->const_value, expr->token));
}

Expr *sema_string(Expr *expr) {
  int length = expr->string_literal->length;
  Type *type = type_array(type_array_incomplete(type_char()), length);

  expr->type = type_convert(type);

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

    for (int i = 0; i < params->length; i++) {
      Expr *arg = args->buffer[i];
      Symbol *param = params->buffer[i];
      if (!check_same(arg->type, param->type)) {
        error(expr->token, "parameter types and argument types should be the same.");
      }
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

  expr->type = type_convert(member->type);
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
  Expr *int_const = expr_integer(1, expr->token);
  return comp_assign_post(ND_ADD, expr->expr, int_const, expr->token);
}

Expr *sema_post_dec(Expr *expr) {
  Expr *int_const = expr_integer(1, expr->token);
  return comp_assign_post(ND_SUB, expr->expr, int_const, expr->token);
}

Expr *sema_pre_inc(Expr *expr) {
  Expr *int_const = expr_integer(1, expr->token);
  return comp_assign_pre(ND_ADD, expr->expr, int_const, expr->token);
}

Expr *sema_pre_dec(Expr *expr) {
  Expr *int_const = expr_integer(1, expr->token);
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
    expr->type = type_convert(expr->expr->type->pointer_to);
  } else {
    error(expr->token, "operand should have pointer type.");
  }

  return expr;
}

// convert +expr to expr
Expr *sema_uplus(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (!check_integer(expr->expr->type)) {
    error(expr->token, "operand should have arithmetic type.");
  }

  return expr->expr;
}

Expr *sema_uminus(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_integer(expr->expr->type)) {
    expr->type = expr->expr->type;
  } else {
    error(expr->token, "operand should have arithmetic type.");
  }

  return expr;
}

Expr *sema_not(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_integer(expr->expr->type)) {
    expr->type = expr->expr->type;
  } else {
    error(expr->token, "operand should have integer type.");
  }

  return expr;
}

Expr *sema_lnot(Expr *expr) {
  expr->expr = sema_expr(expr->expr);

  if (check_scalar(expr->expr->type)) {
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
    return sema_integer(expr_integer(type->original->size, expr->token));
  }

  Type *type = sema_type_name(expr->type_name);
  return sema_integer(expr_integer(type->original->size, expr->token));
}

// convert alignof to integer-constant
Expr *sema_alignof(Expr *expr) {
  Type *type = sema_type_name(expr->type_name);
  return sema_integer(expr_integer(type->align, expr->token));
}

Expr *sema_cast(Expr *expr) {
  expr->expr = sema_expr(expr->expr);
  expr->type = sema_type_name(expr->type_name);

  // TODO: check casting

  return expr;
}

Expr *sema_multiplicative(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = type_int();
  } else {
    error(expr->token, "operands should have intger type.");
  }

  return expr;
}

Expr *sema_add(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = type_int();
  } else if (check_pointer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = expr->lhs->type;
  } else if (check_integer(expr->lhs->type) && check_pointer(expr->rhs->type)) {
    Expr *tmp = expr->lhs;
    expr->lhs = expr->rhs;
    expr->rhs = tmp;
    expr->type = expr->lhs->type;
  } else {
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_sub(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = type_int();
  } else if (check_pointer(expr->lhs->type) && check_integer(expr->rhs->type)) {
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
    expr->type = type_int();
  } else {
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_relational(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
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

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
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
    expr->type = type_int();
  } else {
    error(expr->token, "invalid operand types.");
  }

  return expr;
}

Expr *sema_logical(Expr *expr) {
  expr->lhs = sema_expr(expr->lhs);
  expr->rhs = sema_expr(expr->rhs);

  if (check_scalar(expr->lhs->type) && check_scalar(expr->rhs->type)) {
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

  if (check_integer(expr->lhs->type) && check_integer(expr->rhs->type)) {
    expr->type = type_int();
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
  if (expr->nd_type == ND_IDENTIFIER)
    return sema_identifier(expr);
  if (expr->nd_type == ND_INTEGER)
    return sema_integer(expr);
  if (expr->nd_type == ND_ENUM_CONST)
    return sema_enum_const(expr);
  if (expr->nd_type == ND_STRING)
    return sema_string(expr);
  if (expr->nd_type == ND_SUBSCRIPTION)
    return sema_subscription(expr);
  if (expr->nd_type == ND_CALL)
    return sema_call(expr);
  if (expr->nd_type == ND_DOT)
    return sema_dot(expr);
  if (expr->nd_type == ND_ARROW)
    return sema_arrow(expr);
  if (expr->nd_type == ND_POST_INC)
    return sema_post_inc(expr);
  if (expr->nd_type == ND_POST_DEC)
    return sema_post_dec(expr);
  if (expr->nd_type == ND_PRE_INC)
    return sema_pre_inc(expr);
  if (expr->nd_type == ND_PRE_DEC)
    return sema_pre_dec(expr);
  if (expr->nd_type == ND_ADDRESS)
    return sema_address(expr);
  if (expr->nd_type == ND_INDIRECT)
    return sema_indirect(expr);
  if (expr->nd_type == ND_UPLUS)
    return sema_uplus(expr);
  if (expr->nd_type == ND_UMINUS)
    return sema_uminus(expr);
  if (expr->nd_type == ND_NOT)
    return sema_not(expr);
  if (expr->nd_type == ND_LNOT)
    return sema_lnot(expr);
  if (expr->nd_type == ND_SIZEOF)
    return sema_sizeof(expr);
  if (expr->nd_type == ND_ALIGNOF)
    return sema_alignof(expr);
  if (expr->nd_type == ND_CAST)
    return sema_cast(expr);
  if (expr->nd_type == ND_MUL || expr->nd_type == ND_DIV || expr->nd_type == ND_MOD)
    return sema_multiplicative(expr);
  if (expr->nd_type == ND_ADD)
    return sema_add(expr);
  if (expr->nd_type == ND_SUB)
    return sema_sub(expr);
  if (expr->nd_type == ND_LSHIFT || expr->nd_type == ND_RSHIFT)
    return sema_shift(expr);
  if (expr->nd_type == ND_LT || expr->nd_type == ND_GT || expr->nd_type == ND_LTE || expr->nd_type == ND_GTE)
    return sema_relational(expr);
  if (expr->nd_type == ND_EQ || expr->nd_type == ND_NEQ)
    return sema_equality(expr);
  if (expr->nd_type == ND_AND || expr->nd_type == ND_XOR || expr->nd_type == ND_OR)
    return sema_bitwise(expr);
  if (expr->nd_type == ND_LAND || expr->nd_type == ND_LOR)
    return sema_logical(expr);
  if (expr->nd_type == ND_CONDITION)
    return sema_condition(expr);
  if (expr->nd_type == ND_ASSIGN)
    return sema_assign(expr);
  if (expr->nd_type == ND_MUL_ASSIGN)
    return sema_mul_assign(expr);
  if (expr->nd_type == ND_DIV_ASSIGN)
    return sema_div_assign(expr);
  if (expr->nd_type == ND_MOD_ASSIGN)
    return sema_mod_assign(expr);
  if (expr->nd_type == ND_ADD_ASSIGN)
    return sema_add_assign(expr);
  if (expr->nd_type == ND_SUB_ASSIGN)
    return sema_sub_assign(expr);
  if (expr->nd_type == ND_COMMA)
    return sema_comma(expr);

  error(expr->token, "invalid node type.");
}

// semantics of declaration

void sema_decl(Decl *decl, bool global);
DeclAttribution *sema_specs(Vector *specs, Token *token);
Type *sema_struct(Specifier *spec);
Type *sema_enum(Specifier *spec);
Type *sema_typedef_name(Specifier *spec);
Type *sema_declarator(Declarator *decl, Type *type);
Type *sema_type_name(TypeName *type_name);
void sema_initializer(Initializer *init, Type *type);

void sema_decl(Decl *decl, bool global) {
  DeclAttribution *attr = sema_specs(decl->specs, decl->token);

  for (int i = 0; i < decl->symbols->length; i++) {
    Symbol *symbol = decl->symbols->buffer[i];
    symbol->type = sema_declarator(symbol->decl, attr->type);

    if (symbol->init) {
      sema_initializer(symbol->init, symbol->type);

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
  } else if (sp_type == 2 && sp_unsigned == 1 && sp_char == 1) {
    type = type_uchar();
  } else if (sp_type == 1 && sp_short == 1) {
    type = type_short();
  } else if (sp_type == 2 && sp_unsigned == 1 && sp_short == 1) {
    type = type_ushort();
  } else if (sp_type == 1 && sp_int == 1) {
    type = type_int();
  } else if (sp_type == 2 && sp_unsigned == 1 && sp_int == 1) {
    type = type_uint();
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
        symbol->const_expr = sema_expr(symbol->const_expr);
        const_value = symbol->const_expr->int_value;
      } else {
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
  return spec->typedef_symbol->type;
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

void sema_initializer(Initializer *init, Type *type) {
  init->type = type;

  if (init->list) {
    for (int i = 0; i < init->list->length; i++) {
      sema_initializer(init->list->buffer[i], type->array_of);
    }
  } else {
    init->expr = sema_expr(init->expr);
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

void sema_if(Stmt *stmt) {
  stmt->if_cond = sema_expr(stmt->if_cond);
  check_scalar(stmt->if_cond->type);

  sema_stmt(stmt->then_body);

  if (stmt->else_body) {
    sema_stmt(stmt->else_body);
  }
}

void sema_while(Stmt *stmt) {
  begin_loop();

  stmt->while_cond = sema_expr(stmt->while_cond);
  check_scalar(stmt->while_cond->type);

  sema_stmt(stmt->while_body);

  end_loop();
}

void sema_do(Stmt *stmt) {
  begin_loop();

  stmt->do_cond = sema_expr(stmt->do_cond);
  check_scalar(stmt->do_cond->type);

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
    check_scalar(stmt->for_cond->type);
  }

  if (stmt->for_after) {
    stmt->for_after = sema_expr(stmt->for_after);
  }

  sema_stmt(stmt->for_body);

  vector_pop(tag_scopes);
  end_loop();
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
  if (stmt->ret) {
    stmt->ret = sema_expr(stmt->ret);
  }

  // TODO: return type check and casting
}

void sema_stmt(Stmt *stmt) {
  if (stmt->nd_type == ND_COMP) {
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
  } else if (stmt->nd_type == ND_CONTINUE) {
    sema_continue(stmt);
  } else if (stmt->nd_type == ND_BREAK) {
    sema_break(stmt);
  } else if (stmt->nd_type == ND_RETURN) {
    sema_return(stmt);
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

  stack_size = func->symbol->type->ellipsis ? 176 : 0;

  for (int i = 0; i < func->symbol->type->params->length; i++) {
    Symbol *param = func->symbol->type->params->buffer[i];
    put_variable(attr, param, false);
  }

  vector_push(tag_scopes, map_new());
  sema_stmt(func->body);
  vector_pop(tag_scopes);

  func->stack_size = stack_size;
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
