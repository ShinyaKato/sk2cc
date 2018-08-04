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
  return type == IDENTIFIER || type == INDIRECT;
}

Type *value_type_add(Type *left_type, Type *right_type) {
  if (type_integer(left_type) && type_integer(right_type)) {
    return type_int();
  } else if (left_type->type == POINTER && type_integer(right_type)) {
    return left_type;
  }
  return NULL;
}

Type *value_type_sub(Type *left_type, Type *right_type) {
  if (type_integer(left_type) && type_integer(right_type)) {
    return type_int();
  } else if (left_type->type == POINTER && type_integer(right_type)) {
    return left_type;
  }
  return NULL;
}

void analyze_expr(Node *node) {
  if (node->type == CONST) {
    node->value_type = type_int();
  }

  if (node->type == STRING_LITERAL) {
    String *str = node->string_value;
    Type *array = type_array_of(type_char(), str->length + 1);
    node->value_type = type_convert(array);
    node->string_label = string_literals->length;
    vector_push(string_literals, node->string_value);
  }

  if (node->type == IDENTIFIER) {
    Symbol *symbol = lookup_symbol(node->identifier);
    if (symbol) {
      node->symbol = symbol;
      node->value_type = type_convert(symbol->value_type);
    } else {
      node->symbol = NULL;
      node->value_type = type_int();
    }
  }

  if (node->type == FUNC_CALL) {
    analyze_expr(node->expr);
    if (node->expr->type != IDENTIFIER) {
      error("invalid function call.");
    }
    if (node->args->length > 6) {
      error("too many arguments.");
    }
    for (int i = 0; i < node->args->length; i++) {
      Node *arg = node->args->array[i];
      analyze_expr(arg);
    }
    node->value_type = node->expr->value_type;
  }

  if (node->type == POST_INC) {
    analyze_expr(node->expr);
    if (!check_lvalue(node->expr->type)) {
      error("operand of postfix increment should be identifier or indirect operator.");
    }
    node->value_type = value_type_add(node->expr->value_type, type_int());
    if (!node->value_type) {
      error("invalid operand types for postfix increment.");
    }
  }

  if (node->type == POST_DEC) {
    analyze_expr(node->expr);
    if (!check_lvalue(node->expr->type)) {
      error("operand of postfix decrement should be identifier or indirect operator.");
    }
    node->value_type = value_type_sub(node->expr->value_type, type_int());
    if (!node->value_type) {
      error("invalid operand types for postfix decrement.");
    }
  }

  if (node->type == PRE_INC) {
    analyze_expr(node->expr);
    if (!check_lvalue(node->expr->type)) {
      error("operand of prefix increment should be identifier or indirect operator.");
    }
    node->value_type = value_type_add(node->expr->value_type, type_int());
    if (!node->value_type) {
      error("invalid operand types for prefix increment.");
    }
  }

  if (node->type == PRE_DEC) {
    analyze_expr(node->expr);
    if (!check_lvalue(node->expr->type)) {
      error("operand of prefix decrement should be identifier or indirect operator.");
    }
    node->value_type = value_type_add(node->expr->value_type, type_int());
    if (!node->value_type) {
      error("invalid operand types for prefix decrement.");
    }
  }

  if (node->type == ADDRESS) {
    analyze_expr(node->expr);
    if (node->expr->type == IDENTIFIER) {
      node->value_type = type_pointer_to(node->expr->value_type);
    } else {
      error("operand of unary & operator should be identifier.");
    }
  }

  if (node->type == INDIRECT) {
    analyze_expr(node->expr);
    if (node->expr->value_type->type == POINTER) {
      node->value_type = type_convert(node->expr->value_type->pointer_to);
    } else {
      error("operand of unary * operator should have pointer type.");
    }
  }

  if (node->type == UPLUS) {
    analyze_expr(node->expr);
    if (type_integer(node->expr->value_type)) {
      node->value_type = node->expr->value_type;
    } else {
      error("operand of unary + operator should have integer type.");
    }
  }

  if (node->type == UMINUS) {
    analyze_expr(node->expr);
    if (type_integer(node->expr->value_type)) {
      node->value_type = node->expr->value_type;
    } else {
      error("operand of unary - operator should have integer type.");
    }
  }

  if (node->type == NOT) {
    analyze_expr(node->expr);
    if (type_integer(node->expr->value_type)) {
      node->value_type = node->expr->value_type;
    } else {
      error("operand of ~ operator should have integer type.");
    }
  }

  if (node->type == LNOT) {
    analyze_expr(node->expr);
    node->value_type = node->expr->value_type;
    if (type_integer(node->expr->value_type)) {
      node->value_type = node->expr->value_type;
    } else {
      error("operand of ! operator should have integer type.");
    }
  }

  if (node->type == SIZEOF) {
    analyze_expr(node->expr);
    node->type = CONST;
    node->value_type = type_int();
    node->int_value = node->expr->value_type->original_size;
  }

  if (node->type == MUL || node->type == DIV) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else {
      if (node->type == MUL) error("operands of binary * should have integer type.");
      if (node->type == DIV) error("operands of / should have integer type.");
    }
  }

  if (node->type == MOD) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else {
      error("operands of %% operator should have integer type.");
    }
  }

  if (node->type == ADD) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    node->value_type = value_type_add(node->left->value_type, node->right->value_type);
    if (!node->value_type) {
      error("invalid operand types for binary + operator.");
    }
  }

  if (node->type == SUB) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    node->value_type = value_type_sub(node->left->value_type, node->right->value_type);
    if (!node->value_type) {
      error("invalid operand types for binary - operator.");
    }
  }

  if (node->type == LSHIFT || node->type == RSHIFT) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else {
      error("operands of shift operators should be integer type.");
    }
  }

  if (node->type == LT || node->type == GT || node->type == LTE || node->type == GTE) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else {
      error("operands of relational operators should be integer type.");
    }
  }

  if (node->type == EQ || node->type == NEQ) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else {
      error("operands of equality operators should be integer type.");
    }
  }

  if (node->type == AND || node->type == XOR || node->type == OR) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else {
      error("operands of bit operators should be integer type.");
    }
  }

  if (node->type == LAND || node->type == LOR) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else {
      error("operands of logical and/or operators should be integer type.");
    }
  }

  if (node->type == CONDITION) {
    analyze_expr(node->control);
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (!type_integer(node->control->value_type)) {
      error("control expression of conditional-expression should have integer type.");
    }
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else {
      error("operands of conditional-expression should be integer type.");
    }
  }

  if (node->type == ASSIGN) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (!check_lvalue(node->left->type)) {
      error("left side of assignment operator should be identifier or indirect operator.");
    }
    node->value_type = node->left->value_type;
  }

  if (node->type == ADD_ASSIGN) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    node->value_type = value_type_add(node->left->value_type, node->right->value_type);
    if (!check_lvalue(node->left->type)) {
      error("left side of assignment operator should be identifier or indirect operator.");
    }
  }

  if (node->type == SUB_ASSIGN) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    node->value_type = value_type_sub(node->left->value_type, node->right->value_type);
    if (!check_lvalue(node->left->type)) {
      error("left side of assignment operator should be identifier or indirect operator.");
    }
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

void analyze_comp_stmt(Node *node);

void analyze_stmt(Node *node) {
  if (node->type == COMP_STMT) {
    make_scope();
    analyze_comp_stmt(node);
    remove_scope();
  }

  if (node->type == EXPR_STMT) {
    analyze_expr(node->expr);
  }

  if (node->type == IF_STMT) {
    analyze_expr(node->control);
    analyze_stmt(node->if_body);
  }

  if (node->type == IF_ELSE_STMT) {
    analyze_expr(node->control);
    analyze_stmt(node->if_body);
    analyze_stmt(node->else_body);
  }

  if (node->type == WHILE_STMT) {
    continue_level++;
    break_level++;

    analyze_expr(node->control);
    analyze_stmt(node->loop_body);

    continue_level--;
    break_level--;
  }

  if (node->type == DO_WHILE_STMT) {
    continue_level++;
    break_level++;

    analyze_expr(node->control);
    analyze_stmt(node->loop_body);

    continue_level--;
    break_level--;
  }

  if (node->type == FOR_STMT) {
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

  if (node->type == CONTINUE_STMT) {
    if (continue_level == 0) {
      error("continue statement should appear in loops.");
    }
  }

  if (node->type == BREAK_STMT) {
    if (break_level == 0) {
      error("break statement should appear in loops.");
    }
  }

  if (node->type == RETURN_STMT) {
    if (node->expr) analyze_expr(node->expr);
  }
}

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

void analyze_func_def(Node *node) {
  put_symbol(node->symbol->identifier, node->symbol);

  local_vars_size = 0;
  make_scope();
  if (node->param_symbols->length > 6) {
    error("too many parameters.");
  }
  for (int i = 0; i < node->param_symbols->length; i++) {
    Symbol *param = node->param_symbols->array[i];
    if (lookup_symbol(param->identifier)) {
      error("duplicated parameter declaration.");
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
