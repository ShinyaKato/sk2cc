#include "sk2cc.h"

char *token_type_name[] = {
  "void",
  "bool",
  "char",
  "int",
  "double",
  "unsigned",
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
  "space",
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
  TokenType specifiers[] = { tVOID, tBOOL, tCHAR, tINT, tDOUBLE, tUNSIGNED, tSTRUCT, tENUM };
  for (int i = 0; i < sizeof(specifiers) / sizeof(specifiers[0]); i++) {
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

Initializer *initializer_new() {
  Initializer *initializer = (Initializer *) calloc(1, sizeof(Initializer));
  return initializer;
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
    if (!check_token(tLPAREN) && !symbol) {
      error(token, "undefined identifier.");
    }
    return node_identifier(token->identifier, symbol, token);
  }
  if (token->type == tLPAREN) {
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
      Node *expr = node_additive(ADD, node, index, token);
      node = node_indirect(expr, token);
    } else if (read_token(tDOT)) {
      char *member = expect_token(tIDENTIFIER)->identifier;
      node = node_dot(node, member, token);
    } else if (read_token(tARROW)) {
      char *member = expect_token(tIDENTIFIER)->identifier;
      Node *expr = node_indirect(node, token);
      node = node_dot(expr, member, token);
    } else if (read_token(tINC)) {
      node = node_inc(POST_INC, node, token);
    } else if (read_token(tDEC)) {
      node = node_dec(POST_DEC, node, token);
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
        type = expression()->value_type;
      }
      expect_token(tRPAREN);
    } else {
      type = unary_expression()->value_type;
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
    return node_inc(PRE_INC, unary_expression(), token);
  }
  if (read_token(tDEC)) {
    return node_dec(PRE_DEC, unary_expression(), token);
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
    NodeType type;
    if (read_token(tMUL)) type = MUL;
    else if (read_token(tDIV)) type = DIV;
    else if (read_token(tMOD)) type = MOD;
    else break;

    Node *right = cast_expression();
    node = node_multiplicative(type, node, right, token);
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
    node = node_additive(type, node, right, token);
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
    node = node_shift(type, node, right, token);
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
    node = node_relational(type, node, right, token);
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
    node = node_equality(type, node, right, token);
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
  NodeType type;
  if (read_token(tASSIGN)) type = ASSIGN;
  else if (read_token(tADD_ASSIGN)) type = ADD_ASSIGN;
  else if (read_token(tSUB_ASSIGN)) type = SUB_ASSIGN;
  else if (read_token(tMUL_ASSIGN)) type = MUL_ASSIGN;
  else return conditional_expression(left);

  return node_assign(type, left, assignment_expression(), token);
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
      if (symbol->value_type->incomplete) {
        error(symbol->token, "declaration with incomplete type.");
      }
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
  } else if (read_token(tUNSIGNED)) {
    if (read_token(tCHAR)) {
      return type_uchar();
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

  Symbol *symbol = symbol_new();
  symbol->token = token;
  symbol->identifier = token->identifier;
  symbol->value_type = type;
  symbol->defined = type->type != FUNCTION;

  return symbol;
}

Initializer *initializer() {
  Initializer *initializer = initializer_new();
  if (!read_token(tLBRACE)) {
    initializer->node = assignment_expression();
  } else {
    Vector *elements = vector_new();
    do {
      vector_push(elements, assignment_expression());
    } while (read_token(tCOMMA));
    expect_token(tRBRACE);

    initializer->array = true;
    initializer->elements = elements;
  }

  return initializer;
}

Symbol *init_declarator(Type *specifier, Symbol *symbol) {
  if (read_token(tASSIGN)) {
    symbol->initializer = initializer();
    if (symbol->value_type->type == ARRAY) {
      if (symbol->value_type->incomplete) {
        Type *type = type_array_of(symbol->value_type->array_of, symbol->initializer->elements->length);
        type_copy(symbol->value_type, type);
      }
      if (symbol->value_type->array_size < symbol->initializer->elements->length) {
        error(symbol->token, "too many initializers.");
      }
    }
  }

  return symbol;
}

Vector *declaration(Type *specifier, Symbol *first_symbol) {
  Vector *symbols = vector_new();
  if (first_symbol) {
    Symbol *symbol = init_declarator(specifier, first_symbol);
    if (!specifier->definition && symbol->value_type->incomplete) {
      error(symbol->token, "declaration with incomplete type.");
    }
    if (specifier->definition) {
      map_put(typedef_names, symbol->identifier, symbol->value_type);
    } else {
      symbol_put(symbol->identifier, symbol);
      if (!specifier->external && symbol->value_type->type != FUNCTION) {
        vector_push(symbols, symbol);
      }
    }
  } else if (!check_token(tSEMICOLON)) {
    Symbol *symbol = init_declarator(specifier, declarator(specifier));
    if (!specifier->definition && symbol->value_type->incomplete) {
      error(symbol->token, "declaration with incomplete type.");
    }
    if (specifier->definition) {
      map_put(typedef_names, symbol->identifier, symbol->value_type);
    } else {
      symbol_put(symbol->identifier, symbol);
      if (!specifier->external && symbol->value_type->type != FUNCTION) {
        vector_push(symbols, symbol);
      }
    }
  }
  while (read_token(tCOMMA)) {
    Symbol *symbol = init_declarator(specifier, declarator(specifier));
    if (!specifier->definition && symbol->value_type->incomplete) {
      error(symbol->token, "declaration with incomplete type.");
    }
    if (specifier->definition) {
      map_put(typedef_names, symbol->identifier, symbol->value_type);
    } else {
      symbol_put(symbol->identifier, symbol);
      if (!specifier->external && symbol->value_type->type != FUNCTION) {
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
      for (int i = 0; i < declarations->length; i++) {
        Symbol *symbol = declarations->array[i];
        if (symbol->initializer) {
          Initializer *init = symbol->initializer;
          if (!init->array) {
            Node *identifier = node_identifier(symbol->identifier, symbol, symbol->token);
            Node *assign = node_assign(ASSIGN, identifier, init->node, identifier->token);
            Node *stmt_expr = node_expr_stmt(assign);
            vector_push(statements, stmt_expr);
          } else {
            Initializer *init = symbol->initializer;
            Vector *elements = init->elements;
            for (int j = 0; j < elements->length; j++) {
              Node *node = elements->array[j];
              Node *identifier = node_identifier(symbol->identifier, symbol, symbol->token);
              Node *index = node_int_const(j, symbol->token);
              Node *add = node_additive(ADD, identifier, index, symbol->token);
              Node *lvalue = node_indirect(add, symbol->token);
              Node *assign = node_assign(ASSIGN, lvalue, node, identifier->token);
              Node *stmt_expr = node_expr_stmt(assign);
              vector_push(statements, stmt_expr);
            }
          }
        }
      }
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
    Vector *declarations = declaration(declaration_specifiers(), NULL);
    Vector *statements = vector_new();
    for (int i = 0; i < declarations->length; i++) {
      Symbol *symbol = declarations->array[i];
      if (symbol->initializer) {
        Initializer *init = symbol->initializer;
        if (!init->array) {
          Node *identifier = node_identifier(symbol->identifier, symbol, symbol->token);
          Node *assign = node_assign(ASSIGN, identifier, init->node, identifier->token);
          Node *stmt_expr = node_expr_stmt(assign);
          vector_push(statements, stmt_expr);
        } else {
          Initializer *init = symbol->initializer;
          Vector *elements = init->elements;
          for (int j = 0; j < elements->length; j++) {
            Node *node = elements->array[j];
            Node *identifier = node_identifier(symbol->identifier, symbol, symbol->token);
            Node *index = node_int_const(j, symbol->token);
            Node *add = node_additive(ADD, identifier, index, symbol->token);
            Node *lvalue = node_indirect(add, symbol->token);
            Node *assign = node_assign(ASSIGN, lvalue, node, identifier->token);
            Node *stmt_expr = node_expr_stmt(assign);
            vector_push(statements, stmt_expr);
          }
        }
      }
    }
    init = node_comp_stmt(statements);
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
  symbol->defined = true;
  begin_function_scope(symbol);
  Node *function_body = compound_statement();
  int local_vars_size = get_local_vars_size();
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
          Symbol *symbol = symbols->array[i];
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
    Token *token = input_tokens->array[i];
    if (token->type == tSPACE || token->type == tNEWLINE) continue;
    vector_push(tokens, token);
  }

  string_literals = vector_new();

  tags = map_new();
  typedef_names = map_new();
  enum_constants = map_new();

  continue_level = 0;
  break_level = 0;

  return translate_unit();
}
