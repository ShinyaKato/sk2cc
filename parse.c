#include "sk2cc.h"

int tokens_pos;
Vector *tokens;

Vector *string_literals;
Map *tags;
int continue_level, break_level;

Vector *scopes;
int local_vars_size;

Symbol *symbol_new() {
  Symbol *symbol = (Symbol *) calloc(1, sizeof(Symbol));
  return symbol;
}

Symbol *symbol_lookup(char *identifier) {
  for (int i = scopes->length - 1; i >= 0; i--) {
    Map *map = scopes->buffer[i];
    Symbol *symbol = map_lookup(map, identifier);
    if (symbol) return symbol;
  }
  return NULL;
}

void symbol_put(char *identifier, Symbol *symbol) {
  Map *map = scopes->buffer[scopes->length - 1];

  if (symbol->sy_type != TYPENAME && symbol->sy_type != ENUM_CONST) {
    if (identifier) {
      Symbol *previous = map_lookup(map, identifier);
      if (previous && previous->defined) {
        error(symbol->token, "duplicated function or variable definition of '%s'.", identifier);
      }
    }

    if (scopes->length == 1) {
      symbol->sy_type = GLOBAL;
    } else {
      int size = symbol->type->size;
      if (size % 8 != 0) size = size / 8 * 8 + 8;
      local_vars_size += size;

      symbol->sy_type = LOCAL;
      symbol->offset = local_vars_size;
    }
  }

  if (identifier) map_put(map, identifier, symbol);
}

void begin_scope() {
  vector_push(scopes, map_new());
}

void end_scope() {
  vector_pop(scopes);
}

void begin_function_scope(Symbol *symbol) {
  symbol_put(symbol->identifier, symbol);

  local_vars_size = symbol->type->ellipsis ? 176 : 0;
  begin_scope();

  Vector *params = symbol->type->params;
  for (int i = 0; i < params->length; i++) {
    Symbol *param = params->buffer[i];
    symbol_put(param->identifier, param);
  }
}

void begin_global_scope() {
  scopes = vector_new();
  begin_scope();
}

Token *peek_token() {
  return tokens->buffer[tokens_pos];
}

Token *get_token() {
  return tokens->buffer[tokens_pos++];
}

Token *expect_token(TokenType tk_type) {
  Token *token = get_token();
  if (token->tk_type != tk_type) {
    error(token, "%s is expected.", token->tk_name);
  }
  return token;
}

Token *optional_token(TokenType tk_type) {
  if (peek_token()->tk_type == tk_type) {
    return get_token();
  }
  return NULL;
}

bool read_token(TokenType tk_type) {
  if (peek_token()->tk_type == tk_type) {
    get_token();
    return true;
  }
  return false;
}

bool check_token(TokenType tk_type) {
  return peek_token()->tk_type == tk_type;
}

bool check_type_specifier() {
  if (check_token(tVOID)) return true;
  if (check_token(tBOOL)) return true;
  if (check_token(tCHAR)) return true;
  if (check_token(tSHORT)) return true;
  if (check_token(tINT)) return true;
  if (check_token(tDOUBLE)) return true;
  if (check_token(tUNSIGNED)) return true;
  if (check_token(tSTRUCT)) return true;
  if (check_token(tENUM)) return true;

  if (check_token(tIDENTIFIER)) {
    Symbol *symbol = symbol_lookup(peek_token()->identifier);
    return symbol && symbol->sy_type == TYPENAME;
  }

  return false;
}

bool check_declaration_specifier() {
  if (check_type_specifier()) return true;

  if (check_token(tTYPEDEF)) return true;
  if (check_token(tEXTERN)) return true;

  if (check_token(tNORETURN)) return true;

  return false;
}

void begin_loop() {
  continue_level++;
  break_level++;
}

void end_loop() {
  continue_level--;
  break_level--;
}

Node *assignment_expression();
Node *expression();
Type *type_name();

Node *primary_expression() {
  Token *token = get_token();

  if (token->tk_type == tINT_CONST) {
    return node_int_const(token->int_value, token);
  }
  if (token->tk_type == tFLOAT_CONST) {
    return node_float_const(token->double_value, token);
  }
  if (token->tk_type == tSTRING_LITERAL) {
    int string_label = string_literals->length;
    vector_push(string_literals, token->string_value);
    return node_string_literal(token->string_value, string_label, token);
  }
  if (token->tk_type == tIDENTIFIER) {
    Symbol *symbol = symbol_lookup(token->identifier);
    if (symbol && symbol->sy_type == ENUM_CONST) {
      return node_int_const(symbol->enum_value, token);
    }
    if (!check_token(tLPAREN) && !symbol) {
      error(token, "undefined identifier.");
    }
    return node_identifier(token->identifier, symbol, token);
  }
  if (token->tk_type == tLPAREN) {
    Node *node = expression();
    expect_token(tRPAREN);
    return node;
  }

  error(token, "invalid primary expression.");
}

Node *postfix_expression(Node *primary_expr) {
  Node *node = primary_expr;

  while (1) {
    Token *token = peek_token();
    if (read_token(tLPAREN)) {
      Vector *args = vector_new();
      if (!check_token(tRPAREN)) {
        do {
          vector_push(args, assignment_expression());
        } while (read_token(tCOMMA));
      }
      expect_token(tRPAREN);
      node = node_func_call(node, args, token);
    } else if (read_token(tLBRACKET)) {
      Node *index = expression();
      expect_token(tRBRACKET);
      Node *expr = node_add(node, index, token);
      node = node_indirect(expr, token);
    } else if (read_token(tDOT)) {
      char *member = expect_token(tIDENTIFIER)->identifier;
      node = node_dot(node, member, token);
    } else if (read_token(tARROW)) {
      char *member = expect_token(tIDENTIFIER)->identifier;
      Node *expr = node_indirect(node, token);
      node = node_dot(expr, member, token);
    } else if (read_token(tINC)) {
      node = node_post_inc(node, token);
    } else if (read_token(tDEC)) {
      node = node_post_dec(node, token);
    } else {
      break;
    }
  }

  return node;
}

Node *unary_expression() {
  Token *token = peek_token();

  if (read_token(tSIZEOF)) {
    Type *type;
    if (read_token(tLPAREN)) {
      if (check_type_specifier()) {
        type = type_name();
      } else {
        type = expression()->type;
      }
      expect_token(tRPAREN);
    } else {
      type = unary_expression()->type;
    }
    return node_int_const(type->original_size, token);
  }

  if (read_token(tALIGNOF)) {
    expect_token(tLPAREN);
    Type *type = type_name();
    expect_token(tRPAREN);
    return node_int_const(type->align, token);
  }

  if (read_token(tINC)) {
    return node_pre_inc(unary_expression(), token);
  }
  if (read_token(tDEC)) {
    return node_pre_dec(unary_expression(), token);
  }

  if (read_token(tAND)) {
    return node_address(unary_expression(), token);
  }
  if (read_token(tMUL)) {
    return node_indirect(unary_expression(), token);
  }

  if (read_token(tADD)) {
    return node_unary_arithmetic(UPLUS, unary_expression(), token);
  }
  if (read_token(tSUB)) {
    return node_unary_arithmetic(UMINUS, unary_expression(), token);
  }
  if (read_token(tNOT)) {
    return node_unary_arithmetic(NOT, unary_expression(), token);
  }
  if (read_token(tLNOT)) {
    return node_unary_arithmetic(LNOT, unary_expression(), token);
  }

  return postfix_expression(primary_expression());
}

Node *cast_expression() {
  if (!read_token(tLPAREN)) {
    return unary_expression();
  }
  if (!check_type_specifier()) {
    Node *expr = expression();
    expect_token(tRPAREN);
    return postfix_expression(expr);
  }

  Token *token = peek_token();
  Type *type = type_name();
  expect_token(tRPAREN);

  return node_cast(type, cast_expression(), token);
}

Node *multiplicative_expression(Node *cast_expr) {
  Node *node = cast_expr;

  while (1) {
    Token *token = peek_token();
    if (read_token(tMUL)) {
      node = node_mul(node, cast_expression(), token);
    } else if (read_token(tDIV)) {
      node = node_div(node, cast_expression(), token);
    } else if (read_token(tMOD)) {
      node = node_mod(node, cast_expression(), token);
    } else {
      break;
    }
  }

  return node;
}

Node *additive_expression(Node *cast_expr) {
  Node *node = multiplicative_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (read_token(tADD)) {
      node = node_add(node, multiplicative_expression(cast_expression()), token);
    } else if (read_token(tSUB)) {
      node = node_sub(node, multiplicative_expression(cast_expression()), token);
    } else {
      break;
    }
  }

  return node;
}

Node *shift_expression(Node *cast_expr) {
  Node *node = additive_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    NodeType nd_type;
    if (read_token(tLSHIFT)) nd_type = LSHIFT;
    else if (read_token(tRSHIFT)) nd_type = RSHIFT;
    else break;

    Node *right = additive_expression(cast_expression());
    node = node_shift(nd_type, node, right, token);
  }

  return node;
}

Node *relational_expression(Node *cast_expr) {
  Node *node = shift_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    NodeType nd_type;
    if (read_token(tLT)) nd_type = LT;
    else if (read_token(tGT)) nd_type = GT;
    else if (read_token(tLTE)) nd_type = LTE;
    else if (read_token(tGTE)) nd_type = GTE;
    else break;

    Node *right = shift_expression(cast_expression());
    node = node_relational(nd_type, node, right, token);
  }

  return node;
}

Node *equality_expression(Node *cast_expr) {
  Node *node = relational_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    NodeType nd_type;
    if (read_token(tEQ)) nd_type = EQ;
    else if (read_token(tNEQ)) nd_type = NEQ;
    else break;

    Node *right = relational_expression(cast_expression());
    node = node_equality(nd_type, node, right, token);
  }

  return node;
}

Node *and_expression(Node *cast_expr) {
  Node *node = equality_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tAND)) break;

    Node *right = equality_expression(cast_expression());
    node = node_bitwise(AND, node, right, token);
  }

  return node;
}

Node *exclusive_or_expression(Node *cast_expr) {
  Node *node = and_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tXOR)) break;

    Node *right = and_expression(cast_expression());
    node = node_bitwise(XOR, node, right, token);
  }

  return node;
}

Node *inclusive_or_expression(Node *cast_expr) {
  Node *node = exclusive_or_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tOR)) break;

    Node *right = exclusive_or_expression(cast_expression());
    node = node_bitwise(OR, node, right, token);
  }

  return node;
}

Node *logical_and_expression(Node *cast_expr) {
  Node *node = inclusive_or_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tLAND)) break;

    Node *right = inclusive_or_expression(cast_expression());
    node = node_logical(LAND, node, right, token);
  }

  return node;
}

Node *logical_or_expression(Node *cast_expr) {
  Node *node = logical_and_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tLOR)) break;

    Node *right = logical_and_expression(cast_expression());
    node = node_logical(LOR, node, right, token);
  }

  return node;
}

Node *conditional_expression(Node *cast_expr) {
  Node *control = logical_or_expression(cast_expr);
  Token *token = peek_token();
  if (!read_token(tQUESTION)) return control;

  Node *left = expression();
  expect_token(tCOLON);
  Node *right = conditional_expression(cast_expression());

  return node_conditional(control, left, right, token);
}

Node *assignment_expression() {
  Node *left = cast_expression();

  Token *token = peek_token();
  if (read_token(tASSIGN)) {
    return node_assign(left, assignment_expression(), token);
  } else if (read_token(tADD_ASSIGN)) {
    return node_add_assign(left, assignment_expression(), token);
  } else if (read_token(tSUB_ASSIGN)) {
    return node_sub_assign(left, assignment_expression(), token);
  } else if (read_token(tMUL_ASSIGN)) {
    return node_mul_assign(left, assignment_expression(), token);
  } else if (read_token(tDIV_ASSIGN)) {
    return node_div_assign(left, assignment_expression(), token);
  } else if (read_token(tMOD_ASSIGN)) {
    return node_mod_assign(left, assignment_expression(), token);
  }

  return conditional_expression(left);
}

Node *expression() {
  Node *node = assignment_expression();

  while (1) {
    Token *token = peek_token();
    if (!read_token(tCOMMA)) break;

    Node *right = assignment_expression();
    node = node_comma(node, right, token);
  }

  return node;
}

Type *type_specifier();
Symbol *declarator(Type *type);

Type *abstract_declarator(Type *specifier) {
  Type *type = specifier;
  while (read_token(tMUL)) {
    type = type_pointer_to(type);
  }
  return type;
}

Type *type_name() {
  Type *specifier = type_specifier();
  return abstract_declarator(specifier);
}

Type *struct_or_union_specifier() {
  expect_token(tSTRUCT);

  Token *token = optional_token(tIDENTIFIER);
  if (token && !map_lookup(tags, token->identifier)) {
    Type *incomplete_type = type_new();
    incomplete_type->ty_type = STRUCT;
    incomplete_type->incomplete = true;
    map_put(tags, token->identifier, incomplete_type);
  }

  if (!read_token(tLBRACE)) {
    if (!token) error(peek_token(), "invalid struct type specifier.");
    return map_lookup(tags, token->identifier);
  }

  Vector *symbols = vector_new();
  do {
    Type *specifier = type_specifier();
    do {
      Symbol *symbol = declarator(specifier);
      if (symbol->type->incomplete) {
        error(symbol->token, "declaration with incomplete type.");
      }
      vector_push(symbols, symbol);
    } while (read_token(tCOMMA));
    expect_token(tSEMICOLON);
  } while (!check_token(tRBRACE) && !check_token(tEND));
  expect_token(tRBRACE);

  Type *type = type_struct(symbols);
  if (!token) return type;

  Type *dest = map_lookup(tags, token->identifier);
  type_copy(dest, type);
  return dest;
}

Type *enum_specifier() {
  expect_token(tENUM);

  Token *token = optional_token(tIDENTIFIER);
  if (token && !map_lookup(tags, token->identifier)) {
    Type *incomplete_type = type_new();
    incomplete_type->incomplete = true;
    map_put(tags, token->identifier, incomplete_type);
  }

  if (!read_token(tLBRACE)) {
    if (!token) error(peek_token(), "invalid enum type spcifier.");
    return map_lookup(tags, token->identifier);
  }

  int enum_value = 0;
  do {
    Token *token = expect_token(tIDENTIFIER);
    Symbol *symbol = symbol_new();
    symbol->sy_type = ENUM_CONST;
    symbol->identifier = token->identifier;
    symbol->token = token;
    symbol->type = type_int();
    symbol->enum_value = enum_value++;
    symbol_put(token->identifier, symbol);
  } while (read_token(tCOMMA));
  expect_token(tRBRACE);

  Type *type = type_int();
  if (!token) return type;

  Type *dest = map_lookup(tags, token->identifier);
  type_copy(dest, type);
  return dest;
}

Type *type_specifier() {
  if (read_token(tVOID)) {
    return type_void();
  } else if (read_token(tBOOL)) {
    return type_bool();
  } else if (read_token(tSHORT)) {
    return type_short();
  } else if (read_token(tCHAR)) {
    return type_char();
  } else if (read_token(tINT)) {
    return type_int();
  } else if (read_token(tDOUBLE)) {
    return type_double();
  } else if (read_token(tUNSIGNED)) {
    if (read_token(tCHAR)) {
      return type_uchar();
    } else if (read_token(tSHORT)) {
      return type_ushort();
    } else if (read_token(tINT)) {
      return type_uint();
    } else {
      return type_uint();
    }
  } else if (check_token(tSTRUCT)) {
    return struct_or_union_specifier();
  } else if (check_token(tENUM)) {
    return enum_specifier();
  }

  Token *token = expect_token(tIDENTIFIER);
  Symbol *symbol = symbol_lookup(token->identifier);
  if (symbol && symbol->sy_type == TYPENAME) {
    return symbol->type;
  }
  error(token, "type specifier is expected.");
}

Type *declaration_specifiers() {
  bool definition = read_token(tTYPEDEF);
  bool external = read_token(tEXTERN);
  read_token(tNORETURN);

  Type *specifier = type_specifier();
  specifier->definition = definition;
  specifier->external = external;

  return specifier;
}

Type *direct_declarator(Type *type) {
  if (read_token(tLBRACKET)) {
    if (read_token(tRBRACKET)) {
      return type_incomplete_array_of(direct_declarator(type));
    }
    int size = expect_token(tINT_CONST)->int_value;
    expect_token(tRBRACKET);
    return type_array_of(direct_declarator(type), size);
  }

  if (read_token(tLPAREN)) {
    Vector *params = vector_new();
    bool ellipsis = false;
    if (!check_token(tRPAREN)) {
      do {
        if (read_token(tELLIPSIS)) {
          ellipsis = true;
          break;
        }
        Type *specifier = declaration_specifiers();
        Symbol *param = declarator(specifier);
        if (param->type->ty_type == ARRAY) {
          param->type = type_pointer_to(param->type->array_of);
        }
        vector_push(params, param);
      } while (read_token(tCOMMA));
    }
    expect_token(tRPAREN);
    return type_function_returning(direct_declarator(type), params, ellipsis);
  }

  return type;
}

Symbol *declarator(Type *specifier) {
  Type *type = specifier;
  while (read_token(tMUL)) {
    type = type_pointer_to(type);
  }
  Token *token = expect_token(tIDENTIFIER);
  type = direct_declarator(type);

  Symbol *symbol = symbol_new();
  symbol->token = token;
  symbol->identifier = token->identifier;
  symbol->type = type;
  symbol->defined = type->ty_type != FUNCTION;

  return symbol;
}

Node *initializer(Type *type) {
  if (type->ty_type == ARRAY) {
    Vector *array_init = vector_new();
    expect_token(tLBRACE);
    do {
      vector_push(array_init, initializer(type->array_of));
    } while (read_token(tCOMMA));
    expect_token(tRBRACE);
    return node_array_init(array_init, type);
  }

  return node_init(assignment_expression(), type);
}

Symbol *init_declarator(Type *specifier, Symbol *symbol) {
  if (read_token(tASSIGN)) {
    symbol->initializer = initializer(symbol->type);
    if (symbol->type->ty_type == ARRAY) {
      int length = symbol->initializer->array_init->length;
      if (symbol->type->incomplete) {
        Type *type = type_array_of(symbol->type->array_of, length);
        type_copy(symbol->type, type);
      } else {
        if (symbol->type->array_size < length) {
          error(symbol->token, "too many array elements.");
        }
      }
    }
  }

  return symbol;
}

Vector *declaration(Type *specifier, Symbol *first_symbol) {
  Vector *symbols = vector_new();
  if (first_symbol) {
    Symbol *symbol = init_declarator(specifier, first_symbol);
    if (!specifier->definition && symbol->type->incomplete) {
      error(symbol->token, "declaration with incomplete type.");
    }
    if (specifier->definition) {
      symbol->sy_type = TYPENAME;
      symbol_put(symbol->identifier, symbol);
    } else {
      symbol_put(symbol->identifier, symbol);
      if (!specifier->external && symbol->type->ty_type != FUNCTION) {
        vector_push(symbols, symbol);
      }
    }
  } else if (!check_token(tSEMICOLON)) {
    Symbol *symbol = init_declarator(specifier, declarator(specifier));
    if (!specifier->definition && symbol->type->incomplete) {
      error(symbol->token, "declaration with incomplete type.");
    }
    if (specifier->definition) {
      symbol->sy_type = TYPENAME;
      symbol_put(symbol->identifier, symbol);
    } else {
      symbol_put(symbol->identifier, symbol);
      if (!specifier->external && symbol->type->ty_type != FUNCTION) {
        vector_push(symbols, symbol);
      }
    }
  }
  while (read_token(tCOMMA)) {
    Symbol *symbol = init_declarator(specifier, declarator(specifier));
    if (!specifier->definition && symbol->type->incomplete) {
      error(symbol->token, "declaration with incomplete type.");
    }
    if (specifier->definition) {
      symbol->sy_type = TYPENAME;
      symbol_put(symbol->identifier, symbol);
    } else {
      symbol_put(symbol->identifier, symbol);
      if (!specifier->external && symbol->type->ty_type != FUNCTION) {
        vector_push(symbols, symbol);
      }
    }
  }
  expect_token(tSEMICOLON);

  return symbols;
}

Node *statement();

Node *compound_statement() {
  Vector *statements = vector_new();
  expect_token(tLBRACE);
  while (!check_token(tRBRACE) && !check_token(tEND)) {
    if (check_declaration_specifier()) {
      Vector *declarations = declaration(declaration_specifiers(), NULL);
      vector_push(statements, node_decl(declarations));
    } else {
      vector_push(statements, statement());
    }
  }
  expect_token(tRBRACE);

  return node_comp_stmt(statements);
}

Node *expression_statement() {
  Node *expr = !check_token(tSEMICOLON) ? expression() : NULL;
  expect_token(tSEMICOLON);

  return node_expr_stmt(expr);
}

Node *if_statement() {
  expect_token(tIF);
  expect_token(tLPAREN);
  Node *if_control = expression();
  expect_token(tRPAREN);
  Node *if_body = statement();
  Node *else_body = read_token(tELSE) ? statement() : NULL;

  return node_if_stmt(if_control, if_body, else_body);
}

Node *while_statement() {
  begin_loop();
  expect_token(tWHILE);
  expect_token(tLPAREN);
  Node *loop_control = expression();
  expect_token(tRPAREN);
  Node *loop_body = statement();
  end_loop();

  return node_while_stmt(loop_control, loop_body);
}

Node *do_while_statement() {
  begin_loop();
  expect_token(tDO);
  Node *loop_body = statement();
  expect_token(tWHILE);
  expect_token(tLPAREN);
  Node *loop_control = expression();
  expect_token(tRPAREN);
  expect_token(tSEMICOLON);
  end_loop();

  return node_do_while_stmt(loop_control, loop_body);
}

Node *for_statement() {
  begin_scope();
  begin_loop();
  expect_token(tFOR);
  expect_token(tLPAREN);
  Node *loop_init;
  if (check_declaration_specifier()) {
    Vector *declarations = declaration(declaration_specifiers(), NULL);
    loop_init = node_decl(declarations);
  } else {
    loop_init = node_expr_stmt(!check_token(tSEMICOLON) ? expression() : NULL);
    expect_token(tSEMICOLON);
  }
  Node *loop_control = !check_token(tSEMICOLON) ? expression() : NULL;
  expect_token(tSEMICOLON);
  Node *loop_afterthrough = node_expr_stmt(!check_token(tRPAREN) ? expression() : NULL);
  expect_token(tRPAREN);
  Node *loop_body = statement();
  end_loop();
  end_scope();

  return node_for_stmt(loop_init, loop_control, loop_afterthrough, loop_body);
}

Node *continue_statement() {
  Token *token = expect_token(tCONTINUE);
  expect_token(tSEMICOLON);

  return node_continue_stmt(continue_level, token);
}

Node *break_statement() {
  Token *token = expect_token(tBREAK);
  expect_token(tSEMICOLON);

  return node_break_stmt(break_level, token);
}

Node *return_statement() {
  expect_token(tRETURN);
  Node *expr = !check_token(tSEMICOLON) ? expression() : NULL;
  expect_token(tSEMICOLON);

  return node_return_stmt(expr);
}

Node *statement() {
  TokenType type = peek_token()->tk_type;
  if (type == tLBRACE) {
    begin_scope();
    Node *node = compound_statement();
    end_scope();
    return node;
  }
  if (type == tIF) return if_statement();
  if (type == tWHILE) return while_statement();
  if (type == tDO) return do_while_statement();
  if (type == tFOR) return for_statement();
  if (type == tCONTINUE) return continue_statement();
  if (type == tBREAK) return break_statement();
  if (type == tRETURN) return return_statement();
  return expression_statement();
}

Node *function_definition(Symbol *symbol) {
  symbol->defined = true;
  begin_function_scope(symbol);
  Node *function_body = compound_statement();
  end_scope();

  return node_func_def(symbol, function_body, local_vars_size, symbol->token);
}

Node *translate_unit() {
  begin_global_scope();
  Vector *declarations = vector_new();
  Vector *definitions = vector_new();
  while (!check_token(tEND)) {
    Type *specifier = declaration_specifiers();
    if (check_token(tSEMICOLON)) {
      declaration(specifier, NULL);
    } else {
      Symbol *first_symbol = declarator(specifier);
      if (!check_token(tLBRACE)) {
        Vector *symbols = declaration(specifier, first_symbol);
        for (int i = 0; i < symbols->length; i++) {
          Symbol *symbol = symbols->buffer[i];
          vector_push(declarations, symbol);
        }
      } else {
        Node *node = function_definition(first_symbol);
        vector_push(definitions, node);
      }
    }
  }
  end_scope();

  return node_trans_unit(string_literals, declarations, definitions);
}

Node *parse(Vector *input_tokens) {
  tokens = vector_new();
  tokens_pos = 0;

  for (int i = 0; i < input_tokens->length; i++) {
    Token *token = input_tokens->buffer[i];
    if (token->tk_type == tSPACE || token->tk_type == tNEWLINE) continue;
    vector_push(tokens, token);
  }

  string_literals = vector_new();

  tags = map_new();

  continue_level = 0;
  break_level = 0;

  return translate_unit();
}
