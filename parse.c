#include "cc.h"

Map *struct_tags, *typedef_names;

Symbol *symbol_new() {
  Symbol *symbol = (Symbol *) calloc(1, sizeof(Symbol));
  return symbol;
}

Node *node_new() {
  Node *node = (Node *) calloc(1, sizeof(Node));
  return node;
}

Node *assignment_expression();
Node *expression();

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
    Node *node = node_new();
    node->type = IDENTIFIER;
    node->identifier = token->identifier;
    return node;
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
  NodeType type;
  if (read_token(tINC)) type = PRE_INC;
  else if (read_token(tDEC)) type = PRE_DEC;
  else if (read_token(tAND)) type = ADDRESS;
  else if (read_token(tMUL)) type = INDIRECT;
  else if (read_token(tADD)) type = UPLUS;
  else if (read_token(tSUB)) type = UMINUS;
  else if (read_token(tNOT)) type = NOT;
  else if (read_token(tLNOT)) type = LNOT;
  else if (read_token(tSIZEOF)) type = SIZEOF;
  else return postfix_expression();

  Node *node = node_new();
  node->type = type;
  node->expr = unary_expression();

  return node;
}

Node *multiplicative_expression(Node *unary_exp) {
  Node *node = unary_exp;

  while (1) {
    NodeType type;
    if (read_token(tMUL)) type = MUL;
    else if (read_token(tDIV)) type = DIV;
    else if (read_token(tMOD)) type = MOD;
    else break;

    Node *left = node;
    Node *right = unary_expression();

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *additive_expression(Node *unary_exp) {
  Node *node = multiplicative_expression(unary_exp);

  while (1) {
    NodeType type;
    if (read_token(tADD)) type = ADD;
    else if (read_token(tSUB)) type = SUB;
    else break;

    Node *left = node;
    Node *right = multiplicative_expression(unary_expression());

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *shift_expression(Node *unary_exp) {
  Node *node = additive_expression(unary_exp);

  while (1) {
    NodeType type;
    if (read_token(tLSHIFT)) type = LSHIFT;
    else if (read_token(tRSHIFT)) type = RSHIFT;
    else break;

    Node *left = node;
    Node *right = additive_expression(unary_expression());

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *relational_expression(Node *unary_exp) {
  Node *node = shift_expression(unary_exp);

  while (1) {
    NodeType type;
    if (read_token(tLT)) type = LT;
    else if (read_token(tGT)) type = GT;
    else if (read_token(tLTE)) type = LTE;
    else if (read_token(tGTE)) type = GTE;
    else break;

    Node *left = node;
    Node *right = shift_expression(unary_expression());

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *equality_expression(Node *unary_exp) {
  Node *node = relational_expression(unary_exp);

  while (1) {
    NodeType type;
    if (read_token(tEQ)) type = EQ;
    else if (read_token(tNEQ)) type = NEQ;
    else break;

    Node *left = node;
    Node *right = relational_expression(unary_expression());

    node = node_new();
    node->type = type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *and_expression(Node *unary_exp) {
  Node *node = equality_expression(unary_exp);

  while (read_token(tAND)) {
    Node *left = node;
    Node *right = equality_expression(unary_expression());

    node = node_new();
    node->type = AND;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *exclusive_or_expression(Node *unary_exp) {
  Node *node = and_expression(unary_exp);

  while (read_token(tXOR)) {
    Node *left = node;
    Node *right = and_expression(unary_expression());

    node = node_new();
    node->type = XOR;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *inclusive_or_expression(Node *unary_exp) {
  Node *node = exclusive_or_expression(unary_exp);

  while (read_token(tOR)) {
    Node *left = node;
    Node *right = exclusive_or_expression(unary_expression());

    node = node_new();
    node->type = OR;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *logical_and_expression(Node *unary_exp) {
  Node *node = inclusive_or_expression(unary_exp);

  while (read_token(tLAND)) {
    Node *left = node;
    Node *right = inclusive_or_expression(unary_expression());

    node = node_new();
    node->type = LAND;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *logical_or_expression(Node *unary_exp) {
  Node *node = logical_and_expression(unary_exp);

  while (read_token(tLOR)) {
    Node *left = node;
    Node *right = logical_and_expression(unary_expression());

    node = node_new();
    node->type = LOR;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *conditional_expression(Node *unary_exp) {
  Node *node = logical_or_expression(unary_exp);

  if (read_token(tQUESTION)) {
    Node *control = node;
    Node *left = expression();
    expect_token(tCOLON);
    Node *right = conditional_expression(unary_expression());

    node = node_new();
    node->type = CONDITION;
    node->control = control;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *assignment_expression() {
  Node *unary_exp = unary_expression();

  NodeType type;
  if (read_token(tASSIGN)) type = ASSIGN;
  else if (read_token(tADD_ASSIGN)) type = ADD_ASSIGN;
  else if (read_token(tSUB_ASSIGN)) type = SUB_ASSIGN;
  else return conditional_expression(unary_exp);

  Node *left = unary_exp;
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

bool check_declaration() {
  Token *token = peek_token();
  if (token->type == tCHAR) return true;
  if (token->type == tINT) return true;
  if (token->type == tSTRUCT) return true;
  if (token->type == tTYPEDEF) return true;
  if (token->type == tIDENTIFIER && map_lookup(typedef_names, token->identifier)) return true;
  return false;
}

Type *specifier_qualifier_list() {
  return type_specifier();
}

Type *struct_or_union_specifier() {
  expect_token(tSTRUCT);

  Token *token = optional_token(tIDENTIFIER);
  if (token && !map_lookup(struct_tags, token->identifier)) {
    Type *incomplete_type = type_new();
    incomplete_type->type = STRUCT;
    incomplete_type->incomplete = true;
    map_put(struct_tags, token->identifier, incomplete_type);
  }

  if (!read_token(tLBRACE)) {
    if (!token) error("invalid struct type specifier.");

    Type *type = map_lookup(struct_tags, token->identifier);
    if (!type) error("undefined struct tag.");

    return type;
  }

  Vector *identifiers = vector_new();
  Map *members = map_new();
  do {
    Type *specifier = specifier_qualifier_list();
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

  Type *dest = map_lookup(struct_tags, token->identifier);
  type_copy(dest, type);
  return dest;
}

Type *type_specifier() {
  if (read_token(tCHAR)) {
    return type_char();
  } else if (read_token(tINT)) {
    return type_int();
  } else if (peek_token()->type == tSTRUCT) {
    return struct_or_union_specifier();
  } else if (peek_token()->type == tIDENTIFIER) {
    Token *token = get_token();
    Type *type = map_lookup(typedef_names, token->identifier);
    if (type) return type;
  }
  error("type specifier is expected.");
}

Type *declaration_specifiers() {
  bool definition = read_token(tTYPEDEF);

  Type *specifier = type_specifier();
  specifier->definition = definition;

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
    if (peek_token()->type != tRPAREN) {
      do {
        Type *specifier = declaration_specifiers();
        Symbol *param = declarator(specifier);
        vector_push(params, param);
      } while (read_token(tCOMMA));
    }
    expect_token(tRPAREN);
    return type_function_returning(direct_declarator(type), params);
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

  if (type->incomplete) {
    error("declaration with incomplete type.");
  }

  Symbol *symbol = symbol_new();
  symbol->identifier = token->identifier;
  symbol->value_type = type;

  if (specifier->definition) {
    map_put(typedef_names, token->identifier, type);
  }

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
    Node *first_init_decl = init_declarator(specifier, first_decl);
    if (!specifier->definition) {
      vector_push(declarations, first_init_decl);
    }

    while (read_token(tCOMMA)) {
      Symbol *decl = declarator(specifier);
      Node *init_decl = init_declarator(specifier, decl);
      if (!specifier->definition) {
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
    if (check_declaration()) {
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
    if (check_declaration()) {
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
    if (read_token(tSEMICOLON)) continue;

    Symbol *first_decl = declarator(specifier);
    if (peek_token()->type == tLBRACE) {
      Node *node = function_definition(first_decl);
      vector_push(definitions, node);
    } else {
      Node *node = declaration(specifier, first_decl);
      vector_push(definitions, node);
    }
  }

  Node *node = node_new();
  node->type = TLANS_UNIT;
  node->definitions = definitions;

  return node;
}

Node *parse() {
  struct_tags = map_new();
  typedef_names = map_new();

  return translate_unit();
}
