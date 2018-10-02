#include "sk2cc.h"

bool node_lvalue(Node *node) {
  return node->type == IDENTIFIER || node->type == INDIRECT || node->type == DOT;
}

Node *node_new() {
  Node *node = (Node *) calloc(1, sizeof(Node));
  return node;
}

Node *node_int_const(int int_value, Token *token) {
  Node *node = node_new();
  node->type = INT_CONST;
  node->value_type = type_int();
  node->int_value = int_value;
  node->token = token;
  return node;
}

Node *node_float_const(double double_value, Token *token) {
  Node *node = node_new();
  node->type = FLOAT_CONST;
  node->value_type = type_double();
  node->double_value = double_value;
  node->token = token;
  return node;
}

Node *node_string_literal(String *string_value, int string_label, Token *token) {
  Node *node = node_new();
  node->type = STRING_LITERAL;
  node->value_type = type_convert(type_array_of(type_char(), string_value->length));
  node->string_value = string_value;
  node->string_label = string_label;
  node->token = token;
  return node;
}

Node *node_identifier(char *identifier, Symbol *symbol, Token *token) {
  Node *node = node_new();
  node->type = IDENTIFIER;
  node->value_type = symbol ? type_convert(symbol->value_type) : type_int();
  node->identifier = identifier;
  node->symbol = symbol;
  node->token = token;
  return node;
}

Node *node_func_call(Node *expr, Vector *args, Token *token) {
  if (expr->type != IDENTIFIER) {
    error(token, "invalid function call.");
  }

  if (args->length > 6) {
    error(token, "too many arguments.");
  }

  if (expr->symbol) {
    Vector *params = expr->value_type->params;

    if (!expr->value_type->ellipsis) {
      if (args->length != params->length) {
        error(token, "number of parameters should be %d, but got %d.", params->length, args->length);
      }
    } else {
      if (args->length < params->length) {
        error(token, "number of parameters should be %d or more, but got %d.", params->length, args->length);
      }
    }

    for (int i = 0; i < params->length; i++) {
      Node *arg = args->array[i];
      Symbol *param = params->array[i];
      if (!type_same(arg->value_type, param->value_type)) {
        error(token, "parameter types and argument types should be the same.");
      }
    }
  }

  Type *value_type;
  if (expr->symbol) {
    if (expr->value_type->type != FUNCTION) {
      error(token, "operand of function call should have function type.");
    }
    value_type = expr->value_type->function_returning;
  } else {
    value_type = type_int();
  }

  Node *node = node_new();
  node->type = FUNC_CALL;
  node->value_type = value_type;
  node->expr = expr;
  node->args = args;
  node->token = token;
  return node;
}

Node *node_dot(Node *expr, char *member, Token *token) {
  if (expr->value_type->type != STRUCT) {
    error(token, "operand of . operator should have struct type.");
  }

  Type *value_type = map_lookup(expr->value_type->members, member);
  if (!value_type) {
    error(token, "undefined struct member: %s.", member);
  }

  int *offset = map_lookup(expr->value_type->offsets, member);

  Node *node = node_new();
  node->type = DOT;
  node->value_type = type_convert(value_type);
  node->expr = expr;
  node->member = member;
  node->member_offset = *offset;
  node->token = token;
  return node;
}

Node *node_unary_expr(NodeType type, Type *value_type, Node *expr, Token *token) {
  Node *node = node_new();
  node->type = type;
  node->value_type = value_type;
  node->expr = expr;
  node->token = token;
  return node;
}

Node *node_inc(NodeType type, Node *expr, Token *token) {
  if (!node_lvalue(expr)) {
    error(token, "operand of ++ operator should be lvalue.");
  }

  Type *value_type;
  if (type_integer(expr->value_type)) {
    value_type = type_int();
  } else if (type_pointer(expr->value_type)) {
    value_type = expr->value_type;
  } else {
    error(token, "invalid operand type for ++ operator.");
  }

  return node_unary_expr(type, value_type, expr, token);
}

Node *node_dec(NodeType type, Node *expr, Token *token) {
  if (!node_lvalue(expr)) {
    error(token, "operand of -- operator should be lvalue.");
  }

  Type *value_type;
  if (type_integer(expr->value_type)) {
    value_type = type_int();
  } else if (type_pointer(expr->value_type)) {
    value_type = expr->value_type;
  } else {
    error(token, "invalid operand type for -- operator.");
  }

  return node_unary_expr(type, value_type, expr, token);
}

Node *node_address(Node *expr, Token *token) {
  if (!node_lvalue(expr)) {
    error(token, "operand of %s operator should be lvalue.", token->type_name);
  }

  return node_unary_expr(ADDRESS, type_pointer_to(expr->value_type), expr, token);
}

Node *node_indirect(Node *expr, Token *token) {
  if (!type_pointer(expr->value_type)) {
    error(token, "operand of %s operator should have pointer type.", token->type_name);
  }

  return node_unary_expr(INDIRECT, type_convert(expr->value_type->pointer_to), expr, token);
}

Node *node_unary_arithmetic(NodeType type, Node *expr, Token *token) {
  Type *value_type;
  if (type == UPLUS || type == UMINUS) {
    if (!type_integer(expr->value_type)) {
      if (type == UPLUS) {
        error(token, "operand of unary + operator should have integer type.");
      } else if (type == UMINUS) {
        error(token, "operand of unary - operator should have integer type.");
      }
    }
    value_type = expr->value_type;
  } else if (type == NOT) {
    if (!type_integer(expr->value_type)) {
      error(token, "operand of ~ operator should have integer type.");
    }
    value_type = type_int();
  } else if (type == LNOT) {
    if (!type_scalar(expr->value_type)) {
      error(token, "operand of ! operator should have scalar type.");
    }
    value_type = type_int();
  }

  return node_unary_expr(type, value_type, expr, token);
}

Node *node_cast(Type *value_type, Node *expr, Token *token) {
  Node *node = node_new();
  node->type = CAST;
  node->value_type = value_type;
  node->expr = expr;
  node->token = token;
  return node;
}

Node *node_binary_expr(NodeType type, Type *value_type, Node *left, Node *right, Token *token) {
  Node *node = node_new();
  node->type = type;
  node->value_type = value_type;
  node->left = left;
  node->right = right;
  node->token = token;
  return node;
}

Type *semantics_mul(Node *left, Node *right, Token *token) {
  if (type_integer(left->value_type) && type_integer(right->value_type)) {
    return type_int();
  }

  error(token, "operands of %s operator should have integer type.", token->type_name);
}

Node *node_mul(Node *left, Node *right, Token *token) {
  Type *type = semantics_mul(left, right, token);
  return node_binary_expr(MUL, type, left, right, token);
}

Node *node_div(Node *left, Node *right, Token *token) {
  Type *type = semantics_mul(left, right, token);
  return node_binary_expr(DIV, type, left, right, token);
}

Node *node_mod(Node *left, Node *right, Token *token) {
  Type *value_type;
  if (type_integer(left->value_type) && type_integer(right->value_type)) {
    value_type = type_int();
  } else {
    error(token, "operands of %s operator should have integer type.", token->type_name);
  }

  return node_binary_expr(MOD, value_type, left, right, token);
}

Node *node_add(Node *left, Node *right, Token *token) {
  Type *value_type;
  if (type_integer(left->value_type) && type_integer(right->value_type)) {
    value_type = type_int();
  } else if (type_pointer(left->value_type) && type_integer(right->value_type)) {
    value_type = left->value_type;
  } else if (type_integer(left->value_type) && type_pointer(right->value_type)) {
    Node *temp = left;
    left = right;
    right = temp;
    value_type = left->value_type;
  } else {
    error(token, "invalid operand types for %s operator.", token->type_name);
  }

  return node_binary_expr(ADD, value_type, left, right, token);
}

Node *node_sub(Node *left, Node *right, Token *token) {
  Type *value_type;
  if (type_integer(left->value_type) && type_integer(right->value_type)) {
    value_type = type_int();
  } else if (type_pointer(left->value_type) && type_integer(right->value_type)) {
    value_type = left->value_type;
  } else {
    error(token, "invalid operand types for %s operator.", token->type_name);
  }

  return node_binary_expr(SUB, value_type, left, right, token);
}

Node *node_shift(NodeType type, Node *left, Node *right, Token *token) {
  Type *value_type;
  if (type_integer(left->value_type) && type_integer(right->value_type)) {
    value_type = type_int();
  } else {
    if (type == LSHIFT) {
      error(token, "operands of << operator should be integer type.");
    } else if (type == RSHIFT) {
      error(token, "operands of >> operator should be integer type.");
    }
  }

  return node_binary_expr(type, value_type, left, right, token);
}

Node *node_relational(NodeType type, Node *left, Node *right, Token *token) {
  Type *value_type;
  if (type_integer(left->value_type) && type_integer(right->value_type)) {
    value_type = type_int();
  } else if (type_pointer(left->value_type) && type_pointer(right->value_type)) {
    value_type = type_int();
  } else {
    if (type == LT) {
      error(token, "operands of < operator should be integer type.");
    } else if (type == GT) {
      error(token, "operands of > operator should be integer type.");
    } else if (type == LTE) {
      error(token, "operands of <= operator should be integer type.");
    } else if (type == GTE) {
      error(token, "operands of >= operator should be integer type.");
    }
  }

  return node_binary_expr(type, value_type, left, right, token);
}

Node *node_equality(NodeType type, Node *left, Node *right, Token *token) {
  Type *value_type;
  if (type_integer(left->value_type) && type_integer(right->value_type)) {
    value_type = type_int();
  } else if (type_pointer(left->value_type) && type_pointer(right->value_type)) {
    value_type = type_int();
  } else {
    if (type == EQ) {
      error(token, "operands of == operator should be integer type.");
    } else if (type == NEQ) {
      error(token, "operands of != operator should be integer type.");
    }
  }

  return node_binary_expr(type, value_type, left, right, token);
}

Node *node_bitwise(NodeType type, Node *left, Node *right, Token *token) {
  Type *value_type;
  if (type_integer(left->value_type) && type_integer(right->value_type)) {
    value_type = type_int();
  } else {
    if (type == AND) {
      error(token, "operands of & operator should be integer type.");
    } else if (type == XOR) {
      error(token, "operands of ^ operator should be integer type.");
    } else if (type == OR) {
      error(token, "operands of | operator should be integer type.");
    }
  }

  return node_binary_expr(type, value_type, left, right, token);
}

Node *node_logical(NodeType type, Node *left, Node *right, Token *token) {
  Type *value_type;
  if (type_scalar(left->value_type) && type_scalar(right->value_type)) {
    value_type = type_int();
  } else {
    if (type == LAND) {
      error(token, "operands of && operator should be integer type.");
    } else if (type == LOR) {
      error(token, "operands of || operator should be integer type.");
    }
  }

  return node_binary_expr(type, value_type, left, right, token);
}

Node *node_conditional(Node *control, Node *left, Node *right, Token *token) {
  Type *value_type;
  if (type_integer(left->value_type) && type_integer(right->value_type)) {
    value_type = type_int();
  } else if (type_pointer(left->value_type) && type_pointer(right->value_type)) {
    value_type = left->value_type;
  } else {
    error(token, "second and third operands of ?: operator should have the same type.");
  }

  Node *node = node_new();
  node->type = CONDITION;
  node->value_type = value_type;
  node->control = control;
  node->left = left;
  node->right = right;
  node->token = token;
  return node;
}

Node *node_assign(Node *left, Node *right, Token *token) {
  if (!node_lvalue(left)) {
    error(token, "left side of %s operator should be lvalue.", token->type_name);
  }

  return node_binary_expr(ASSIGN, left->value_type, left, right, token);
}

Node *node_compound_assign(NodeType type, Node *left, Node *right, Token *token) {
  Symbol *symbol = symbol_new();
  symbol->token = token;
  symbol->identifier = NULL;
  symbol->value_type = type_pointer_to(left->value_type);
  symbol_put(NULL, symbol);

  Node *id = node_identifier(NULL, symbol, token);
  Node *addr = node_address(left, token);
  Node *assign = node_assign(id, addr, token);
  Node *indirect = node_indirect(id, token);

  Node *op;
  if (type == ADD) op = node_add(indirect, right, token);
  else if (type == SUB) op = node_sub(indirect, right, token);
  else if (type == MUL) op = node_mul(indirect, right, token);
  else if (type == DIV) op = node_div(indirect, right, token);
  else if (type == MOD) op = node_mod(indirect, right, token);

  return node_comma(assign, node_assign(indirect, op, token), token);
}

Node *node_add_assign(Node *left, Node *right, Token *token) {
  return node_compound_assign(ADD, left, right, token);
}

Node *node_sub_assign(Node *left, Node *right, Token *token) {
  return node_compound_assign(SUB, left, right, token);
}

Node *node_mul_assign(Node *left, Node *right, Token *token) {
  return node_compound_assign(MUL, left, right, token);
}

Node *node_div_assign(Node *left, Node *right, Token *token) {
  return node_compound_assign(DIV, left, right, token);
}

Node *node_mod_assign(Node *left, Node *right, Token *token) {
  return node_compound_assign(MOD, left, right, token);
}

Node *node_comma(Node *left, Node *right, Token *token) {
  return node_binary_expr(COMMA, right->value_type, left, right, token);
}

Node *node_init(Node *init, Type *value_type) {
  Node *node = node_new();
  node->type = INIT;
  node->value_type = value_type;
  node->init = init;
  return node;
}

Node *node_array_init(Vector *array_init, Type *value_type) {
  Node *node = node_new();
  node->type = ARRAY_INIT;
  node->value_type = value_type;
  node->array_init = array_init;
  return node;
}

Node *node_decl(Vector *declarations) {
  Node *node = node_new();
  node->type = VAR_DECL;
  node->declarations = declarations;
  return node;
}

Node *node_comp_stmt(Vector *statements) {
  Node *node = node_new();
  node->type = COMP_STMT;
  node->statements = statements;
  return node;
}

Node *node_expr_stmt(Node *expr) {
  Node *node = node_new();
  node->type = EXPR_STMT;
  node->expr = expr;
  return node;
}

Node *node_if_stmt(Node *if_control, Node *if_body, Node *else_body) {
  Node *node = node_new();
  node->type = IF_STMT;
  node->if_control = if_control;
  node->if_body = if_body;
  node->else_body = else_body;
  return node;
}

Node *node_while_stmt(Node *loop_control, Node *loop_body) {
  Node *node = node_new();
  node->type = WHILE_STMT;
  node->loop_control = loop_control;
  node->loop_body = loop_body;
  return node;
}

Node *node_do_while_stmt(Node *loop_control, Node *loop_body) {
  Node *node = node_new();
  node->type = DO_WHILE_STMT;
  node->loop_control = loop_control;
  node->loop_body = loop_body;
  return node;
}

Node *node_for_stmt(Node *loop_init, Node *loop_control, Node *loop_afterthrough, Node *loop_body) {
  Node *node = node_new();
  node->type = FOR_STMT;
  node->loop_init = loop_init;
  node->loop_control = loop_control;
  node->loop_afterthrough = loop_afterthrough;
  node->loop_body = loop_body;
  return node;
}

Node *node_continue_stmt(int continue_level, Token *token) {
  if (continue_level == 0) {
    error(token, "continue statement should appear in loops.");
  }

  Node *node = node_new();
  node->type = CONTINUE_STMT;
  node->token = token;
  return node;
}

Node *node_break_stmt(int break_level, Token *token) {
  if (break_level == 0) {
    error(token, "break statement should appear in loops.");
  }

  Node *node = node_new();
  node->type = BREAK_STMT;
  node->token = token;
  return node;
}

Node *node_return_stmt(Node *expr) {
  Node *node = node_new();
  node->type = RETURN_STMT;
  node->expr = expr;
  return node;
}

Node *node_func_def(Symbol *symbol, Node *function_body, int local_vars_size, Token *token) {
  if (symbol->value_type->function_returning->type == ARRAY) {
    error(symbol->token, "returning type of function should not be array type.");
  }

  if (symbol->value_type->params->length > 6) {
    error(token, "too many parameters.");
  }

  Node *node = node_new();
  node->type = FUNC_DEF;
  node->symbol = symbol;
  node->local_vars_size = local_vars_size;
  node->function_body = function_body;
  node->token = token;
  return node;
}

Node *node_trans_unit(Vector *string_literals, Vector *declarations, Vector *definitions) {
  Node *node = node_new();
  node->type = TLANS_UNIT;
  node->string_literals = string_literals;
  node->declarations = declarations;
  node->definitions = definitions;
  return node;
}
