#include "cc.h"

Vector *string_literals;
Vector *scopes;
int local_vars_size;
int break_level, continue_level;

Symbol *lookup_symbol(char *identifier) {
  for (int i = scopes->length - 1; i >= 0; i--) {
    Map *map = scopes->array[i];
    Symbol *symbol = map_lookup(map, identifier);
    if (symbol) return symbol;
  }
  return NULL;
}

void put_symbol(char *identifier, Symbol *symbol) {
  Map *map = scopes->array[scopes->length - 1];
  if (map_lookup(map, identifier)) {
    error("duplicated function or variable definition of '%s'.", identifier);
  }

  if (scopes->length == 1) {
    symbol->type = GLOBAL;
  } else {
    int size = symbol->value_type->size;
    if (size % 8 != 0) size = size / 8 * 8 + 8;
    local_vars_size += size;

    symbol->type = LOCAL;
    symbol->offset = local_vars_size;
  }

  map_put(map, identifier, symbol);
}

void make_scope() {
  vector_push(scopes, map_new());
}

void remove_scope() {
  vector_pop(scopes);
}

bool check_lvalue(NodeType type) {
  return type == IDENTIFIER || type == INDIRECT || type == DOT;
}

void analyze_expr(Node *node);

void analyze_const(Node *node) {
  node->value_type = type_int();
}

void analyze_string_literal(Node *node) {
  String *string_value = node->string_value;
  Type *type = type_array_of(type_char(), string_value->length);
  node->value_type = type_convert(type);
  node->string_label = string_literals->length;
  vector_push(string_literals, node->string_value);
}

void analyze_identifier(Node *node) {
  Symbol *symbol = lookup_symbol(node->identifier);
  if (symbol) {
    node->symbol = symbol;
    node->value_type = type_convert(symbol->value_type);
  } else {
    node->symbol = NULL;
    node->value_type = type_int();
  }
}

void analyze_func_call(Node *node) {
  analyze_expr(node->expr);
  for (int i = 0; i < node->args->length; i++) {
    Node *arg = node->args->array[i];
    analyze_expr(arg);
  }
  node->value_type = node->expr->value_type;

  if (node->expr->type != IDENTIFIER) {
    error("invalid function call.");
  }
  if (node->args->length > 6) {
    error("too many arguments.");
  }
}

void analyze_dot(Node *node) {
  analyze_expr(node->expr);
  if (node->expr->value_type->type != STRUCT) {
    error("operand of . operator should have struct type.");
  }
  node->value_type = map_lookup(node->expr->value_type->members, node->identifier);
  node->value_type = type_convert(node->value_type);
  if (node->value_type == NULL) {
    error("undefined struct member.");
  }
  int *offset = map_lookup(node->expr->value_type->offsets, node->identifier);
  node->member_offset = *offset;
}

void analyze_increment_operator(Node *node) {
  analyze_expr(node->expr);
  if (!check_lvalue(node->expr->type)) {
    if (node->type == POST_INC) {
      error("operand of postfix ++ operator should be lvalue.");
    }
    if (node->type == PRE_INC) {
      error("operand of prefix ++ operator should be lvalue.");
    }
  }
  if (type_integer(node->expr->value_type)) {
    node->value_type = type_int();
  } else if (type_pointer(node->expr->value_type)) {
    node->value_type = node->expr->value_type;
  } else {
    if (node->type == POST_INC) {
      error("invalid operand type for postfix ++ operator.");
    }
    if (node->type == PRE_INC) {
      error("invalid operand type for prefix ++ operator.");
    }
  }
}

void analyze_decrement_operator(Node *node) {
  analyze_expr(node->expr);
  if (!check_lvalue(node->expr->type)) {
    if (node->type == POST_DEC) {
      error("operand of postfix -- operator should be lvalue.");
    }
    if (node->type == PRE_DEC) {
      error("operand of prefix -- operator should be lvalue.");
    }
  }
  if (type_integer(node->expr->value_type)) {
    node->value_type = type_int();
  } else if (type_pointer(node->expr->value_type)) {
    node->value_type = node->expr->value_type;
  } else {
    if (node->type == POST_DEC) {
      error("invalid operand type for postfix -- operator.");
    }
    if (node->type == PRE_DEC) {
      error("invalid operand type for prefix -- operator.");
    }
  }
}

void analyze_address(Node *node) {
  analyze_expr(node->expr);
  node->value_type = type_pointer_to(node->expr->value_type);
  if (node->expr->type != IDENTIFIER) {
    error("operand of unary & operator should be identifier.");
  }
}

void analyze_indirect(Node *node) {
  analyze_expr(node->expr);
  if (type_pointer(node->expr->value_type)) {
    node->value_type = type_convert(node->expr->value_type->pointer_to);
  } else {
    error("operand of unary * operator should have pointer type.");
  }
}

void analyze_uarithmetic_operator(Node *node) {
  analyze_expr(node->expr);
  if (type_integer(node->expr->value_type)) {
    node->value_type = type_int();
  } else {
    if (node->type == UPLUS) {
      error("operand of unary + operator should have integer type.");
    }
    if (node->type == UMINUS) {
      error("operand of unary - operator should have integer type.");
    }
  }
}

void analyze_not(Node *node) {
  analyze_expr(node->expr);
  if (type_integer(node->expr->value_type)) {
    node->value_type = type_int();
  } else  {
    error("operand of ~ operator should have integer type.");
  }
}

void analyze_lnot(Node *node) {
  analyze_expr(node->expr);
  if (type_scalar(node->expr->value_type)) {
    node->value_type = type_int();
  } else {
    error("operand of ! operator should have integer type.");
  }
}

void analyze_sizeof(Node *node) {
  analyze_expr(node->expr);
  node->type = CONST;
  node->value_type = type_int();
  node->int_value = node->expr->value_type->original_size;
}

void analyze_multiprecative_operator(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else {
    if (node->type == MUL) {
      error("operands of binary * should have integer type.");
    }
    if (node->type == DIV) {
    error("operands of / should have integer type.");
    }
  }
}

void analyze_mod(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else {
    error("operands of %% operator should have integer type.");
  }
}

void analyze_add(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else if (type_pointer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = node->left->value_type;
  } else if (type_integer(node->left->value_type) && type_pointer(node->right->value_type)) {
    Node *left = node->left;
    node->left = node->right;
    node->right = left;
    node->value_type = node->left->value_type;
  } else {
    error("invalid operand types for binary + operator.");
  }
}

void analyze_sub(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else if (type_pointer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = node->left->value_type;
  } else {
    error("invalid operand types for binary - operator.");
  }
}

void analyze_bitwise_operator(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else {
    if (node->type == LSHIFT) {
      error("operands of << operator should be integer type.");
    }
    if (node->type == RSHIFT) {
      error("operands of >> operator should be integer type.");
    }
    if (node->type == AND) {
      error("operands of binary & operator should be integer type.");
    }
    if (node->type == XOR) {
      error("operands of ^ operator should be integer type.");
    }
    if (node->type == OR) {
      error("operands of | operator should be integer type.");
    }
  }
}

void analyze_relational_operator(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else if (type_pointer(node->left->value_type) && type_pointer(node->right->value_type)) {
    node->value_type = type_int();
  } else {
    if (node->type == LT) {
      error("operands of < operator should be integer type.");
    }
    if (node->type == GT) {
      error("operands of > operator should be integer type.");
    }
    if (node->type == LTE) {
      error("operands of <= operator should be integer type.");
    }
    if (node->type == GTE) {
      error("operands of >= operator should be integer type.");
    }
  }
}

void analyze_equality_operator(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else if (type_pointer(node->left->value_type) && type_pointer(node->right->value_type)) {
    node->value_type = type_int();
  } else {
    if (node->type == EQ) {
      error("operands of == operator should be integer type.");
    }
    if (node->type == NEQ) {
      error("operands of != operator should be integer type.");
    }
  }
}

void analyze_logical_operator(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (type_scalar(node->left->value_type) && type_scalar(node->right->value_type)) {
    node->value_type = type_int();
  } else {
    if (node->type == LAND) {
      error("operands of && operator should be integer type.");
    }
    if (node->type == LOR) {
      error("operands of || operator should be integer type.");
    }
  }
}

void analyze_condition(Node *node) {
  analyze_expr(node->control);
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else if (type_pointer(node->left->value_type) && type_pointer(node->right->value_type)) {
    node->value_type = node->left->value_type;
  } else {
    error("second and third operands of ?: operator should be integer type.");
  }
}

void analyze_assign(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  node->value_type = node->left->value_type;
  if (!check_lvalue(node->left->type)) {
    error("left side of = operator should be lvalue.");
  }
}

void analyze_add_assign(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (!check_lvalue(node->left->type)) {
    error("left side of += operator should be lvalue.");
  }
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else if (type_pointer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = node->left->value_type;
  } else {
    error("invalid operand types for += operator.");
  }
}

void analyze_sub_assign(Node *node) {
  analyze_expr(node->left);
  analyze_expr(node->right);
  if (!check_lvalue(node->left->type)) {
    error("left side of -= operator should be lvalue.");
  }
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = type_int();
  } else if (type_pointer(node->left->value_type) && type_integer(node->right->value_type)) {
    node->value_type = node->left->value_type;
  } else {
    error("invalid operand types for -= operator.");
  }
}

void analyze_expr(Node *node) {
  NodeType type = node->type;
  if (type == CONST) {
    analyze_const(node);
  } else if (type == STRING_LITERAL) {
    analyze_string_literal(node);
  } else if (type == IDENTIFIER) {
    analyze_identifier(node);
  } else if (type == FUNC_CALL) {
    analyze_func_call(node);
  } else if (type == DOT) {
    analyze_dot(node);
  } else if (type == POST_INC || type == PRE_INC) {
    analyze_increment_operator(node);
  } else if (type == POST_DEC || type == PRE_DEC) {
    analyze_decrement_operator(node);
  } else if (type == ADDRESS) {
    analyze_address(node);
  } else if (type == INDIRECT) {
    analyze_indirect(node);
  } else if (type == UPLUS || type == UMINUS) {
    analyze_uarithmetic_operator(node);
  } else if (type == NOT) {
    analyze_not(node);
  } else if (type == LNOT) {
    analyze_lnot(node);
  } else if (type == SIZEOF) {
    analyze_sizeof(node);
  } else if (type == MUL || type == DIV) {
    analyze_multiprecative_operator(node);
  } else if (type == MOD) {
    analyze_mod(node);
  } else if (type == ADD) {
    analyze_add(node);
  } else if (type == SUB) {
    analyze_sub(node);
  } else if (type == LSHIFT || type == RSHIFT || type == AND || type == XOR || type == OR) {
    analyze_bitwise_operator(node);
  } else if (type == LT || type == GT || type == LTE || type == GTE) {
    analyze_relational_operator(node);
  } else if (type == EQ || type == NEQ) {
    analyze_equality_operator(node);
  } else if (type == LAND || type == LOR) {
    analyze_logical_operator(node);
  } else if (type == CONDITION) {
    analyze_condition(node);
  } else if (type == ASSIGN) {
    analyze_assign(node);
  } else if (type == ADD_ASSIGN) {
    analyze_add_assign(node);
  } else if (type == SUB_ASSIGN) {
    analyze_sub_assign(node);
  }
}

void analyze_var_decl(Node *node) {
  for (int i = 0; i < node->declarations->length; i++) {
    Node *init_decl = node->declarations->array[i];
    Symbol *symbol = init_decl->symbol;
    put_symbol(symbol->identifier, symbol);
    if (init_decl->initializer) {
      analyze_expr(init_decl->initializer);
    }
  }
}

void analyze_stmt(Node *node);

void analyze_comp_stmt(Node *node) {
  for (int i = 0; i < node->statements->length; i++) {
    Node *stmt = node->statements->array[i];
    if (stmt->type == VAR_DECL) {
      analyze_var_decl(stmt);
    } else {
      analyze_stmt(stmt);
    }
  }
}

void analyze_expr_stmt(Node *node) {
  analyze_expr(node->expr);
}

void analyze_if_stmt(Node *node) {
  analyze_expr(node->control);
  analyze_stmt(node->if_body);
}

void analyze_if_else_stmt(Node *node) {
  analyze_expr(node->control);
  analyze_stmt(node->if_body);
  analyze_stmt(node->else_body);
}

void analyze_while_stmt(Node *node) {
  continue_level++;
  break_level++;

  analyze_expr(node->control);
  analyze_stmt(node->loop_body);

  continue_level--;
  break_level--;
}

void analyze_do_while_stmt(Node *node) {
  continue_level++;
  break_level++;

  analyze_expr(node->control);
  analyze_stmt(node->loop_body);

  continue_level--;
  break_level--;
}

void analyze_for_stmt(Node *node) {
  continue_level++;
  break_level++;

  make_scope();
  if (node->init) {
    if (node->init->type == VAR_DECL) {
      analyze_var_decl(node->init);
    } else {
      analyze_expr(node->init);
    }
  }
  if (node->control) analyze_expr(node->control);
  if (node->afterthrough) analyze_expr(node->afterthrough);
  analyze_stmt(node->loop_body);
  remove_scope();

  continue_level--;
  break_level--;
}

void analyze_continue_stmt(Node *node) {
  if (continue_level == 0) {
    error("continue statement should appear in loops.");
  }
}

void analyze_break_stmt(Node *node) {
  if (break_level == 0) {
    error("break statement should appear in loops.");
  }
}

void analyze_return_stmt(Node *node) {
  if (node->expr) analyze_expr(node->expr);
}

void analyze_stmt(Node *node) {
  if (node->type == COMP_STMT) {
    make_scope();
    analyze_comp_stmt(node);
    remove_scope();
  } else if (node->type == EXPR_STMT) {
    analyze_expr_stmt(node);
  } else if (node->type == IF_STMT) {
    analyze_if_stmt(node);
  } else if (node->type == IF_ELSE_STMT) {
    analyze_if_else_stmt(node);
  } else if (node->type == WHILE_STMT) {
    analyze_while_stmt(node);
  } else if (node->type == DO_WHILE_STMT) {
    analyze_do_while_stmt(node);
  } else if (node->type == FOR_STMT) {
    analyze_for_stmt(node);
  } else if (node->type == CONTINUE_STMT) {
    analyze_continue_stmt(node);
  } else if (node->type == BREAK_STMT) {
    analyze_break_stmt(node);
  } else if (node->type == RETURN_STMT) {
    analyze_return_stmt(node);
  }
}

void analyze_func_def(Node *node) {
  if (node->symbol->value_type->type == ARRAY) {
    error("returning type of function should not be array type.");
  }
  put_symbol(node->symbol->identifier, node->symbol);

  local_vars_size = 0;
  make_scope();
  if (node->param_symbols->length > 6) {
    error("too many parameters.");
  }
  for (int i = 0; i < node->param_symbols->length; i++) {
    Symbol *param = node->param_symbols->array[i];
    if (param->value_type->type == ARRAY) {
      error("type of function parameter should not be array type.");
    }
    put_symbol(param->identifier, param);
  }
  analyze_comp_stmt(node->function_body);
  remove_scope();

  node->local_vars_size = local_vars_size;
}

void analyze_trans_unit(Node *node) {
  for (int i = 0; i < node->definitions->length; i++) {
    Node *def = node->definitions->array[i];
    if (def->type == VAR_DECL) {
      analyze_var_decl(def);
    } else if (def->type == FUNC_DEF) {
      analyze_func_def(def);
    }
  }
}

void analyze(Node *node) {
  string_literals = vector_new();

  scopes = vector_new();
  make_scope();

  break_level = 0;
  continue_level = 0;

  analyze_trans_unit(node);

  node->string_literals = string_literals;
}
