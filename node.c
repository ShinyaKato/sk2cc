#include "cc.h"

Node *node_new() {
  Node *node = (Node *) calloc(1, sizeof(Node));
  return node;
}

Node *node_int_const(int int_value, Token *token) {
  Node *node = node_new();
  node->type = INT_CONST;
  node->int_value = int_value;
  node->token = token;
  return node;
}

Node *node_float_const(double double_value, Token *token) {
  Node *node = node_new();
  node->type = FLOAT_CONST;
  node->double_value = double_value;
  node->token = token;
  return node;
}

Node *node_string_literal(String *string_value, Token *token) {
  Node *node = node_new();
  node->type = STRING_LITERAL;
  node->string_value = string_value;
  node->token = token;
  return node;
}

Node *node_identifier(char *identifier, Token *token) {
  Node *node = node_new();
  node->type = IDENTIFIER;
  node->identifier = identifier;
  node->token = token;
  return node;
}

Node *node_func_call(Node *expr, Vector *args, Token *token) {
  Node *node = node_new();
  node->type = FUNC_CALL;
  node->expr = expr;
  node->args = args;
  node->token = token;
  return node;
}

Node *node_dot(Node *expr, char *member, Token *token) {
  Node *node = node_new();
  node->type = DOT;
  node->expr = expr;
  node->member = member;
  node->token = token;
  return node;
}

Node *node_sizeof(Node *expr, Token *token) {
  Node *node = node_new();
  node->type = SIZEOF;
  node->expr = expr;
  node->token = token;
  return node;
}

Node *node_unary_expr(NodeType type, Node *expr, Token *token) {
  Node *node = node_new();
  node->type = type;
  node->expr = expr;
  node->token = token;
  return node;
}

Node *node_cast(Type *value_type, Node *expr, Token *token) {
  Node *node = node_new();
  node->type = CAST;
  node->value_type = value_type;
  node->expr = expr;
  node->token = token;
  return node;
}

Node *node_binary_expr(NodeType type, Node *left, Node *right, Token *token) {
  Node *node = node_new();
  node->type = type;
  node->left = left;
  node->right = right;
  node->token = token;
  return node;
}

Node *node_condition(Node *control, Node *left, Node *right, Token *token) {
  Node *node = node_new();
  node->type = CONDITION;
  node->control = control;
  node->left = left;
  node->right = right;
  node->token = token;
  return node;
}

Node *node_assign(NodeType type, Node *left, Node *right, Token *token) {
  Node *node = node_new();
  node->type = type;
  node->left = left;
  node->right = right;
  node->token = token;
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

Node *node_if_stmt(Node *control, Node *if_body, Node *else_body) {
  Node *node = node_new();
  node->type = IF_STMT;
  node->control = control;
  node->if_body = if_body;
  node->else_body = else_body;
  return node;
}

Node *node_while_stmt(Node *control, Node *loop_body) {
  Node *node = node_new();
  node->type = WHILE_STMT;
  node->control = control;
  node->loop_body = loop_body;
  return node;
}

Node *node_do_while_stmt(Node *control, Node *loop_body) {
  Node *node = node_new();
  node->type = DO_WHILE_STMT;
  node->control = control;
  node->loop_body = loop_body;
  return node;
}

Node *node_for_stmt(Node *init, Node *control, Node *afterthrough, Node *loop_body) {
  Node *node = node_new();
  node->type = FOR_STMT;
  node->init = init;
  node->control = control;
  node->afterthrough = afterthrough;
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

Node *node_func_def(Symbol *symbol, Node *function_body, Token *token) {
  Node *node = node_new();
  node->type = FUNC_DEF;
  node->symbol = symbol;
  node->function_body = function_body;
  node->token = token;
  return node;
}

Node *node_trans_unit(Vector *definitions) {
  Node *node = node_new();
  node->type = TLANS_UNIT;
  node->definitions = definitions;
  return node;
}
