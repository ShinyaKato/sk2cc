#include "cc.h"

char *token_type_name[] = {
  "void",
  "bool",
  "char",
  "int",
  "double",
  "struct",
  "enum",
  "typedef",
  "extern",
  "_Noreturn",
  "sizeof",
  "_Alignof",
  "if",
  "else",
  "while",
  "do",
  "for",
  "continue",
  "break",
  "return",
  "identifier",
  "integer constant",
  "floating point constant",
  "string literal",
  "[",
  "]",
  "(",
  ")",
  "{",
  "}",
  ".",
  "->",
  "++",
  "--",
  "~",
  "!",
  "*",
  "/",
  "%",
  "+",
  "-",
  "<<",
  ">>",
  "<",
  ">",
  "<=",
  ">=",
  "==",
  "!=",
  "&",
  "^",
  "|",
  "&&",
  "||",
  "?",
  ":",
  ";",
  "...",
  "=",
  "+=",
  "-=",
  "*=",
  ",",
  "#",
  "new line",
  "end of file"
};

int tokens_pos;
Vector *tokens;

Vector *string_literals;
Map *tags, *typedef_names, *enum_constants;
int continue_level, break_level;

Token *peek_token() {
  return tokens->array[tokens_pos];
}

Token *get_token() {
  return tokens->array[tokens_pos++];
}

Token *expect_token(TokenType type) {
  Token *token = get_token();
  if (token->type != type) {
    error(token, "%s is expected.", token_type_name[type]);
  }
  return token;
}

Token *optional_token(TokenType type) {
  if (peek_token()->type == type) {
    return get_token();
  }
  return NULL;
}

bool read_token(TokenType type) {
  if (peek_token()->type == type) {
    get_token();
    return true;
  }
  return false;
}

bool check_token(TokenType type) {
  return peek_token()->type == type;
}

bool check_strage_class_specifier() {
  return check_token(tTYPEDEF);
}

bool check_type_specifier() {
  TokenType specifiers[7] = { tVOID, tBOOL, tCHAR, tINT, tDOUBLE, tSTRUCT, tENUM };
  for (int i = 0; i < 7; i++) {
    if (check_token(specifiers[i])) {
      return true;
    }
  }
  return check_token(tIDENTIFIER) && map_lookup(typedef_names, peek_token()->identifier);
}

bool check_function_specifier() {
  return check_token(tNORETURN);
}

bool check_declaration_specifier() {
  return check_strage_class_specifier() || check_type_specifier() || check_function_specifier();
}

void begin_loop() {
  continue_level++;
  break_level++;
}

void end_loop() {
  continue_level--;
  break_level--;
}

Symbol *symbol_new() {
  Symbol *symbol = (Symbol *) calloc(1, sizeof(Symbol));
  return symbol;
}

Node *assignment_expression();
Node *expression();
Type *type_name();

Node *primary_expression() {
  Token *token = get_token();

  if (token->type == tINT_CONST) {
    return node_int_const(token->int_value, token);
  }
  if (token->type == tFLOAT_CONST) {
    return node_float_const(token->double_value, token);
  }
  if (token->type == tSTRING_LITERAL) {
    int string_label = string_literals->length;
    vector_push(string_literals, token->string_value);
    return node_string_literal(token->string_value, string_label, token);
  }
  if (token->type == tIDENTIFIER) {
    int *enum_value = map_lookup(enum_constants, token->identifier);
    if (enum_value) {
      return node_int_const(*enum_value, token);
    }
    Symbol *symbol = symbol_lookup(token->identifier);
    return node_identifier(token->identifier, symbol, token);
  }
  if (token->type == tLPAREN) {
    Node *node = expression();
    expect_token(tRPAREN);
    return node;
  }

  error(token, "invalid primary expression.");
}

Node *postfix_expression() {
  Node *node = primary_expression();

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
      Node *expr = node_binary_expr(ADD, node, index, token);
      node = node_unary_expr(INDIRECT, expr, token);
    } else if (read_token(tDOT)) {
      char *member = expect_token(tIDENTIFIER)->identifier;
      node = node_dot(node, member, token);
    } else if (read_token(tARROW)) {
      char *member = expect_token(tIDENTIFIER)->identifier;
      Node *expr = node_unary_expr(INDIRECT, node, token);
      node = node_dot(expr, member, token);
    } else if (read_token(tINC)) {
      node = node_unary_expr(POST_INC, node, token);
    } else if (read_token(tDEC)) {
      node = node_unary_expr(POST_DEC, node, token);
    } else {
      break;
    }
  }

  return node;
}

Node *unary_expression() {
  Token *token = peek_token();

  if (read_token(tSIZEOF)) {
    if (read_token(tLPAREN)) {
      Node *node;
      if (check_type_specifier()) {
        Type *type = type_name();
        node = node_int_const(type->original_size, token);
      } else {
        node = node_unary_expr(SIZEOF, expression(), token);
      }
      expect_token(tRPAREN);
      return node;
    }
    return node_unary_expr(SIZEOF, unary_expression(), token);
  }

  if (read_token(tALIGNOF)) {
    expect_token(tLPAREN);
    Type *type = type_name();
    expect_token(tRPAREN);
    return node_int_const(type->align, token);
  }

  NodeType type;
  if (read_token(tINC)) type = PRE_INC;
  else if (read_token(tDEC)) type = PRE_DEC;
  else if (read_token(tAND)) type = ADDRESS;
  else if (read_token(tMUL)) type = INDIRECT;
  else if (read_token(tADD)) type = UPLUS;
  else if (read_token(tSUB)) type = UMINUS;
  else if (read_token(tNOT)) type = NOT;
  else if (read_token(tLNOT)) type = LNOT;
  else return postfix_expression();

  return node_unary_expr(type, unary_expression(), token);
}

Node *cast_expression() {
  if (!read_token(tLPAREN)) {
    return unary_expression();
  }
  if (!check_type_specifier()) {
    Node *node = expression();
    expect_token(tRPAREN);
    return node;
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
    NodeType type;
    if (read_token(tMUL)) type = MUL;
    else if (read_token(tDIV)) type = DIV;
    else if (read_token(tMOD)) type = MOD;
    else break;

    Node *right = cast_expression();
    node = node_binary_expr(type, node, right, token);
  }

  return node;
}

Node *additive_expression(Node *cast_expr) {
  Node *node = multiplicative_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    NodeType type;
    if (read_token(tADD)) type = ADD;
    else if (read_token(tSUB)) type = SUB;
    else break;

    Node *right = multiplicative_expression(cast_expression());
    node = node_binary_expr(type, node, right, token);
  }

  return node;
}

Node *shift_expression(Node *cast_expr) {
  Node *node = additive_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    NodeType type;
    if (read_token(tLSHIFT)) type = LSHIFT;
    else if (read_token(tRSHIFT)) type = RSHIFT;
    else break;

    Node *right = additive_expression(cast_expression());
    node = node_binary_expr(type, node, right, token);
  }

  return node;
}

Node *relational_expression(Node *cast_expr) {
  Node *node = shift_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    NodeType type;
    if (read_token(tLT)) type = LT;
    else if (read_token(tGT)) type = GT;
    else if (read_token(tLTE)) type = LTE;
    else if (read_token(tGTE)) type = GTE;
    else break;

    Node *right = shift_expression(cast_expression());
    node = node_binary_expr(type, node, right, token);
  }

  return node;
}

Node *equality_expression(Node *cast_expr) {
  Node *node = relational_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    NodeType type;
    if (read_token(tEQ)) type = EQ;
    else if (read_token(tNEQ)) type = NEQ;
    else break;

    Node *right = relational_expression(cast_expression());
    node = node_binary_expr(type, node, right, token);
  }

  return node;
}

Node *and_expression(Node *cast_expr) {
  Node *node = equality_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tAND)) break;

    Node *right = equality_expression(cast_expression());
    node = node_binary_expr(AND, node, right, token);
  }

  return node;
}

Node *exclusive_or_expression(Node *cast_expr) {
  Node *node = and_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tXOR)) break;

    Node *right = and_expression(cast_expression());
    node = node_binary_expr(XOR, node, right, token);
  }

  return node;
}

Node *inclusive_or_expression(Node *cast_expr) {
  Node *node = exclusive_or_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tOR)) break;

    Node *right = exclusive_or_expression(cast_expression());
    node = node_binary_expr(OR, node, right, token);
  }

  return node;
}

Node *logical_and_expression(Node *cast_expr) {
  Node *node = inclusive_or_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tLAND)) break;

    Node *right = inclusive_or_expression(cast_expression());
    node = node_binary_expr(LAND, node, right, token);
  }

  return node;
}

Node *logical_or_expression(Node *cast_expr) {
  Node *node = logical_and_expression(cast_expr);

  while (1) {
    Token *token = peek_token();
    if (!read_token(tLOR)) break;

    Node *right = logical_and_expression(cast_expression());
    node = node_binary_expr(LOR, node, right, token);
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

  return node_condition(control, left, right, token);
}

Node *assignment_expression() {
  Node *left = cast_expression();

  Token *token = peek_token();
  NodeType type;
  if (read_token(tASSIGN)) type = ASSIGN;
  else if (read_token(tADD_ASSIGN)) type = ADD_ASSIGN;
  else if (read_token(tSUB_ASSIGN)) type = SUB_ASSIGN;
  else if (read_token(tMUL_ASSIGN)) type = MUL_ASSIGN;
  else return conditional_expression(left);

  return node_assign(type, left, assignment_expression(), token);
}

Node *expression() {
  return assignment_expression();
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
    incomplete_type->type = STRUCT;
    incomplete_type->incomplete = true;
    map_put(tags, token->identifier, incomplete_type);
  }

  if (!read_token(tLBRACE)) {
    if (!token) error(peek_token(), "invalid struct type specifier.");
    return map_lookup(tags, token->identifier);
  }

  Vector *identifiers = vector_new();
  Map *members = map_new();
  do {
    Type *specifier = type_specifier();
    do {
      Symbol *symbol = declarator(specifier);
      vector_push(identifiers, symbol->identifier);
      map_put(members, symbol->identifier, symbol->value_type);
    } while (read_token(tCOMMA));
    expect_token(tSEMICOLON);
  } while (!check_token(tRBRACE) && !check_token(tEND));
  expect_token(tRBRACE);

  Type *type = type_struct(identifiers, members);
  if (!token) return type;

  Type *dest = map_lookup(tags, token->identifier);
  type_copy(dest, type);
  return dest;
}

Type *enum_specifier() {
  expect_token(tENUM);

  Token *token = optional_token(tIDENTIFIER);

  if (!read_token(tLBRACE)) {
    if (!token) error(peek_token(), "invalid enum type spcifier.");

    Type *type = map_lookup(tags, token->identifier);
    if (!type) error(token, "undefined enum tag.");

    return type;
  }

  int enum_value = 0;
  do {
    Token *token = expect_token(tIDENTIFIER);
    int *enum_const = calloc(1, sizeof(int));
    *enum_const = enum_value++;
    map_put(enum_constants, token->identifier, enum_const);
  } while (read_token(tCOMMA));
  expect_token(tRBRACE);

  Type *type = type_int();
  if (!token) return type;

  map_put(tags, token->identifier, type);
  return type;
}

Type *type_specifier() {
  if (read_token(tVOID)) {
    return type_void();
  } else if (read_token(tBOOL)) {
    return type_bool();
  } else if (read_token(tCHAR)) {
    return type_char();
  } else if (read_token(tINT)) {
    return type_int();
  } else if (read_token(tDOUBLE)) {
    return type_double();
  } else if (check_token(tSTRUCT)) {
    return struct_or_union_specifier();
  } else if (check_token(tENUM)) {
    return enum_specifier();
  }

  Token *token = get_token();
  Type *type = map_lookup(typedef_names, token->identifier);
  if (!type) error(token, "type specifier is expected.");
  return type;
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
    int size = 0;
    if (check_token(tINT_CONST)) {
      size = get_token()->int_value;
    }
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
        if (param->value_type->type == ARRAY) {
          param->value_type = type_pointer_to(param->value_type->array_of);
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

  if (!specifier->definition && type->incomplete) {
    error(token, "declaration with incomplete type.");
  }

  Symbol *symbol = symbol_new();
  symbol->token = token;
  symbol->identifier = token->identifier;
  symbol->value_type = type;

  return symbol;
}

Node *initializer(Symbol *symbol) {
  Node *id = node_identifier(symbol->identifier, symbol, symbol->token);

  Node *node;
  if (!read_token(tLBRACE)) {
    Node *assign = node_assign(ASSIGN, id, assignment_expression(), symbol->token);

    node = node_new();
    node->type = VAR_INIT;
    node->expr = assign;
  } else {
    Vector *elements = vector_new();
    do {
      Node *index = node_int_const(elements->length, symbol->token);
      Node *add = node_binary_expr(ADD, id, index, symbol->token);
      Node *lvalue = node_unary_expr(INDIRECT, add, symbol->token);
      Node *assign = node_assign(ASSIGN, lvalue, assignment_expression(), symbol->token);
      vector_push(elements, assign);
    } while (read_token(tCOMMA));
    expect_token(tRBRACE);

    node = node_new();
    node->type = VAR_ARRAY_INIT;
    node->array_elements = elements;
  }

  return node;
}

Node *init_declarator(Type *specifier, Symbol *symbol) {
  Node *node = node_new();
  node->type = VAR_INIT_DECL;
  node->symbol = symbol;
  if (read_token(tASSIGN)) {
    node->initializer = initializer(symbol);
    if (symbol->value_type->size == 0 && node->initializer->type == VAR_ARRAY_INIT) {
      int length = node->initializer->array_elements->length;
      symbol->value_type->array_size = length;
      symbol->value_type->size = length * symbol->value_type->array_of->size;
    }
  }

  return node;
}

Node *declaration(Type *specifier, Symbol *first_decl) {
  Vector *declarations = vector_new();

  if (first_decl || !read_token(tSEMICOLON)) {
    if (!first_decl) first_decl = declarator(specifier);
    if (specifier->definition) {
      map_put(typedef_names, first_decl->identifier, first_decl->value_type);
    } else {
      Node *first_init_decl = init_declarator(specifier, first_decl);
      first_decl->declaration = specifier->external || first_decl->value_type->type == FUNCTION;
      vector_push(declarations, first_init_decl);
      symbol_put(first_decl->identifier, first_decl);
    }

    while (read_token(tCOMMA)) {
      Symbol *decl = declarator(specifier);
      if (specifier->definition) {
        map_put(typedef_names, decl->identifier, decl->value_type);
      } else {
        Node *init_decl = init_declarator(specifier, decl);
        decl->declaration = specifier->external || decl->value_type->type == FUNCTION;
        vector_push(declarations, init_decl);
        symbol_put(decl->identifier, decl);
      }
    }
    expect_token(tSEMICOLON);
  }

  Node *node = node_new();
  node->type = VAR_DECL;
  node->declarations = declarations;

  return node;
}

Node *statement();

Node *compound_statement() {
  Vector *statements = vector_new();
  expect_token(tLBRACE);
  while (!check_token(tRBRACE) && !check_token(tEND)) {
    if (check_declaration_specifier()) {
      vector_push(statements, declaration(declaration_specifiers(), NULL));
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
  Node *control = expression();
  expect_token(tRPAREN);
  Node *if_body = statement();
  Node *else_body = read_token(tELSE) ? statement() : NULL;

  return node_if_stmt(control, if_body, else_body);
}

Node *while_statement() {
  begin_loop();
  expect_token(tWHILE);
  expect_token(tLPAREN);
  Node *control = expression();
  expect_token(tRPAREN);
  Node *loop_body = statement();
  end_loop();

  return node_while_stmt(control, loop_body);
}

Node *do_while_statement() {
  begin_loop();
  expect_token(tDO);
  Node *loop_body = statement();
  expect_token(tWHILE);
  expect_token(tLPAREN);
  Node *control = expression();
  expect_token(tRPAREN);
  expect_token(tSEMICOLON);
  end_loop();

  return node_do_while_stmt(control, loop_body);
}

Node *for_statement() {
  begin_scope();
  begin_loop();
  expect_token(tFOR);
  expect_token(tLPAREN);
  Node *init;
  if (check_declaration_specifier()) {
    init = declaration(declaration_specifiers(), NULL);
  } else {
    init = !check_token(tSEMICOLON) ? expression() : NULL;
    expect_token(tSEMICOLON);
  }
  Node *control = !check_token(tSEMICOLON) ? expression() : NULL;
  expect_token(tSEMICOLON);
  Node *afterthrough = !check_token(tRPAREN) ? expression() : NULL;
  expect_token(tRPAREN);
  Node *loop_body = statement();
  end_loop();
  end_scope();

  return node_for_stmt(init, control, afterthrough, loop_body);
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
  TokenType type = peek_token()->type;
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
  begin_function_scope(symbol);
  Node *function_body = compound_statement();
  int local_vars_size = get_local_vars_size();
  end_scope();

  return node_func_def(symbol, function_body, local_vars_size, symbol->token);
}

Node *translate_unit() {
  begin_global_scope();
  Vector *definitions = vector_new();
  while (!check_token(tEND)) {
    Type *specifier = declaration_specifiers();
    if (check_token(tSEMICOLON)) {
      Node *node = declaration(specifier, NULL);
      vector_push(definitions, node);
    } else {
      Symbol *first_decl = declarator(specifier);
      if (check_token(tLBRACE)) {
        Node *node = function_definition(first_decl);
        vector_push(definitions, node);
      } else {
        Node *node = declaration(specifier, first_decl);
        vector_push(definitions, node);
      }
    }
  }
  end_scope();

  return node_trans_unit(string_literals, definitions);
}

Node *parse(Vector *input_tokens) {
  tokens_pos = 0;
  tokens = input_tokens;

  string_literals = vector_new();

  tags = map_new();
  typedef_names = map_new();
  enum_constants = map_new();

  continue_level = 0;
  break_level = 0;

  return translate_unit();
}
