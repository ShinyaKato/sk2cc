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

Token *expect_token(int tk_type) {
  Token *token = get_token();
  if (token->tk_type != tk_type) {
    error(token, "%s is expected.", token_name(tk_type));
  }
  return token;
}

Token *optional_token(int tk_type) {
  if (peek_token()->tk_type == tk_type) {
    return get_token();
  }
  return NULL;
}

bool read_token(int tk_type) {
  if (peek_token()->tk_type == tk_type) {
    get_token();
    return true;
  }
  return false;
}

bool check_token(int tk_type) {
  return peek_token()->tk_type == tk_type;
}

bool check_type_specifier() {
  if (check_token(TK_VOID)) return true;
  if (check_token(TK_BOOL)) return true;
  if (check_token(TK_CHAR)) return true;
  if (check_token(TK_SHORT)) return true;
  if (check_token(TK_INT)) return true;
  if (check_token(TK_UNSIGNED)) return true;
  if (check_token(TK_STRUCT)) return true;
  if (check_token(TK_ENUM)) return true;

  if (check_token(TK_IDENTIFIER)) {
    Symbol *symbol = symbol_lookup(peek_token()->identifier);
    return symbol && symbol->sy_type == TYPENAME;
  }

  return false;
}

bool check_declaration_specifier() {
  if (check_type_specifier()) return true;

  if (check_token(TK_TYPEDEF)) return true;
  if (check_token(TK_EXTERN)) return true;

  if (check_token(TK_NORETURN)) return true;

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

  if (token->tk_type == TK_INTEGER_CONST) {
    return node_int_const(token->int_value, token);
  }
  if (token->tk_type == TK_STRING_LITERAL) {
    int string_label = string_literals->length;
    vector_push(string_literals, token->string_value);
    return node_string_literal(token->string_value, string_label, token);
  }
  if (token->tk_type == TK_IDENTIFIER) {
    Symbol *symbol = symbol_lookup(token->identifier);
    if (symbol && symbol->sy_type == ENUM_CONST) {
      return node_int_const(symbol->enum_value, token);
    }
    if (!check_token('(') && !symbol) {
      error(token, "undefined identifier.");
    }
    return node_identifier(token->identifier, symbol, token);
  }
  if (token->tk_type == '(') {
    Node *node = expression();
    expect_token(')');
    return node;
  }

  error(token, "invalid primary expression.");
}

Node *postfix_expression(Node *primary_expr) {
  Node *node = primary_expr;

  while (1) {
    Token *token = peek_token();
    if (read_token('(')) {
      Vector *args = vector_new();
      if (!check_token(')')) {
        do {
          vector_push(args, assignment_expression());
        } while (read_token(','));
      }
      expect_token(')');
      node = node_func_call(node, args, token);
    } else if (read_token('[')) {
      Node *index = expression();
      expect_token(']');
      Node *expr = node_add(node, index, token);
      node = node_indirect(expr, token);
    } else if (read_token('.')) {
      char *member = expect_token(TK_IDENTIFIER)->identifier;
      node = node_dot(node, member, token);
    } else if (read_token(TK_ARROW)) {
      char *member = expect_token(TK_IDENTIFIER)->identifier;
      Node *expr = node_indirect(node, token);
      node = node_dot(expr, member, token);
    } else if (read_token(TK_INC)) {
      node = node_post_inc(node, token);
    } else if (read_token(TK_DEC)) {
      node = node_post_dec(node, token);
    } else {
      break;
    }
  }

  return node;
}

Node *unary_expression() {
  Token *token = peek_token();

  if (read_token(TK_SIZEOF)) {
    Type *type;
    if (read_token('(')) {
      if (check_type_specifier()) {
        type = type_name();
      } else {
        type = expression()->type;
      }
      expect_token(')');
    } else {
      type = unary_expression()->type;
    }
    return node_int_const(type->original_size, token);
  }

  if (read_token(TK_ALIGNOF)) {
    expect_token('(');
    Type *type = type_name();
    expect_token(')');
    return node_int_const(type->align, token);
  }

  if (read_token(TK_INC)) {
    return node_pre_inc(unary_expression(), token);
  }
  if (read_token(TK_DEC)) {
    return node_pre_dec(unary_expression(), token);
  }

  if (read_token('&')) {
    return node_address(unary_expression(), token);
  }
  if (read_token('*')) {
    return node_indirect(unary_expression(), token);
  }

  if (read_token('+')) {
    return node_unary_arithmetic(UPLUS, unary_expression(), token);
  }
  if (read_token('-')) {
    return node_unary_arithmetic(UMINUS, unary_expression(), token);
  }
  if (read_token('~')) {
    return node_unary_arithmetic(NOT, unary_expression(), token);
  }
  if (read_token('!')) {
    return node_unary_arithmetic(LNOT, unary_expression(), token);
  }

  return postfix_expression(primary_expression());
}

Node *cast_expression() {
  if (!read_token('(')) {
    return unary_expression();
  }
  if (!check_type_specifier()) {
    Node *expr = expression();
    expect_token(')');
    return postfix_expression(expr);
  }

  Token *token = peek_token();
  Type *type = type_name();
  expect_token(')');

  return node_cast(type, cast_expression(), token);
}

Node *multiplicative_expression(Node *cast_expr) {
  Node *node = cast_expr;

  while (1) {
    Token *token = peek_token();
    if (read_token('*')) {
      node = node_mul(node, cast_expression(), token);
    } else if (read_token('/')) {
      node = node_div(node, cast_expression(), token);
    } else if (read_token('%')) {
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
    if (read_token('+')) {
      node = node_add(node, multiplicative_expression(cast_expression()), token);
    } else if (read_token('-')) {
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
    if (read_token(TK_LSHIFT)) nd_type = LSHIFT;
    else if (read_token(TK_RSHIFT)) nd_type = RSHIFT;
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
    if (read_token('<')) nd_type = LT;
    else if (read_token('>')) nd_type = GT;
    else if (read_token(TK_LTE)) nd_type = LTE;
    else if (read_token(TK_GTE)) nd_type = GTE;
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
    if (read_token(TK_EQ)) nd_type = EQ;
    else if (read_token(TK_NEQ)) nd_type = NEQ;
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
    if (!read_token('&')) break;

    Node *right = equality_expression(cast_expression());
    node = node_bitwise(AND, node, right, token);
  }

  return node;
}

Node *exclusive_or_expression(Node *cast_expr) {
  Node *node = and_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token('^')) break;

    Node *right = and_expression(cast_expression());
    node = node_bitwise(XOR, node, right, token);
  }

  return node;
}

Node *inclusive_or_expression(Node *cast_expr) {
  Node *node = exclusive_or_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token('|')) break;

    Node *right = exclusive_or_expression(cast_expression());
    node = node_bitwise(OR, node, right, token);
  }

  return node;
}

Node *logical_and_expression(Node *cast_expr) {
  Node *node = inclusive_or_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(TK_AND)) break;

    Node *right = inclusive_or_expression(cast_expression());
    node = node_logical(LAND, node, right, token);
  }

  return node;
}

Node *logical_or_expression(Node *cast_expr) {
  Node *node = logical_and_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(TK_OR)) break;

    Node *right = logical_and_expression(cast_expression());
    node = node_logical(LOR, node, right, token);
  }

  return node;
}

Node *conditional_expression(Node *cast_expr) {
  Node *control = logical_or_expression(cast_expr);
  Token *token = peek_token();
  if (!read_token('?')) return control;

  Node *left = expression();
  expect_token(':');
  Node *right = conditional_expression(cast_expression());

  return node_conditional(control, left, right, token);
}

Node *assignment_expression() {
  Node *left = cast_expression();

  Token *token = peek_token();
  if (read_token('=')) {
    return node_assign(left, assignment_expression(), token);
  } else if (read_token(TK_ADD_ASSIGN)) {
    return node_add_assign(left, assignment_expression(), token);
  } else if (read_token(TK_SUB_ASSIGN)) {
    return node_sub_assign(left, assignment_expression(), token);
  } else if (read_token(TK_MUL_ASSIGN)) {
    return node_mul_assign(left, assignment_expression(), token);
  } else if (read_token(TK_DIV_ASSIGN)) {
    return node_div_assign(left, assignment_expression(), token);
  } else if (read_token(TK_MOD_ASSIGN)) {
    return node_mod_assign(left, assignment_expression(), token);
  }

  return conditional_expression(left);
}

Node *expression() {
  Node *node = assignment_expression();

  while (1) {
    Token *token = peek_token();
    if (!read_token(',')) break;

    Node *right = assignment_expression();
    node = node_comma(node, right, token);
  }

  return node;
}

Type *type_specifier();
Symbol *declarator(Type *type);

Type *abstract_declarator(Type *specifier) {
  Type *type = specifier;
  while (read_token('*')) {
    type = type_pointer_to(type);
  }
  return type;
}

Type *type_name() {
  Type *specifier = type_specifier();
  return abstract_declarator(specifier);
}

Type *struct_or_union_specifier() {
  expect_token(TK_STRUCT);

  Token *token = optional_token(TK_IDENTIFIER);
  if (token && !map_lookup(tags, token->identifier)) {
    Type *incomplete_type = type_new();
    incomplete_type->ty_type = STRUCT;
    incomplete_type->incomplete = true;
    map_put(tags, token->identifier, incomplete_type);
  }

  if (!read_token('{')) {
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
    } while (read_token(','));
    expect_token(';');
  } while (!check_token('}') && !check_token(TK_EOF));
  expect_token('}');

  Type *type = type_struct(symbols);
  if (!token) return type;

  Type *dest = map_lookup(tags, token->identifier);
  type_copy(dest, type);
  return dest;
}

Type *enum_specifier() {
  expect_token(TK_ENUM);

  Token *token = optional_token(TK_IDENTIFIER);
  if (token && !map_lookup(tags, token->identifier)) {
    Type *incomplete_type = type_new();
    incomplete_type->incomplete = true;
    map_put(tags, token->identifier, incomplete_type);
  }

  if (!read_token('{')) {
    if (!token) error(peek_token(), "invalid enum type spcifier.");
    return map_lookup(tags, token->identifier);
  }

  int enum_value = 0;
  do {
    Token *token = expect_token(TK_IDENTIFIER);
    if (read_token('=')) {
      enum_value = expect_token(TK_INTEGER_CONST)->int_value;
    }

    Symbol *symbol = symbol_new();
    symbol->sy_type = ENUM_CONST;
    symbol->identifier = token->identifier;
    symbol->token = token;
    symbol->type = type_int();
    symbol->enum_value = enum_value++;
    symbol_put(token->identifier, symbol);
  } while (read_token(','));
  expect_token('}');

  Type *type = type_int();
  if (!token) return type;

  Type *dest = map_lookup(tags, token->identifier);
  type_copy(dest, type);
  return dest;
}

Type *type_specifier() {
  if (read_token(TK_VOID)) {
    return type_void();
  } else if (read_token(TK_SHORT)) {
    return type_short();
  } else if (read_token(TK_CHAR)) {
    return type_char();
  } else if (read_token(TK_INT)) {
    return type_int();
  } else if (read_token(TK_UNSIGNED)) {
    if (read_token(TK_CHAR)) {
      return type_uchar();
    } else if (read_token(TK_SHORT)) {
      return type_ushort();
    } else if (read_token(TK_INT)) {
      return type_uint();
    } else {
      return type_uint();
    }
  } else if (read_token(TK_BOOL)) {
    return type_bool();
  } else if (check_token(TK_STRUCT)) {
    return struct_or_union_specifier();
  } else if (check_token(TK_ENUM)) {
    return enum_specifier();
  }

  Token *token = expect_token(TK_IDENTIFIER);
  Symbol *symbol = symbol_lookup(token->identifier);
  if (symbol && symbol->sy_type == TYPENAME) {
    return symbol->type;
  }
  error(token, "type specifier is expected.");
}

Type *declaration_specifiers() {
  bool definition = read_token(TK_TYPEDEF);
  bool external = read_token(TK_EXTERN);
  read_token(TK_NORETURN);

  Type *specifier = type_specifier();
  specifier->definition = definition;
  specifier->external = external;

  return specifier;
}

Type *direct_declarator(Type *type) {
  if (read_token('[')) {
    if (read_token(']')) {
      return type_incomplete_array_of(direct_declarator(type));
    }
    int size = expect_token(TK_INTEGER_CONST)->int_value;
    expect_token(']');
    return type_array_of(direct_declarator(type), size);
  }

  if (read_token('(')) {
    Vector *params = vector_new();
    bool ellipsis = false;
    if (!check_token(')')) {
      do {
        if (read_token(TK_ELLIPSIS)) {
          ellipsis = true;
          break;
        }
        Type *specifier = declaration_specifiers();
        Symbol *param = declarator(specifier);
        if (param->type->ty_type == ARRAY) {
          param->type = type_pointer_to(param->type->array_of);
        }
        vector_push(params, param);
      } while (read_token(','));
    }
    expect_token(')');
    return type_function_returning(direct_declarator(type), params, ellipsis);
  }

  return type;
}

Symbol *declarator(Type *specifier) {
  Type *type = specifier;
  while (read_token('*')) {
    type = type_pointer_to(type);
  }
  Token *token = expect_token(TK_IDENTIFIER);
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
    expect_token('{');
    do {
      vector_push(array_init, initializer(type->array_of));
    } while (read_token(','));
    expect_token('}');
    return node_array_init(array_init, type);
  }

  return node_init(assignment_expression(), type);
}

Symbol *init_declarator(Type *specifier, Symbol *symbol) {
  if (read_token('=')) {
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
  } else if (!check_token(';')) {
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
  while (read_token(',')) {
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
  expect_token(';');

  return symbols;
}

Node *statement();

Node *compound_statement() {
  Vector *statements = vector_new();
  expect_token('{');
  while (!check_token('}') && !check_token(TK_EOF)) {
    if (check_declaration_specifier()) {
      Vector *declarations = declaration(declaration_specifiers(), NULL);
      vector_push(statements, node_decl(declarations));
    } else {
      vector_push(statements, statement());
    }
  }
  expect_token('}');

  return node_comp_stmt(statements);
}

Node *expression_statement() {
  Node *expr = !check_token(';') ? expression() : NULL;
  expect_token(';');

  return node_expr_stmt(expr);
}

Node *if_statement() {
  expect_token(TK_IF);
  expect_token('(');
  Node *if_control = expression();
  expect_token(')');
  Node *if_body = statement();
  Node *else_body = read_token(TK_ELSE) ? statement() : NULL;

  return node_if_stmt(if_control, if_body, else_body);
}

Node *while_statement() {
  begin_loop();
  expect_token(TK_WHILE);
  expect_token('(');
  Node *loop_control = expression();
  expect_token(')');
  Node *loop_body = statement();
  end_loop();

  return node_while_stmt(loop_control, loop_body);
}

Node *do_while_statement() {
  begin_loop();
  expect_token(TK_DO);
  Node *loop_body = statement();
  expect_token(TK_WHILE);
  expect_token('(');
  Node *loop_control = expression();
  expect_token(')');
  expect_token(';');
  end_loop();

  return node_do_while_stmt(loop_control, loop_body);
}

Node *for_statement() {
  begin_scope();
  begin_loop();
  expect_token(TK_FOR);
  expect_token('(');
  Node *loop_init;
  if (check_declaration_specifier()) {
    Vector *declarations = declaration(declaration_specifiers(), NULL);
    loop_init = node_decl(declarations);
  } else {
    loop_init = node_expr_stmt(!check_token(';') ? expression() : NULL);
    expect_token(';');
  }
  Node *loop_control = !check_token(';') ? expression() : NULL;
  expect_token(';');
  Node *loop_afterthrough = node_expr_stmt(!check_token(')') ? expression() : NULL);
  expect_token(')');
  Node *loop_body = statement();
  end_loop();
  end_scope();

  return node_for_stmt(loop_init, loop_control, loop_afterthrough, loop_body);
}

Node *continue_statement() {
  Token *token = expect_token(TK_CONTINUE);
  expect_token(';');

  return node_continue_stmt(continue_level, token);
}

Node *break_statement() {
  Token *token = expect_token(TK_BREAK);
  expect_token(';');

  return node_break_stmt(break_level, token);
}

Node *return_statement() {
  expect_token(TK_RETURN);
  Node *expr = !check_token(';') ? expression() : NULL;
  expect_token(';');

  return node_return_stmt(expr);
}

Node *statement() {
  int type = peek_token()->tk_type;
  if (type == '{') {
    begin_scope();
    Node *node = compound_statement();
    end_scope();
    return node;
  }
  if (type == TK_IF) return if_statement();
  if (type == TK_WHILE) return while_statement();
  if (type == TK_DO) return do_while_statement();
  if (type == TK_FOR) return for_statement();
  if (type == TK_CONTINUE) return continue_statement();
  if (type == TK_BREAK) return break_statement();
  if (type == TK_RETURN) return return_statement();
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
  while (!check_token(TK_EOF)) {
    Type *specifier = declaration_specifiers();
    if (check_token(';')) {
      declaration(specifier, NULL);
    } else {
      Symbol *first_symbol = declarator(specifier);
      if (!check_token('{')) {
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
    if (token->tk_type == TK_SPACE || token->tk_type == TK_NEWLINE) continue;
    vector_push(tokens, token);
  }

  string_literals = vector_new();

  tags = map_new();

  continue_level = 0;
  break_level = 0;

  return translate_unit();
}
