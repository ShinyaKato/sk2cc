#include "cc.h"

Map *tags, *typedef_names, *enum_constants;

Symbol *symbol_new() {
  Symbol *symbol = (Symbol *) calloc(1, sizeof(Symbol));
  return symbol;
}

Node *node_new() {
  Node *node = (Node *) calloc(1, sizeof(Node));
  return node;
}

bool check_strage_class_specifier() {
  Token *token = peek_token();
  if (token->type == tTYPEDEF) return true;
  return false;
}

bool check_type_specifier() {
  Token *token = peek_token();
  if (token->type == tVOID) return true;
  if (token->type == tBOOL) return true;
  if (token->type == tCHAR) return true;
  if (token->type == tINT) return true;
  if (token->type == tSTRUCT) return true;
  if (token->type == tENUM) return true;
  if (token->type == tIDENTIFIER && map_lookup(typedef_names, token->identifier)) return true;
  return false;
}

bool check_declaration_specifier() {
  return check_strage_class_specifier() || check_type_specifier();
}

Node *assignment_expression();
Node *expression();
Type *type_name();

Node *primary_expression() {
  Token *token = get_token();

  if (token->type == tINT_CONST) {
    Node *node = node_new();
    node->type = CONST;
    node->int_value = token->int_value;
    return node;
  }

  if (token->type == tSTRING_LITERAL) {
    Node *node = node_new();
    node->type = STRING_LITERAL;
    node->string_value = token->string_value;
    return node;
  }

  if (token->type == tIDENTIFIER) {
    int *enum_value = map_lookup(enum_constants, token->identifier);
    if (enum_value) {
      Node *node = node_new();
      node->type = CONST;
      node->int_value = *enum_value;
      return node;
    } else {
      Node *node = node_new();
      node->type = IDENTIFIER;
      node->identifier = token->identifier;
      return node;
    }
  }

  if (token->type == tLPAREN) {
    Node *node = expression();
    expect_token(tRPAREN);
    return node;
  }

  error("unexpected primary expression.");
}

Node *postfix_expression() {
  Node *node = primary_expression();

  while (1) {
    if (read_token(tLPAREN)) {
      Node *expr = node;
      Vector *args = vector_new();
      if (peek_token()->type != tRPAREN) {
        do {
          vector_push(args, assignment_expression());
        } while (read_token(tCOMMA));
      }
      expect_token(tRPAREN);

      node = node_new();
      node->type = FUNC_CALL;
      node->expr = expr;
      node->args = args;
    } else if (read_token(tLBRACKET)) {
      Node *left = node;
      Node *right = expression();
      expect_token(tRBRACKET);

      Node *expr = node_new();
      expr->type = ADD;
      expr->left = left;
      expr->right = right;

      node = node_new();
      node->type = INDIRECT;
      node->expr = expr;
    } else if (read_token(tDOT)) {
      Node *expr = node;
      Token *token = expect_token(tIDENTIFIER);

      node = node_new();
      node->type = DOT;
      node->expr = expr;
      node->identifier = token->identifier;
    } else if (read_token(tARROW)) {
      Node *expr = node_new();
      expr->type = INDIRECT;
      expr->expr = node;
      Token *token = expect_token(tIDENTIFIER);

      node = node_new();
      node->type = DOT;
      node->expr = expr;
      node->identifier = token->identifier;
    } else if (read_token(tINC)) {
      Node *expr = node;

      node = node_new();
      node->type = POST_INC;
      node->expr = expr;
    } else if (read_token(tDEC)) {
      Node *expr = node;

      node = node_new();
      node->type = POST_DEC;
      node->expr = expr;
    } else {
      break;
    }
  }

  return node;
}

Node *unary_expression() {
  if (read_token(tSIZEOF)) {
    Node *node = node_new();
    if (read_token(tLPAREN)) {
      if (check_type_specifier()) {
        Type *type = type_name();
        node->type = CONST;
        node->int_value = type->size;
      } else {
        node->type = SIZEOF;
        node->expr = expression();
      }
      expect_token(tRPAREN);
    } else {
      node->type = SIZEOF;
      node->expr = unary_expression();
    }
    return node;
  }

  if (read_token(tALIGNOF)) {
    expect_token(tLPAREN);
    Type *type = type_name();
    expect_token(tRPAREN);
    Node *node = node_new();
    node->type = CONST;
    node->int_value = type->align;
    return node;
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

  Node *node = node_new();
  node->type = type;
  node->expr = unary_expression();

  return node;
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
  Type *type = type_name();
  expect_token(tRPAREN);

  Node *node = node_new();
  node->type = CAST;
  node->value_type = type;
  node->expr = cast_expression();

  return node;
}

Node *multiplicative_expression(Node *cast_expr) {
  Node *node = cast_expr;

  while (1) {
    NodeType type;
    if (read_token(tMUL)) type = MUL;
    else if (read_token(tDIV)) type = DIV;
    else if (read_token(tMOD)) type = MOD;
    else break;

    Node *left = node;
    Node *right = cast_expression();

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *additive_expression(Node *cast_expr) {
  Node *node = multiplicative_expression(cast_expr);

  while (1) {
    NodeType type;
    if (read_token(tADD)) type = ADD;
    else if (read_token(tSUB)) type = SUB;
    else break;

    Node *left = node;
    Node *right = multiplicative_expression(cast_expression());

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *shift_expression(Node *cast_expr) {
  Node *node = additive_expression(cast_expr);

  while (1) {
    NodeType type;
    if (read_token(tLSHIFT)) type = LSHIFT;
    else if (read_token(tRSHIFT)) type = RSHIFT;
    else break;

    Node *left = node;
    Node *right = additive_expression(cast_expression());

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *relational_expression(Node *cast_expr) {
  Node *node = shift_expression(cast_expr);

  while (1) {
    NodeType type;
    if (read_token(tLT)) type = LT;
    else if (read_token(tGT)) type = GT;
    else if (read_token(tLTE)) type = LTE;
    else if (read_token(tGTE)) type = GTE;
    else break;

    Node *left = node;
    Node *right = shift_expression(cast_expression());

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *equality_expression(Node *cast_expr) {
  Node *node = relational_expression(cast_expr);

  while (1) {
    NodeType type;
    if (read_token(tEQ)) type = EQ;
    else if (read_token(tNEQ)) type = NEQ;
    else break;

    Node *left = node;
    Node *right = relational_expression(cast_expression());

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *and_expression(Node *cast_expr) {
  Node *node = equality_expression(cast_expr);

  while (read_token(tAND)) {
    Node *left = node;
    Node *right = equality_expression(cast_expression());

    node = node_new();
    node->type = AND;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *exclusive_or_expression(Node *cast_expr) {
  Node *node = and_expression(cast_expr);

  while (read_token(tXOR)) {
    Node *left = node;
    Node *right = and_expression(cast_expression());

    node = node_new();
    node->type = XOR;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *inclusive_or_expression(Node *cast_expr) {
  Node *node = exclusive_or_expression(cast_expr);

  while (read_token(tOR)) {
    Node *left = node;
    Node *right = exclusive_or_expression(cast_expression());

    node = node_new();
    node->type = OR;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *logical_and_expression(Node *cast_expr) {
  Node *node = inclusive_or_expression(cast_expr);

  while (read_token(tLAND)) {
    Node *left = node;
    Node *right = inclusive_or_expression(cast_expression());

    node = node_new();
    node->type = LAND;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *logical_or_expression(Node *cast_expr) {
  Node *node = logical_and_expression(cast_expr);

  while (read_token(tLOR)) {
    Node *left = node;
    Node *right = logical_and_expression(cast_expression());

    node = node_new();
    node->type = LOR;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *conditional_expression(Node *cast_expr) {
  Node *node = logical_or_expression(cast_expr);

  if (read_token(tQUESTION)) {
    Node *control = node;
    Node *left = expression();
    expect_token(tCOLON);
    Node *right = conditional_expression(cast_expression());

    node = node_new();
    node->type = CONDITION;
    node->control = control;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *assignment_expression() {
  Node *cast_expr = cast_expression();

  NodeType type;
  if (read_token(tASSIGN)) type = ASSIGN;
  else if (read_token(tADD_ASSIGN)) type = ADD_ASSIGN;
  else if (read_token(tSUB_ASSIGN)) type = SUB_ASSIGN;
  else return conditional_expression(cast_expr);

  Node *left = cast_expr;
  Node *right = assignment_expression();

  Node *node = node_new();
  node->type = type;
  node->value_type = left->value_type;
  node->left = left;
  node->right = right;

  return node;
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
    if (!token) error("invalid struct type specifier.");

    Type *type = map_lookup(tags, token->identifier);
    if (!type) error("undefined struct tag.");

    return type;
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
  } while (peek_token()->type != tRBRACE && peek_token()->type != tEND);
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
    if (!token) error("invalid enum type spcifier.");

    Type *type = map_lookup(tags, token->identifier);
    if (!type) error("undefined enum tag.");

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
  } else if (peek_token()->type == tSTRUCT) {
    return struct_or_union_specifier();
  } else if (peek_token()->type == tENUM) {
    return enum_specifier();
  } else if (peek_token()->type == tIDENTIFIER) {
    Token *token = get_token();
    Type *type = map_lookup(typedef_names, token->identifier);
    if (type) return type;
  }
  error("type specifier is expected.");
}

Type *declaration_specifiers() {
  bool definition = read_token(tTYPEDEF);
  bool external = read_token(tEXTERN);

  Type *specifier = type_specifier();
  specifier->definition = definition;
  specifier->external = external;

  return specifier;
}

Type *direct_declarator(Type *type) {
  if (read_token(tLBRACKET)) {
    int size = expect_token(tINT_CONST)->int_value;
    expect_token(tRBRACKET);
    return type_array_of(direct_declarator(type), size);
  }

  if (read_token(tLPAREN)) {
    Vector *params = vector_new();
    bool ellipsis = false;
    if (peek_token()->type != tRPAREN) {
      do {
        if (read_token(tELLIPSIS)) {
          ellipsis = true;
          break;
        }
        Type *specifier = declaration_specifiers();
        Symbol *param = declarator(specifier);
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
    error("declaration with incomplete type.");
  }

  Symbol *symbol = symbol_new();
  symbol->identifier = token->identifier;
  symbol->value_type = type;

  return symbol;
}

Node *initializer(Symbol *symbol) {
  Node *left = node_new();
  Node *right = assignment_expression();

  left->type = IDENTIFIER;
  left->identifier = symbol->identifier;

  Node *node = node_new();
  node->type = ASSIGN;
  node->left = left;
  node->right = right;

  return node;
}

Node *init_declarator(Type *specifier, Symbol *symbol) {
  Node *node = node_new();
  node->type = VAR_INIT_DECL;
  node->symbol = symbol;
  if (read_token(tASSIGN)) {
    node->initializer = initializer(symbol);
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
    }

    while (read_token(tCOMMA)) {
      Symbol *decl = declarator(specifier);
      if (specifier->definition) {
        map_put(typedef_names, decl->identifier, decl->value_type);
      } else {
        Node *init_decl = init_declarator(specifier, decl);
        decl->declaration = specifier->external || decl->value_type->type == FUNCTION;
        vector_push(declarations, init_decl);
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
  Node *node = node_new();
  node->type = COMP_STMT;
  node->statements = vector_new();

  expect_token(tLBRACE);
  while (1) {
    Token *token = peek_token();
    if (token->type == tRBRACE || token->type == tEND) break;
    if (check_declaration_specifier()) {
      Node *decl = declaration(declaration_specifiers(), NULL);
      vector_push(node->statements, decl);
    } else {
      Node *stmt = statement();
      vector_push(node->statements, stmt);
    }
  }
  expect_token(tRBRACE);

  return node;
}

Node *expression_statement() {
  Node *node = node_new();
  node->type = EXPR_STMT;
  node->expr = expression();
  expect_token(tSEMICOLON);

  return node;
}

Node *selection_statement() {
  Node *node = node_new();

  if (read_token(tIF)) {
    node->type = IF_STMT;
    expect_token(tLPAREN);
    node->control = expression();
    expect_token(tRPAREN);
    node->if_body = statement();
    if (read_token(tELSE)) {
      node->type = IF_ELSE_STMT;
      node->else_body = statement();
    }
  }

  return node;
}

Node *iteration_statement() {
  Node *node = node_new();

  if (read_token(tWHILE)) {
    node->type = WHILE_STMT;
    expect_token(tLPAREN);
    node->control = expression();
    expect_token(tRPAREN);
    node->loop_body = statement();
  } else if (read_token(tDO)) {
    node->type = DO_WHILE_STMT;
    node->loop_body = statement();
    expect_token(tWHILE);
    expect_token(tLPAREN);
    node->control = expression();
    expect_token(tRPAREN);
    expect_token(tSEMICOLON);
  } else if (read_token(tFOR)) {
    node->type = FOR_STMT;
    expect_token(tLPAREN);
    if (check_declaration_specifier()) {
      node->init = declaration(declaration_specifiers(), NULL);
    } else {
      node->init = peek_token()->type != tSEMICOLON ? expression() : NULL;
      expect_token(tSEMICOLON);
    }
    node->control = peek_token()->type != tSEMICOLON ? expression() : NULL;
    expect_token(tSEMICOLON);
    node->afterthrough = peek_token()->type != tRPAREN ? expression() : NULL;
    expect_token(tRPAREN);
    node->loop_body = statement();
  }

  return node;
}

Node *jump_statement() {
  Node *node = node_new();

  if (read_token(tCONTINUE)) {
    node->type = CONTINUE_STMT;
    expect_token(tSEMICOLON);
  } else if (read_token(tBREAK)) {
    node->type = BREAK_STMT;
    expect_token(tSEMICOLON);
  } else if (read_token(tRETURN)) {
    node->type = RETURN_STMT;
    node->expr = peek_token()->type != tSEMICOLON ? expression() : NULL;
    expect_token(tSEMICOLON);
  }

  return node;
}

Node *statement() {
  Node *node;

  TokenType type = peek_token()->type;
  if (type == tLBRACE) {
    node = compound_statement();
  } else if (type == tIF) {
    node = selection_statement();
  } else if (type == tWHILE || type == tDO || type == tFOR) {
    node = iteration_statement();
  } else if (type == tCONTINUE || type == tBREAK || type == tRETURN) {
    node = jump_statement();
  } else {
    node = expression_statement();
  }

  return node;
}

Node *function_definition(Symbol *func_symbol) {
  Node *node = node_new();
  node->type = FUNC_DEF;
  node->identifier = func_symbol->identifier;
  node->symbol = func_symbol;
  node->function_body = compound_statement();

  return node;
}

Node *translate_unit() {
  Vector *definitions = vector_new();

  while (peek_token()->type != tEND) {
    Type *specifier = declaration_specifiers();
    if (peek_token()->type == tSEMICOLON) {
      Node *node = declaration(specifier, NULL);
      vector_push(definitions, node);
    } else {
      Symbol *first_decl = declarator(specifier);
      if (peek_token()->type == tLBRACE) {
        Node *node = function_definition(first_decl);
        vector_push(definitions, node);
      } else {
        Node *node = declaration(specifier, first_decl);
        vector_push(definitions, node);
      }
    }
  }

  Node *node = node_new();
  node->type = TLANS_UNIT;
  node->definitions = definitions;

  return node;
}

Node *parse() {
  tags = map_new();
  typedef_names = map_new();
  enum_constants = map_new();

  return translate_unit();
}
