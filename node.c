#include "sk2cc.h"

bool check_lvalue(NodeType nd_type) {
  return nd_type == ND_IDENTIFIER || nd_type == ND_INDIRECT || nd_type == ND_DOT;
}

Expr *expr_new(NodeType nd_type, Type *type, Token *token) {
  Expr *node = calloc(1, sizeof(Expr));
  node->nd_type = nd_type;
  node->type = type;
  node->token = token;
  return node;
}

Expr *expr_identifier(char *identifier, Symbol *symbol, Token *token) {
  Type *type = symbol ? type_convert(symbol->type) : type_int();
  Expr *node = expr_new(ND_IDENTIFIER, type, token);
  node->identifier = identifier;
  node->symbol = symbol;
  return node;
}

Expr *expr_integer(int int_value, Token *token) {
  Expr *node = expr_new(ND_INTEGER, type_int(), token);
  node->int_value = int_value;
  return node;
}

Expr *expr_string(String *string_literal, int string_label, Token *token) {
  Type *type = type_convert(type_array(type_char(), string_literal->length));
  Expr *node = expr_new(ND_STRING, type, token);
  node->string_literal = string_literal;
  node->string_label = string_label;
  return node;
}

Expr *unary_expr(NodeType nd_type, Type *type, Expr *expr, Token *token) {
  Expr *node = expr_new(nd_type, type, token);
  node->expr = expr;
  return node;
}

Expr *expr_subscription(Expr *expr, Expr *index, Token *token) {
  return expr_indirect(expr_add(expr, index, token), token);
}

Expr *expr_call(Expr *expr, Vector *args, Token *token) {
  if (expr->nd_type != ND_IDENTIFIER) {
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
      Expr *arg = args->buffer[i];
      Symbol *param = params->buffer[i];
      if (!check_same(arg->type, param->type)) {
        error(token, "parameter types and argument types should be the same.");
      }
    }
  }

  Type *type;
  if (expr->symbol) {
    if (expr->type->ty_type != TY_FUNCTION) {
      error(token, "operand of function call should have function type.");
    }
    type = expr->type->returning;
  } else {
    type = type_int();
  }

  Expr *node = unary_expr(ND_CALL, type, expr, token);
  node->args = args;
  return node;
}

Expr *expr_dot(Expr *expr, char *member, Token *token) {
  if (expr->type->ty_type != TY_STRUCT) {
    error(token, "operand of . operator should have struct type.");
  }

  Member *struct_member = map_lookup(expr->type->members, member);
  if (!struct_member) {
    error(token, "undefined struct member: %s.", member);
  }

  Type *type = type_convert(struct_member->type);
  Expr *node = unary_expr(ND_DOT, type, expr, token);
  node->member = member;
  node->offset = struct_member->offset;
  return node;
}

Expr *expr_arrow(Expr *expr, char *member, Token *token) {
  return expr_dot(expr_indirect(expr, token), member, token);
}

Expr *expr_post_inc(Expr *expr, Token *token) {
  Symbol *sym_addr = symbol_variable(type_pointer(expr->type), NULL, token);
  symbol_put(NULL, sym_addr);

  Symbol *sym_val = symbol_variable(expr->type, NULL, token);
  symbol_put(NULL, sym_val);

  Expr *addr_id = expr_identifier(NULL, sym_addr, token);
  Expr *addr = expr_address(expr, token);
  Expr *addr_assign = expr_assign(addr_id, addr, token);
  Expr *val_id = expr_identifier(NULL, sym_val, token);
  Expr *val = expr_indirect(addr_id, token);
  Expr *val_assign = expr_assign(val_id, val, token);
  Expr *inc = expr_pre_inc(val, token);

  return expr_comma(expr_comma(expr_comma(addr_assign, val_assign, token), inc, token), val_id, token);
}

Expr *expr_post_dec(Expr *expr, Token *token) {
  Symbol *sym_addr = symbol_variable(type_pointer(expr->type), NULL, token);
  symbol_put(NULL, sym_addr);

  Symbol *sym_val = symbol_variable(expr->type, NULL, token);
  symbol_put(NULL, sym_val);

  Expr *addr_id = expr_identifier(NULL, sym_addr, token);
  Expr *addr = expr_address(expr, token);
  Expr *addr_assign = expr_assign(addr_id, addr, token);
  Expr *val_id = expr_identifier(NULL, sym_val, token);
  Expr *val = expr_indirect(addr_id, token);
  Expr *val_assign = expr_assign(val_id, val, token);
  Expr *dec = expr_pre_dec(val, token);

  return expr_comma(expr_comma(expr_comma(addr_assign, val_assign, token), dec, token), val_id, token);
}

Expr *expr_pre_inc(Expr *expr, Token *token) {
  return expr_add_assign(expr, expr_integer(1, token), token);
}

Expr *expr_pre_dec(Expr *expr, Token *token) {
  return expr_sub_assign(expr, expr_integer(1, token), token);
}

Expr *expr_address(Expr *expr, Token *token) {
  if (!check_lvalue(expr->nd_type)) {
    error(token, "operand of %s operator should be lvalue.", token_name(token->tk_type));
  }

  return unary_expr(ND_ADDRESS, type_pointer(expr->type), expr, token);
}

Expr *expr_indirect(Expr *expr, Token *token) {
  if (!check_pointer(expr->type)) {
    error(token, "operand of %s operator should have pointer type.", token_name(token->tk_type));
  }

  return unary_expr(ND_INDIRECT, type_convert(expr->type->pointer_to), expr, token);
}

Expr *expr_uplus(Expr *expr, Token *token) {
  if (!check_integer(expr->type)) {
    error(token, "operand of unary + operator should have integer type.");
  }

  return unary_expr(ND_UPLUS, type_int(), expr, token);
}

Expr *expr_uminus(Expr *expr, Token *token) {
  if (!check_integer(expr->type)) {
    error(token, "operand of unary - operator should have integer type.");
  }

  return unary_expr(ND_UMINUS, type_int(), expr, token);
}

Expr *expr_not(Expr *expr, Token *token) {
  if (!check_integer(expr->type)) {
    error(token, "operand of ~ operator should have integer type.");
  }

  return unary_expr(ND_NOT, type_int(), expr, token);
}

Expr *expr_lnot(Expr *expr, Token *token) {
  if (!check_scalar(expr->type)) {
    error(token, "operand of ! operator should have scalar type.");
  }

  return unary_expr(ND_LNOT, type_int(), expr, token);
}

Expr *expr_cast(Type *type, Expr *expr, Token *token) {
  return unary_expr(ND_CAST, type, expr, token);
}

Expr *binary_expr(NodeType nd_type, Type *type, Expr *lhs, Expr *rhs, Token *token) {
  Expr *node = expr_new(nd_type, type, token);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Expr *expr_mul(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_integer(lhs->type) && check_integer(rhs->type))) {
    error(token, "operands of %s operator should have integer type.", token_name(token->tk_type));
  }

  return binary_expr(ND_MUL, type_int(), lhs, rhs, token);
}

Expr *expr_div(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_integer(lhs->type) && check_integer(rhs->type))) {
    error(token, "operands of %s operator should have integer type.", token_name(token->tk_type));
  }

  return binary_expr(ND_DIV, type_int(), lhs, rhs, token);
}

Expr *expr_mod(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_integer(lhs->type) && check_integer(rhs->type))) {
    error(token, "operands of %s operator should have integer type.", token_name(token->tk_type));
  }

  return binary_expr(ND_MOD, type_int(), lhs, rhs, token);
}

Expr *expr_add(Expr *lhs, Expr *rhs, Token *token) {
  Type *type;
  if (check_integer(lhs->type) && check_integer(rhs->type)) {
    type = type_int();
  } else if (check_pointer(lhs->type) && check_integer(rhs->type)) {
    type = lhs->type;
  } else if (check_integer(lhs->type) && check_pointer(rhs->type)) {
    Expr *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
    type = lhs->type;
  } else {
    error(token, "invalid operand types for %s operator.", token_name(token->tk_type));
  }

  return binary_expr(ND_ADD, type, lhs, rhs, token);
}

Expr *expr_sub(Expr *lhs, Expr *rhs, Token *token) {
  Type *type;
  if (check_integer(lhs->type) && check_integer(rhs->type)) {
    type = type_int();
  } else if (check_pointer(lhs->type) && check_integer(rhs->type)) {
    type = lhs->type;
  } else {
    error(token, "invalid operand types for %s operator.", token_name(token->tk_type));
  }

  return binary_expr(ND_SUB, type, lhs, rhs, token);
}

Expr *expr_lshift(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_integer(lhs->type) && check_integer(rhs->type))) {
    error(token, "operands of << operator should be integer type.");
  }

  return binary_expr(ND_LSHIFT, type_int(), lhs, rhs, token);
}

Expr *expr_rshift(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_integer(lhs->type) && check_integer(rhs->type))) {
    error(token, "operands of >> operator should be integer type.");
  }

  return binary_expr(ND_RSHIFT, type_int(), lhs, rhs, token);
}

Expr *expr_lt(Expr *lhs, Expr *rhs, Token *token) {
  Type *type;
  if (check_integer(lhs->type) && check_integer(rhs->type)) {
    type = type_int();
  } else if (check_pointer(lhs->type) && check_pointer(rhs->type)) {
    type = type_int();
  } else {
    error(token, "operands of %s operator should be integer type.", token_name(token->tk_type));
  }

  return binary_expr(ND_LT, type, lhs, rhs, token);
}

Expr *expr_gt(Expr *lhs, Expr *rhs, Token *token) {
  return expr_lt(rhs, lhs, token);
}

Expr *expr_lte(Expr *lhs, Expr *rhs, Token *token) {
  Type *type;
  if (check_integer(lhs->type) && check_integer(rhs->type)) {
    type = type_int();
  } else if (check_pointer(lhs->type) && check_pointer(rhs->type)) {
    type = type_int();
  } else {
    error(token, "operands of %s operator should be integer type.", token_name(token->tk_type));
  }

  return binary_expr(ND_LTE, type, lhs, rhs, token);
}

Expr *expr_gte(Expr *lhs, Expr *rhs, Token *token) {
  return expr_lte(rhs, lhs, token);
}

Expr *expr_eq(Expr *lhs, Expr *rhs, Token *token) {
  Type *type;
  if (check_integer(lhs->type) && check_integer(rhs->type)) {
    type = type_int();
  } else if (check_pointer(lhs->type) && check_pointer(rhs->type)) {
    type = type_int();
  } else {
    error(token, "operands of == operator should be integer type.");
  }

  return binary_expr(ND_EQ, type, lhs, rhs, token);
}

Expr *expr_neq(Expr *lhs, Expr *rhs, Token *token) {
  Type *type;
  if (check_integer(lhs->type) && check_integer(rhs->type)) {
    type = type_int();
  } else if (check_pointer(lhs->type) && check_pointer(rhs->type)) {
    type = type_int();
  } else {
    error(token, "operands of != operator should be integer type.");
  }

  return binary_expr(ND_NEQ, type, lhs, rhs, token);
}

Expr *expr_and(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_integer(lhs->type) && check_integer(rhs->type))) {
    error(token, "operands of & operator should be integer type.");
  }

  return binary_expr(ND_AND, type_int(), lhs, rhs, token);
}

Expr *expr_xor(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_integer(lhs->type) && check_integer(rhs->type))) {
    error(token, "operands of ^ operator should be integer type.");
  }

  return binary_expr(ND_XOR, type_int(), lhs, rhs, token);
}

Expr *expr_or(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_integer(lhs->type) && check_integer(rhs->type))) {
    error(token, "operands of | operator should be integer type.");
  }

  return binary_expr(ND_OR, type_int(), lhs, rhs, token);
}

Expr *expr_land(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_scalar(lhs->type) && check_scalar(rhs->type))) {
    error(token, "operands of && operator should be integer type.");
  }

  return binary_expr(ND_LAND, type_int(), lhs, rhs, token);
}

Expr *expr_lor(Expr *lhs, Expr *rhs, Token *token) {
  if (!(check_scalar(lhs->type) && check_scalar(rhs->type))) {
    error(token, "operands of || operator should be integer type.");
  }

  return binary_expr(ND_LOR, type_int(), lhs, rhs, token);
}

Expr *expr_conditional(Expr *cond, Expr *lhs, Expr *rhs, Token *token) {
  Type *type;
  if (check_integer(lhs->type) && check_integer(rhs->type)) {
    type = type_int();
  } else if (check_pointer(lhs->type) && check_pointer(rhs->type)) {
    type = lhs->type;
  } else {
    error(token, "second and third operands of ?: operator should have the same type.");
  }

  Expr *node = binary_expr(ND_CONDITION, type, lhs, rhs, token);
  node->cond = cond;
  return node;
}

Expr *expr_assign(Expr *lhs, Expr *rhs, Token *token) {
  if (!check_lvalue(lhs->nd_type)) {
    error(token, "left hand side of %s operator should be lvalue.", token_name(token->tk_type));
  }

  return binary_expr(ND_ASSIGN, lhs->type, lhs, rhs, token);
}

Expr *expr_compound_assign(NodeType nd_type, Expr *lhs, Expr *rhs, Token *token) {
  Symbol *symbol = symbol_variable(type_pointer(lhs->type), NULL, token);
  symbol_put(NULL, symbol);

  Expr *id = expr_identifier(NULL, symbol, token);
  Expr *addr = expr_address(lhs, token);
  Expr *assign = expr_assign(id, addr, token);
  Expr *indirect = expr_indirect(id, token);

  Expr *op;
  if (nd_type == ND_ADD) op = expr_add(indirect, rhs, token);
  else if (nd_type == ND_SUB) op = expr_sub(indirect, rhs, token);
  else if (nd_type == ND_MUL) op = expr_mul(indirect, rhs, token);
  else if (nd_type == ND_DIV) op = expr_div(indirect, rhs, token);
  else if (nd_type == ND_MOD) op = expr_mod(indirect, rhs, token);

  return expr_comma(assign, expr_assign(indirect, op, token), token);
}

Expr *expr_mul_assign(Expr *lhs, Expr *rhs, Token *token) {
  return expr_compound_assign(ND_MUL, lhs, rhs, token);
}

Expr *expr_div_assign(Expr *lhs, Expr *rhs, Token *token) {
  return expr_compound_assign(ND_DIV, lhs, rhs, token);
}

Expr *expr_mod_assign(Expr *lhs, Expr *rhs, Token *token) {
  return expr_compound_assign(ND_MOD, lhs, rhs, token);
}

Expr *expr_add_assign(Expr *lhs, Expr *rhs, Token *token) {
  return expr_compound_assign(ND_ADD, lhs, rhs, token);
}

Expr *expr_sub_assign(Expr *lhs, Expr *rhs, Token *token) {
  return expr_compound_assign(ND_SUB, lhs, rhs, token);
}

Expr *expr_comma(Expr *lhs, Expr *rhs, Token *token) {
  return binary_expr(ND_COMMA, rhs->type, lhs, rhs, token);
}

Decl *decl_new(Vector *declarations) {
  Decl *node = calloc(1, sizeof(Decl));
  node->nd_type = ND_DECL;
  node->declarations = declarations;
  return node;
}

Init *init_expr(Expr *expr, Type *type) {
  Init *init = calloc(1, sizeof(Init));
  init->type = type;
  init->expr = expr;
  return init;
}

Init *init_list(Vector *list, Type *type) {
  Init *init = calloc(1, sizeof(Init));
  init->type = type;
  init->list = list;
  return init;
}

Stmt *stmt_new(NodeType nd_type, Token *token) {
  Stmt *node = calloc(1, sizeof(Stmt));
  node->nd_type = nd_type;
  node->token = token;
  return node;
}

Stmt *stmt_comp(Vector *block_items, Token *token) {
  Stmt *node = stmt_new(ND_COMP, token);
  node->block_items = block_items;
  return node;
}

Stmt *stmt_expr(Expr *expr, Token *token) {
  Stmt *node = stmt_new(ND_EXPR, token);
  node->expr = expr;
  return node;
}

Stmt *stmt_if(Expr *if_cond, Stmt *then_body, Stmt *else_body, Token *token) {
  Stmt *node = stmt_new(ND_IF, token);
  node->if_cond = if_cond;
  node->then_body = then_body;
  node->else_body = else_body;
  return node;
}

Stmt *stmt_while(Expr *while_cond, Stmt *while_body, Token *token) {
  Stmt *node = stmt_new(ND_WHILE, token);
  node->while_cond = while_cond;
  node->while_body = while_body;
  return node;
}

Stmt *stmt_do(Expr *do_cond, Stmt *do_body, Token *token) {
  Stmt *node = stmt_new(ND_DO, token);
  node->do_cond = do_cond;
  node->do_body = do_body;
  return node;
}

Stmt *stmt_for(Node *for_init, Expr *for_cond, Expr *for_after, Stmt *for_body, Token *token) {
  Stmt *node = stmt_new(ND_FOR, token);
  node->for_init = for_init;
  node->for_cond = for_cond;
  node->for_after = for_after;
  node->for_body = for_body;
  return node;
}

Stmt *stmt_continue(int continue_level, Token *token) {
  if (continue_level == 0) {
    error(token, "continue statement should appear in loops.");
  }

  return stmt_new(ND_CONTINUE, token);
}

Stmt *stmt_break(int break_level, Token *token) {
  if (break_level == 0) {
    error(token, "break statement should appear in loops.");
  }

  return stmt_new(ND_BREAK, token);
}

Stmt *stmt_return(Expr *ret_expr, Token *token) {
  Stmt *node = stmt_new(ND_RETURN, token);
  node->ret_expr = ret_expr;
  return node;
}

Func *func_new(Symbol *symbol, Stmt *body, int stack_size, Token *token) {
  if (symbol->type->returning->ty_type == TY_ARRAY) {
    error(symbol->token, "returning type of function should not be array type.");
  }

  if (symbol->type->params->length > 6) {
    error(token, "too many parameters.");
  }

  Func *func = calloc(1, sizeof(Func));
  func->nd_type = ND_FUNC;
  func->symbol = symbol;
  func->body = body;
  func->stack_size = stack_size;
  func->token = token;
  return func;
}

TransUnit *trans_unit_new(Vector *string_literals, Vector *external_decls) {
  TransUnit *trans_unit = calloc(1, sizeof(TransUnit));
  trans_unit->string_literals = string_literals;
  trans_unit->external_decls = external_decls;
  return trans_unit;
}
