#include "sk2cc.h"

Vector *string_literals;
Map *tags;
int continue_level, break_level;

Vector *scopes;
int stack_size;

Symbol *symbol_new(SymbolType sy_type, Type *type, char *identifier, Token *token) {
  Symbol *symbol = calloc(1, sizeof(Symbol));
  symbol->sy_type = sy_type;
  symbol->type = type;
  symbol->identifier = identifier;
  symbol->token = token;
  return symbol;
}

Symbol *symbol_variable(Type *type, char *identifier, Token *token) {
  Symbol *symbol = symbol_new(SY_VARIABLE, type, identifier, token);
  symbol->definition = type->ty_type != TY_FUNCTION;
  return symbol;
}

Symbol *symbol_const(char *identifier, int value, Token *token) {
  Symbol *symbol = symbol_new(SY_CONST, type_int(), identifier, token);
  symbol->const_value = value;
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

  // check the identifier in the current scope.
  if (identifier) {
    Symbol *prev = map_lookup(map, identifier);
    if (prev && prev->definition) {
      error(symbol->token, "duplicated definition of '%s'.", identifier);
    }
  }

  // if the symbol is a variable, add linkage.
  if (symbol->sy_type == SY_VARIABLE) {
    if (scopes->length == 1) {
      // global variable
      symbol->link = LN_EXTERNAL;
    } else {
      // local variable
      int size = symbol->type->size;
      if (size % 8 != 0) {
        size = size / 8 * 8 + 8;
      }
      stack_size += size;

      symbol->link = LN_NONE;
      symbol->offset = stack_size;
    }
  }

  if (identifier) {
    map_put(map, identifier, symbol);
  }
}

void begin_scope() {
  vector_push(scopes, map_new());
}

void end_scope() {
  vector_pop(scopes);
}

void begin_function_scope(Symbol *symbol) {
  symbol_put(symbol->identifier, symbol);

  stack_size = symbol->type->ellipsis ? 176 : 0;
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
    return symbol && symbol->sy_type == SY_TYPE;
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

Expr *cast_expression();
Expr *assignment_expression();
Expr *expression();
Type *type_name();

Expr *primary_expression() {
  Token *token = get_token();

  if (token->tk_type == TK_IDENTIFIER) {
    Symbol *symbol = symbol_lookup(token->identifier);
    if (symbol && symbol->sy_type == SY_CONST) {
      return expr_integer(symbol->const_value, token);
    }
    if (!check_token('(') && !symbol) {
      error(token, "undefined identifier.");
    }
    return expr_identifier(token->identifier, symbol, token);
  }
  if (token->tk_type == TK_INTEGER_CONST) {
    return expr_integer(token->int_value, token);
  }
  if (token->tk_type == TK_STRING_LITERAL) {
    int string_label = string_literals->length;
    vector_push(string_literals, token->string_literal);
    return expr_string(token->string_literal, string_label, token);
  }
  if (token->tk_type == '(') {
    Expr *expr = expression();
    expect_token(')');
    return expr;
  }

  error(token, "invalid primary expression.");
}

Expr *postfix_expression(Expr *node) {
  while (1) {
    Token *token = peek_token();

    if (read_token('[')) {
      Expr *index = expression();
      expect_token(']');
      node = expr_subscription(node, index, token);
    } else if (read_token('(')) {
      Vector *args = vector_new();
      if (!check_token(')')) {
        do {
          vector_push(args, assignment_expression());
        } while (read_token(','));
      }
      expect_token(')');
      node = expr_call(node, args, token);
    } else if (read_token('.')) {
      char *member = expect_token(TK_IDENTIFIER)->identifier;
      node = expr_dot(node, member, token);
    } else if (read_token(TK_ARROW)) {
      char *member = expect_token(TK_IDENTIFIER)->identifier;
      node = expr_arrow(node, member, token);
    } else if (read_token(TK_INC)) {
      node = expr_post_inc(node, token);
    } else if (read_token(TK_DEC)) {
      node = expr_post_dec(node, token);
    } else {
      break;
    }
  }

  return node;
}

Expr *unary_expression() {
  Token *token = peek_token();

  if (read_token(TK_INC)) {
    return expr_pre_inc(unary_expression(), token);
  }
  if (read_token(TK_DEC)) {
    return expr_pre_dec(unary_expression(), token);
  }
  if (read_token('&')) {
    return expr_address(cast_expression(), token);
  }
  if (read_token('*')) {
    return expr_indirect(cast_expression(), token);
  }
  if (read_token('+')) {
    return expr_uplus(cast_expression(), token);
  }
  if (read_token('-')) {
    return expr_uminus(cast_expression(), token);
  }
  if (read_token('~')) {
    return expr_not(cast_expression(), token);
  }
  if (read_token('!')) {
    return expr_lnot(cast_expression(), token);
  }
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
    return expr_integer(type->original->size, token);
  }
  if (read_token(TK_ALIGNOF)) {
    expect_token('(');
    Type *type = type_name();
    expect_token(')');
    return expr_integer(type->align, token);
  }

  return postfix_expression(primary_expression());
}

Expr *cast_expression() {
  if (!read_token('(')) {
    return unary_expression();
  }
  if (!check_type_specifier()) {
    Expr *expr = expression();
    expect_token(')');
    return postfix_expression(expr);
  }

  Token *token = peek_token();
  Type *type = type_name();
  expect_token(')');

  return expr_cast(type, cast_expression(), token);
}

Expr *multiplicative_expression(Expr *node) {
  if (!node) {
    node = cast_expression();
  }

  while (1) {
    Token *token = peek_token();

    if (read_token('*')) {
      node = expr_mul(node, cast_expression(), token);
    } else if (read_token('/')) {
      node = expr_div(node, cast_expression(), token);
    } else if (read_token('%')) {
      node = expr_mod(node, cast_expression(), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *additive_expression(Expr *node) {
  node = multiplicative_expression(node);

  while (1) {
    Token *token = peek_token();

    if (read_token('+')) {
      node = expr_add(node, multiplicative_expression(NULL), token);
    } else if (read_token('-')) {
      node = expr_sub(node, multiplicative_expression(NULL), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *shift_expression(Expr *node) {
  node = additive_expression(node);

  while (1) {
    Token *token = peek_token();

    if (read_token(TK_LSHIFT)) {
      node = expr_lshift(node, additive_expression(NULL), token);
    } else if (read_token(TK_RSHIFT)) {
      node = expr_rshift(node, additive_expression(NULL), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *relational_expression(Expr *node) {
  node = shift_expression(node);

  while (1) {
    Token *token = peek_token();

    if (read_token('<')) {
      node = expr_lt(node, shift_expression(NULL), token);
    } else if (read_token('>')) {
      node = expr_gt(node, shift_expression(NULL), token);
    } else if (read_token(TK_LTE)) {
      node = expr_lte(node, shift_expression(NULL), token);
    } else if (read_token(TK_GTE)) {
      node = expr_gte(node, shift_expression(NULL), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *equality_expression(Expr *node) {
  node = relational_expression(node);

  while (1) {
    Token *token = peek_token();

    if (read_token(TK_EQ)) {
      node = expr_eq(node, relational_expression(NULL), token);
    } else if (read_token(TK_NEQ)) {
      node = expr_neq(node, relational_expression(NULL), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *and_expression(Expr *node) {
  node = equality_expression(node);

  while (1) {
    Token *token = peek_token();

    if (read_token('&')) {
      node = expr_and(node, equality_expression(NULL), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *exclusive_or_expression(Expr *node) {
  node = and_expression(node);

  while (1) {
    Token *token = peek_token();

    if (read_token('^')) {
      node = expr_xor(node, and_expression(NULL), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *inclusive_or_expression(Expr *node) {
  node = exclusive_or_expression(node);

  while (1) {
    Token *token = peek_token();

    if (read_token('|')) {
      node = expr_or(node, exclusive_or_expression(NULL), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *logical_and_expression(Expr *node) {
  node = inclusive_or_expression(node);

  while (1) {
    Token *token = peek_token();

    if (read_token(TK_AND)) {
      node = expr_land(node, inclusive_or_expression(NULL), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *logical_or_expression(Expr *node) {
  node = logical_and_expression(node);

  while (1) {
    Token *token = peek_token();

    if (read_token(TK_OR)) {
      node = expr_lor(node, logical_and_expression(NULL), token);
    } else {
      break;
    }
  }

  return node;
}

Expr *conditional_expression(Expr *node) {
  node = logical_or_expression(node);

  Token *token = peek_token();
  if (read_token('?')) {
    Expr *lhs = expression();
    expect_token(':');
    Expr *rhs = conditional_expression(NULL);
    return expr_conditional(node, lhs, rhs, token);
  }

  return node;
}

Expr *assignment_expression() {
  Expr *node = cast_expression();

  Token *token = peek_token();
  if (read_token('=')) {
    return expr_assign(node, assignment_expression(), token);
  } else if (read_token(TK_ADD_ASSIGN)) {
    return expr_add_assign(node, assignment_expression(), token);
  } else if (read_token(TK_SUB_ASSIGN)) {
    return expr_sub_assign(node, assignment_expression(), token);
  } else if (read_token(TK_MUL_ASSIGN)) {
    return expr_mul_assign(node, assignment_expression(), token);
  } else if (read_token(TK_DIV_ASSIGN)) {
    return expr_div_assign(node, assignment_expression(), token);
  } else if (read_token(TK_MOD_ASSIGN)) {
    return expr_mod_assign(node, assignment_expression(), token);
  }

  return conditional_expression(node);
}

Expr *expression() {
  Expr *node = assignment_expression();

  while (1) {
    Token *token = peek_token();

    if (read_token(',')) {
      node = expr_comma(node, assignment_expression(), token);
    } else {
      break;
    }
  }

  return node;
}

Type *type_specifier();
Symbol *declarator(Type *type);

Type *abstract_declarator(Type *specifier) {
  Type *type = specifier;
  while (read_token('*')) {
    type = type_pointer(type);
  }
  return type;
}

Type *type_name() {
  Type *specifier = type_specifier();
  return abstract_declarator(specifier);
}

Type *struct_or_union_specifier() {
  expect_token(TK_STRUCT);

  Token *token = read_token(TK_IDENTIFIER);
  if (token && !map_lookup(tags, token->identifier)) {
    Type *type = type_incomplete_struct();
    map_put(tags, token->identifier, type);
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
      if (!symbol->type->complete) {
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
  dest->size = type->size;
  dest->align = type->align;
  dest->complete = true;
  dest->members = type->members;
  return dest;
}

Type *enum_specifier() {
  expect_token(TK_ENUM);

  Token *token = read_token(TK_IDENTIFIER);
  if (token && !map_lookup(tags, token->identifier)) {
    map_put(tags, token->identifier, type_int());
  }

  if (!read_token('{')) {
    if (!token) error(peek_token(), "invalid enum type spcifier.");
    return map_lookup(tags, token->identifier);
  }

  int const_value = 0;
  do {
    Token *token = expect_token(TK_IDENTIFIER);
    if (read_token('=')) {
      const_value = expect_token(TK_INTEGER_CONST)->int_value;
    }

    Symbol *symbol = symbol_const(token->identifier, const_value++, token);
    symbol_put(token->identifier, symbol);
  } while (read_token(','));
  expect_token('}');

  if (!token) return type_int();

  return map_lookup(tags, token->identifier);
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
  if (symbol && symbol->sy_type == SY_TYPE) {
    return symbol->type;
  }

  error(token, "type specifier is expected.");
}

Type *declaration_specifiers() {
  bool definition = read_token(TK_TYPEDEF) != NULL;
  bool external = read_token(TK_EXTERN) != NULL;
  read_token(TK_NORETURN);

  Type *specifier = type_specifier();
  specifier->definition = definition;
  specifier->external = external;

  return specifier;
}

Type *direct_declarator(Type *type) {
  if (read_token('[')) {
    if (read_token(']')) {
      return type_incomplete_array(direct_declarator(type));
    }
    int size = expect_token(TK_INTEGER_CONST)->int_value;
    expect_token(']');
    return type_array(direct_declarator(type), size);
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
        if (param->type->ty_type == TY_ARRAY) {
          param->type = type_pointer(param->type->array_of);
        }
        vector_push(params, param);
      } while (read_token(','));
    }
    expect_token(')');
    return type_function(direct_declarator(type), params, ellipsis);
  }

  return type;
}

Symbol *declarator(Type *specifier) {
  Type *type = specifier;
  while (read_token('*')) {
    type = type_pointer(type);
  }
  Token *token = expect_token(TK_IDENTIFIER);
  type = direct_declarator(type);

  return symbol_variable(type, token->identifier, token);
}

Init *initializer(Type *type) {
  if (type->ty_type == TY_ARRAY) {
    Vector *list = vector_new();
    expect_token('{');
    do {
      vector_push(list, initializer(type->array_of));
    } while (read_token(','));
    expect_token('}');
    return init_list(list, type);
  }

  return init_expr(assignment_expression(), type);
}

Symbol *init_declarator(Type *specifier, Symbol *symbol) {
  if (read_token('=')) {
    symbol->init = initializer(symbol->type);
    if (symbol->type->ty_type == TY_ARRAY) {
      int length = symbol->init->list->length;
      if (!symbol->type->complete) {
        symbol->type->size = symbol->type->array_of->size * length;
        symbol->type->complete = true;
        symbol->type->length = length;
      } else {
        if (symbol->type->length < length) {
          error(symbol->token, "too many array elements.");
        }
      }
    }
  }

  return symbol;
}

Decl *declaration(Type *specifier, Symbol *first_symbol) {
  Vector *symbols = vector_new();
  if (first_symbol) {
    Symbol *symbol = init_declarator(specifier, first_symbol);
    if (!specifier->definition && !symbol->type->complete) {
      error(symbol->token, "declaration with incomplete type.");
    }
    if (specifier->definition) {
      symbol->sy_type = SY_TYPE;
      symbol->definition = false;
      symbol_put(symbol->identifier, symbol);
    } else {
      symbol_put(symbol->identifier, symbol);
      if (!specifier->external && symbol->type->ty_type != TY_FUNCTION) {
        vector_push(symbols, symbol);
      }
    }
  } else if (!check_token(';')) {
    Symbol *symbol = init_declarator(specifier, declarator(specifier));
    if (!specifier->definition && !symbol->type->complete) {
      error(symbol->token, "declaration with incomplete type.");
    }
    if (specifier->definition) {
      symbol->sy_type = SY_TYPE;
      symbol->definition = false;
      symbol_put(symbol->identifier, symbol);
    } else {
      symbol_put(symbol->identifier, symbol);
      if (!specifier->external && symbol->type->ty_type != TY_FUNCTION) {
        vector_push(symbols, symbol);
      }
    }
  }
  while (read_token(',')) {
    Symbol *symbol = init_declarator(specifier, declarator(specifier));
    if (!specifier->definition && !symbol->type->complete) {
      error(symbol->token, "declaration with incomplete type.");
    }
    if (specifier->definition) {
      symbol->sy_type = SY_TYPE;
      symbol->definition = false;
      symbol_put(symbol->identifier, symbol);
    } else {
      symbol_put(symbol->identifier, symbol);
      if (!specifier->external && symbol->type->ty_type != TY_FUNCTION) {
        vector_push(symbols, symbol);
      }
    }
  }
  expect_token(';');

  return decl_new(symbols);
}

Stmt *statement();

Stmt *compound_statement() {
  Token *token = peek_token();

  Vector *statements = vector_new();
  expect_token('{');
  while (!check_token('}') && !check_token(TK_EOF)) {
    if (check_declaration_specifier()) {
      vector_push(statements, declaration(declaration_specifiers(), NULL));
    } else {
      vector_push(statements, statement());
    }
  }
  expect_token('}');

  return stmt_comp(statements, token);
}

Stmt *expression_statement() {
  Token *token = peek_token();

  Expr *expr = !check_token(';') ? expression() : NULL;
  expect_token(';');

  return stmt_expr(expr, token);
}

Stmt *if_statement() {
  Token *token = expect_token(TK_IF);

  expect_token('(');
  Expr *if_cond = expression();
  expect_token(')');

  Stmt *then_body = statement();
  Stmt *else_body = read_token(TK_ELSE) ? statement() : NULL;

  return stmt_if(if_cond, then_body, else_body, token);
}

Stmt *while_statement() {
  Token *token = expect_token(TK_WHILE);

  expect_token('(');
  Expr *while_cond = expression();
  expect_token(')');

  begin_loop();
  Stmt *while_body = statement();
  end_loop();

  return stmt_while(while_cond, while_body, token);
}

Stmt *do_while_statement() {
  Token *token = expect_token(TK_DO);

  begin_loop();
  Stmt *do_body = statement();
  end_loop();

  expect_token(TK_WHILE);

  expect_token('(');
  Expr *do_cond = expression();
  expect_token(')');

  expect_token(';');

  return stmt_do(do_cond, do_body, token);
}

Stmt *for_statement() {
  Token *token = expect_token(TK_FOR);

  begin_scope();

  expect_token('(');
  Node *for_init;
  if (check_declaration_specifier()) {
    for_init = (Node *) declaration(declaration_specifiers(), NULL);
  } else {
    for_init = (Node *) (!check_token(';') ? expression() : NULL);
    expect_token(';');
  }
  Expr *for_cond = !check_token(';') ? expression() : NULL;
  expect_token(';');
  Expr *for_after = !check_token(')') ? expression() : NULL;
  expect_token(')');

  begin_loop();
  Stmt *for_body = statement();
  end_loop();

  end_scope();

  return stmt_for(for_init, for_cond, for_after, for_body, token);
}

Stmt *continue_statement() {
  Token *token = expect_token(TK_CONTINUE);
  expect_token(';');

  return stmt_continue(continue_level, token);
}

Stmt *break_statement() {
  Token *token = expect_token(TK_BREAK);
  expect_token(';');

  return stmt_break(break_level, token);
}

Stmt *return_statement() {
  Token *token = expect_token(TK_RETURN);

  Expr *ret_expr = !check_token(';') ? expression() : NULL;
  expect_token(';');

  return stmt_return(ret_expr, token);
}

Stmt *statement() {
  if (check_token('{')) {
    begin_scope();
    Stmt *stmt = compound_statement();
    end_scope();
    return stmt;
  }
  if (check_token(TK_IF)) {
    return if_statement();
  }
  if (check_token(TK_WHILE)) {
    return while_statement();
  }
  if (check_token(TK_DO)) {
    return do_while_statement();
  }
  if (check_token(TK_FOR)) {
    return for_statement();
  }
  if (check_token(TK_CONTINUE)) {
    return continue_statement();
  }
  if (check_token(TK_BREAK)) {
    return break_statement();
  }
  if (check_token(TK_RETURN)) {
    return return_statement();
  }

  return expression_statement();
}

Func *function_definition(Symbol *symbol) {
  begin_function_scope(symbol);
  Stmt *body = compound_statement();
  end_scope();

  symbol->definition = true;

  return func_new(symbol, body, stack_size, symbol->token);
}

TransUnit *translate_unit() {
  Vector *external_decls = vector_new();

  begin_global_scope();
  while (!check_token(TK_EOF)) {
    Type *specifier = declaration_specifiers();
    if (check_token(';')) {
      declaration(specifier, NULL);
    } else {
      Symbol *symbol = declarator(specifier);
      if (!check_token('{')) {
        vector_push(external_decls, declaration(specifier, symbol));
      } else {
        vector_push(external_decls, function_definition(symbol));
      }
    }
  }
  end_scope();

  return trans_unit_new(string_literals, external_decls);
}

TransUnit *parse(Vector *input_tokens) {
  Vector *tokens = vector_new();
  for (int i = 0; i < input_tokens->length; i++) {
    Token *token = input_tokens->buffer[i];
    if (token->tk_type == TK_SPACE) continue;
    if (token->tk_type == TK_NEWLINE) continue;
    vector_push(tokens, token);
  }
  scanner_init(tokens);

  string_literals = vector_new();

  continue_level = 0;
  break_level = 0;

  tags = map_new();

  return translate_unit();
}
