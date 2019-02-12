#include "sk2cc.h"

bool node_lvalue(Node *node) {
  return node->nd_type == IDENTIFIER || node->nd_type == INDIRECT || node->nd_type == DOT;
}

Node *node_new() {
  Node *node = (Node *) calloc(1, sizeof(Node));
  return node;
}

Node *node_int_const(int int_value, Token *token) {
  Node *node = node_new();
  node->nd_type = INT_CONST;
  node->type = type_int();
  node->int_value = int_value;
  node->token = token;
  return node;
}

Node *node_string_literal(String *string_value, int string_label, Token *token) {
  Node *node = node_new();
  node->nd_type = STRING_LITERAL;
  node->type = type_convert(type_array_of(type_char(), string_value->length));
  node->string_value = string_value;
  node->string_label = string_label;
  node->token = token;
  return node;
}

Node *node_identifier(char *identifier, Symbol *symbol, Token *token) {
  Node *node = node_new();
  node->nd_type = IDENTIFIER;
  node->type = symbol ? type_convert(symbol->type) : type_int();
  node->identifier = identifier;
  node->symbol = symbol;
  node->token = token;
  return node;
}

Node *node_func_call(Node *expr, Vector *args, Token *token) {
  if (expr->nd_type != IDENTIFIER) {
    error(token, "invalid function call.");
  }

  if (args->length > 6) {
    error(token, "too many arguments.");
  }

  if (expr->symbol) {
    Vector *params = expr->type->params;

    if (!expr->type->ellipsis) {
      if (args->length != params->length) {
        error(token, "number of parameters should be %d, but got %d.", params->length, args->length);
      }
    } else {
      if (args->length < params->length) {
        error(token, "number of parameters should be %d or more, but got %d.", params->length, args->length);
      }
    }

    for (int i = 0; i < params->length; i++) {
      Node *arg = args->buffer[i];
      Symbol *param = params->buffer[i];
      if (!type_same(arg->type, param->type)) {
        error(token, "parameter types and argument types should be the same.");
      }
    }
  }

  Type *type;
  if (expr->symbol) {
    if (expr->type->ty_type != FUNCTION) {
      error(token, "operand of function call should have function type.");
    }
    type = expr->type->function_returning;
  } else {
    type = type_int();
  }

  Node *node = node_new();
  node->nd_type = FUNC_CALL;
  node->type = type;
  node->expr = expr;
  node->args = args;
  node->token = token;
  return node;
}

Node *node_dot(Node *expr, char *member, Token *token) {
  if (expr->type->ty_type != STRUCT) {
    error(token, "operand of . operator should have struct type.");
  }

  StructMember *s_member = map_lookup(expr->type->members, member);
  if (!s_member) {
    error(token, "undefined struct member: %s.", member);
  }

  Node *node = node_new();
  node->nd_type = DOT;
  node->type = type_convert(s_member->type);
  node->expr = expr;
  node->member = member;
  node->member_offset = s_member->offset;
  node->token = token;
  return node;
}

Node *node_post_inc(Node *expr, Token *token) {
  Symbol *sym_addr = symbol_new();
  sym_addr->token = token;
  sym_addr->identifier = NULL;
  sym_addr->type = type_pointer_to(expr->type);
  symbol_put(NULL, sym_addr);

  Symbol *sym_val = symbol_new();
  sym_val->token = token;
  sym_val->identifier = NULL;
  sym_val->type = expr->type;
  symbol_put(NULL, sym_val);

  Node *addr_id = node_identifier(NULL, sym_addr, token);
  Node *addr = node_address(expr, token);
  Node *addr_assign = node_assign(addr_id, addr, token);
  Node *val_id = node_identifier(NULL, sym_val, token);
  Node *val = node_indirect(addr_id, token);
  Node *val_assign = node_assign(val_id, val, token);
  Node *inc = node_pre_inc(val, token);

  return node_comma(node_comma(node_comma(addr_assign, val_assign, token), inc, token), val_id, token);
}

Node *node_post_dec(Node *expr, Token *token) {
  Symbol *sym_addr = symbol_new();
  sym_addr->token = token;
  sym_addr->identifier = NULL;
  sym_addr->type = type_pointer_to(expr->type);
  symbol_put(NULL, sym_addr);

  Symbol *sym_val = symbol_new();
  sym_val->token = token;
  sym_val->identifier = NULL;
  sym_val->type = expr->type;
  symbol_put(NULL, sym_val);

  Node *addr_id = node_identifier(NULL, sym_addr, token);
  Node *addr = node_address(expr, token);
  Node *addr_assign = node_assign(addr_id, addr, token);
  Node *val_id = node_identifier(NULL, sym_val, token);
  Node *val = node_indirect(addr_id, token);
  Node *val_assign = node_assign(val_id, val, token);
  Node *dec = node_pre_dec(val, token);

  return node_comma(node_comma(node_comma(addr_assign, val_assign, token), dec, token), val_id, token);
}

Node *node_unary_expr(NodeType nd_type, Type *type, Node *expr, Token *token) {
  Node *node = node_new();
  node->nd_type = nd_type;
  node->type = type;
  node->expr = expr;
  node->token = token;
  return node;
}

Node *node_pre_inc(Node *expr, Token *token) {
  return node_add_assign(expr, node_int_const(1, token), token);
}

Node *node_pre_dec(Node *expr, Token *token) {
  return node_sub_assign(expr, node_int_const(1, token), token);
}

Node *node_address(Node *expr, Token *token) {
  if (!node_lvalue(expr)) {
    error(token, "operand of %s operator should be lvalue.", token_name(token->tk_type));
  }

  return node_unary_expr(ADDRESS, type_pointer_to(expr->type), expr, token);
}

Node *node_indirect(Node *expr, Token *token) {
  if (!type_pointer(expr->type)) {
    error(token, "operand of %s operator should have pointer type.", token_name(token->tk_type));
  }

  return node_unary_expr(INDIRECT, type_convert(expr->type->pointer_to), expr, token);
}

Node *node_unary_arithmetic(NodeType nd_type, Node *expr, Token *token) {
  Type *type;
  if (nd_type == UPLUS || nd_type == UMINUS) {
    if (!type_integer(expr->type)) {
      if (nd_type == UPLUS) {
        error(token, "operand of unary + operator should have integer type.");
      } else if (nd_type == UMINUS) {
        error(token, "operand of unary - operator should have integer type.");
      }
    }
    type = expr->type;
  } else if (nd_type == NOT) {
    if (!type_integer(expr->type)) {
      error(token, "operand of ~ operator should have integer type.");
    }
    type = type_int();
  } else if (nd_type == LNOT) {
    if (!type_scalar(expr->type)) {
      error(token, "operand of ! operator should have scalar type.");
    }
    type = type_int();
  }

  return node_unary_expr(nd_type, type, expr, token);
}

Node *node_cast(Type *type, Node *expr, Token *token) {
  Node *node = node_new();
  node->nd_type = CAST;
  node->type = type;
  node->expr = expr;
  node->token = token;
  return node;
}

Node *node_binary_expr(NodeType nd_type, Type *type, Node *left, Node *right, Token *token) {
  Node *node = node_new();
  node->nd_type = nd_type;
  node->type = type;
  node->left = left;
  node->right = right;
  node->token = token;
  return node;
}

Type *semantics_mul(Node *left, Node *right, Token *token) {
  if (type_integer(left->type) && type_integer(right->type)) {
    return type_int();
  }

  error(token, "operands of %s operator should have integer type.", token_name(token->tk_type));
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
  Type *type;
  if (type_integer(left->type) && type_integer(right->type)) {
    type = type_int();
  } else {
    error(token, "operands of %s operator should have integer type.", token_name(token->tk_type));
  }

  return node_binary_expr(MOD, type, left, right, token);
}

Node *node_add(Node *left, Node *right, Token *token) {
  Type *type;
  if (type_integer(left->type) && type_integer(right->type)) {
    type = type_int();
  } else if (type_pointer(left->type) && type_integer(right->type)) {
    type = left->type;
  } else if (type_integer(left->type) && type_pointer(right->type)) {
    Node *temp = left;
    left = right;
    right = temp;
    type = left->type;
  } else {
    error(token, "invalid operand types for %s operator.", token_name(token->tk_type));
  }

  return node_binary_expr(ADD, type, left, right, token);
}

Node *node_sub(Node *left, Node *right, Token *token) {
  Type *type;
  if (type_integer(left->type) && type_integer(right->type)) {
    type = type_int();
  } else if (type_pointer(left->type) && type_integer(right->type)) {
    type = left->type;
  } else {
    error(token, "invalid operand types for %s operator.", token_name(token->tk_type));
  }

  return node_binary_expr(SUB, type, left, right, token);
}

Node *node_shift(NodeType nd_type, Node *left, Node *right, Token *token) {
  Type *type;
  if (type_integer(left->type) && type_integer(right->type)) {
    type = type_int();
  } else {
    if (nd_type == LSHIFT) {
      error(token, "operands of << operator should be integer type.");
    } else if (nd_type == RSHIFT) {
      error(token, "operands of >> operator should be integer type.");
    }
  }

  return node_binary_expr(nd_type, type, left, right, token);
}

Node *node_relational(NodeType nd_type, Node *left, Node *right, Token *token) {
  Type *type;
  if (type_integer(left->type) && type_integer(right->type)) {
    type = type_int();
  } else if (type_pointer(left->type) && type_pointer(right->type)) {
    type = type_int();
  } else {
    if (nd_type == LT) {
      error(token, "operands of < operator should be integer type.");
    } else if (nd_type == GT) {
      error(token, "operands of > operator should be integer type.");
    } else if (nd_type == LTE) {
      error(token, "operands of <= operator should be integer type.");
    } else if (nd_type == GTE) {
      error(token, "operands of >= operator should be integer type.");
    }
  }

  return node_binary_expr(nd_type, type, left, right, token);
}

Node *node_equality(NodeType nd_type, Node *left, Node *right, Token *token) {
  Type *type;
  if (type_integer(left->type) && type_integer(right->type)) {
    type = type_int();
  } else if (type_pointer(left->type) && type_pointer(right->type)) {
    type = type_int();
  } else {
    if (nd_type == EQ) {
      error(token, "operands of == operator should be integer type.");
    } else if (nd_type == NEQ) {
      error(token, "operands of != operator should be integer type.");
    }
  }

  return node_binary_expr(nd_type, type, left, right, token);
}

Node *node_bitwise(NodeType nd_type, Node *left, Node *right, Token *token) {
  Type *type;
  if (type_integer(left->type) && type_integer(right->type)) {
    type = type_int();
  } else {
    if (nd_type == AND) {
      error(token, "operands of & operator should be integer type.");
    } else if (nd_type == XOR) {
      error(token, "operands of ^ operator should be integer type.");
    } else if (nd_type == OR) {
      error(token, "operands of | operator should be integer type.");
    }
  }

  return node_binary_expr(nd_type, type, left, right, token);
}

Node *node_logical(NodeType nd_type, Node *left, Node *right, Token *token) {
  Type *type;
  if (type_scalar(left->type) && type_scalar(right->type)) {
    type = type_int();
  } else {
    if (nd_type == LAND) {
      error(token, "operands of && operator should be integer type.");
    } else if (nd_type == LOR) {
      error(token, "operands of || operator should be integer type.");
    }
  }

  return node_binary_expr(nd_type, type, left, right, token);
}

Node *node_conditional(Node *control, Node *left, Node *right, Token *token) {
  Type *type;
  if (type_integer(left->type) && type_integer(right->type)) {
    type = type_int();
  } else if (type_pointer(left->type) && type_pointer(right->type)) {
    type = left->type;
  } else {
    error(token, "second and third operands of ?: operator should have the same type.");
  }

  Node *node = node_new();
  node->nd_type = CONDITION;
  node->type = type;
  node->control = control;
  node->left = left;
  node->right = right;
  node->token = token;
  return node;
}

Node *node_assign(Node *left, Node *right, Token *token) {
  if (!node_lvalue(left)) {
    error(token, "left side of %s operator should be lvalue.", token_name(token->tk_type));
  }

  return node_binary_expr(ASSIGN, left->type, left, right, token);
}

Node *node_compound_assign(NodeType nd_type, Node *left, Node *right, Token *token) {
  Symbol *symbol = symbol_new();
  symbol->token = token;
  symbol->identifier = NULL;
  symbol->type = type_pointer_to(left->type);
  symbol_put(NULL, symbol);

  Node *id = node_identifier(NULL, symbol, token);
  Node *addr = node_address(left, token);
  Node *assign = node_assign(id, addr, token);
  Node *indirect = node_indirect(id, token);

  Node *op;
  if (nd_type == ADD) op = node_add(indirect, right, token);
  else if (nd_type == SUB) op = node_sub(indirect, right, token);
  else if (nd_type == MUL) op = node_mul(indirect, right, token);
  else if (nd_type == DIV) op = node_div(indirect, right, token);
  else if (nd_type == MOD) op = node_mod(indirect, right, token);

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
  return node_binary_expr(COMMA, right->type, left, right, token);
}

Node *node_init(Node *init, Type *type) {
  Node *node = node_new();
  node->nd_type = INIT;
  node->type = type;
  node->init = init;
  return node;
}

Node *node_array_init(Vector *array_init, Type *type) {
  Node *node = node_new();
  node->nd_type = ARRAY_INIT;
  node->type = type;
  node->array_init = array_init;
  return node;
}

Node *node_decl(Vector *declarations) {
  Node *node = node_new();
  node->nd_type = VAR_DECL;
  node->declarations = declarations;
  return node;
}

Node *node_comp_stmt(Vector *statements) {
  Node *node = node_new();
  node->nd_type = COMP_STMT;
  node->statements = statements;
  return node;
}

Node *node_expr_stmt(Node *expr) {
  Node *node = node_new();
  node->nd_type = EXPR_STMT;
  node->expr = expr;
  return node;
}

Node *node_if_stmt(Node *if_control, Node *if_body, Node *else_body) {
  Node *node = node_new();
  node->nd_type = IF_STMT;
  node->if_control = if_control;
  node->if_body = if_body;
  node->else_body = else_body;
  return node;
}

Node *node_while_stmt(Node *loop_control, Node *loop_body) {
  Node *node = node_new();
  node->nd_type = WHILE_STMT;
  node->loop_control = loop_control;
  node->loop_body = loop_body;
  return node;
}

Node *node_do_while_stmt(Node *loop_control, Node *loop_body) {
  Node *node = node_new();
  node->nd_type = DO_WHILE_STMT;
  node->loop_control = loop_control;
  node->loop_body = loop_body;
  return node;
}

Node *node_for_stmt(Node *loop_init, Node *loop_control, Node *loop_afterthrough, Node *loop_body) {
  Node *node = node_new();
  node->nd_type = FOR_STMT;
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
  node->nd_type = CONTINUE_STMT;
  node->token = token;
  return node;
}

Node *node_break_stmt(int break_level, Token *token) {
  if (break_level == 0) {
    error(token, "break statement should appear in loops.");
  }

  Node *node = node_new();
  node->nd_type = BREAK_STMT;
  node->token = token;
  return node;
}

Node *node_return_stmt(Node *expr) {
  Node *node = node_new();
  node->nd_type = RETURN_STMT;
  node->expr = expr;
  return node;
}

Node *node_func_def(Symbol *symbol, Node *function_body, int local_vars_size, Token *token) {
  if (symbol->type->function_returning->ty_type == ARRAY) {
    error(symbol->token, "returning type of function should not be array type.");
  }

  if (symbol->type->params->length > 6) {
    error(token, "too many parameters.");
  }

  Node *node = node_new();
  node->nd_type = FUNC_DEF;
  node->symbol = symbol;
  node->local_vars_size = local_vars_size;
  node->function_body = function_body;
  node->token = token;
  return node;
}

Node *node_trans_unit(Vector *string_literals, Vector *declarations, Vector *definitions) {
  Node *node = node_new();
  node->nd_type = TLANS_UNIT;
  node->string_literals = string_literals;
  node->declarations = declarations;
  node->definitions = definitions;
  return node;
}
