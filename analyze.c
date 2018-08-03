#include "cc.h"

Vector *string_literals;

Map *external_symbols, *symbols;
int local_vars_size;

int break_level = 0, continue_level = 0;

int put_local_variable(int size) {
  int align_size = size / 8 * 8;
  if (size % 8 != 0) align_size += 8;
  return local_vars_size += align_size;
}

void analyze_expr(Node *node) {
  if (node->type == CONST) {
    node->value_type = type_int();
  }

  if (node->type == STRING_LITERAL) {
    node->value_type = type_pointer_to(type_char());
    node->string_label = string_literals->length;
    vector_push(string_literals, node->string_value);
  }

  if (node->type == IDENTIFIER) {
    if (map_lookup(symbols, node->identifier)) {
      Symbol *symbol = map_lookup(symbols, node->identifier);
      node->symbol = symbol;
      node->value_type = type_convert(symbol->value_type);
    } else if (map_lookup(external_symbols, node->identifier)) {
      Symbol *symbol = map_lookup(external_symbols, node->identifier);
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
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else if (node->left->value_type->type == POINTER && type_integer(node->right->value_type)) {
      node->value_type = node->left->value_type;
    } else {
      error("invalid operand types for binary + operator.");
    }
  }

  if (node->type == SUB) {
    analyze_expr(node->left);
    analyze_expr(node->right);
    if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
      node->value_type = type_int();
    } else if (node->left->value_type->type == POINTER && type_integer(node->right->value_type)) {
      node->value_type = node->left->value_type;
    } else {
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
    if (node->left->type != IDENTIFIER && node->left->type != INDIRECT) {
      error("left side of assignment operator should be identifier or indirect operator.");
    }
    node->value_type = node->left->value_type;
  }
}

void analyze_var_decl(Node *node) {
  for (int i = 0; i < node->var_symbols->length; i++) {
    Symbol *symbol = node->var_symbols->array[i];
    symbol->offset = put_local_variable(symbol->value_type->size);
    if (symbols == NULL) {
      symbol->type = GLOBAL;
      map_put(external_symbols, symbol->identifier, symbol);
    } else {
      symbol->type = LOCAL;
      map_put(symbols, symbol->identifier, symbol);
    }
  }
}

void analyze_stmt(Node *node) {
  if (node->type == COMP_STMT) {
    for (int i = 0; i < node->statements->length; i++) {
      Node *stmt = node->statements->array[i];
      if (stmt->type == VAR_DECL) {
        analyze_var_decl(stmt);
      } else {
        analyze_stmt(stmt);
      }
    }
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

    if (node->init) analyze_expr(node->init);
    if (node->control) analyze_expr(node->control);
    if (node->afterthrough) analyze_expr(node->afterthrough);
    analyze_stmt(node->loop_body);

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

void analyze_func_def(Node *node) {
  symbols = map_new();
  local_vars_size = 0;

  if (map_lookup(external_symbols, node->symbol->identifier)) {
    error("duplicated function definition.");
  }
  map_put(external_symbols, node->symbol->identifier, node->symbol);

  if (node->param_symbols->length > 6) {
    error("too many parameters.");
  }
  for (int i = 0; i < node->param_symbols->length; i++) {
    Symbol *param = node->param_symbols->array[i];
    if (map_lookup(symbols, param->identifier)) {
      error("duplicated parameter declaration.");
    }
    param->offset = put_local_variable(param->value_type->size);
    param->type = LOCAL;
    map_put(symbols, param->identifier, param);
  }

  analyze_stmt(node->function_body);

  node->local_vars_size = local_vars_size;

  symbols = NULL;
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

  external_symbols = map_new();
  symbols = NULL;

  break_level = 0;
  continue_level = 0;

  analyze_trans_unit(node);

  node->string_literals = string_literals;
}
