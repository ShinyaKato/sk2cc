#include "cc.h"

Symbol *symbol_new() {
  Symbol *symbol = (Symbol *) malloc(sizeof(Symbol));
  return symbol;
}

Node *node_new() {
  Node *node = (Node *) malloc(sizeof(Node));

  node->identifier = NULL;
  node->symbol = NULL;
  node->args = NULL;
  node->left = NULL;
  node->right = NULL;
  node->init = NULL;
  node->control = NULL;
  node->afterthrough = NULL;
  node->expr = NULL;
  node->statements = NULL;
  node->if_body = NULL;
  node->else_body = NULL;
  node->loop_body = NULL;
  node->function_body = NULL;
  node->param_symbols = NULL;
  node->definitions = NULL;

  return node;
}

Node *assignment_expression();
Node *expression();

Node *primary_expression() {
  Token *token = get_token();

  if (token->type == tINT_CONST) {
    Node *node = node_new();
    node->type = CONST;
    node->value_type = type_int();
    node->int_value = token->int_value;
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
    } else {
      break;
    }
  }

  return node;
}

Node *unary_expression() {
  NodeType type;
  if (read_token(tAND)) type = ADDRESS;
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
  Node *node;

  Node *unary_exp = unary_expression();
  if (read_token(tASSIGN)) {
    Node *left = unary_exp;
    Node *right = assignment_expression();

    node = node_new();
    node->type = ASSIGN;
    node->value_type = left->value_type;
    node->left = left;
    node->right = right;
  } else {
    node = conditional_expression(unary_exp);
  }

  return node;
}

Node *expression() {
  return assignment_expression();
}

Type *declaration_specifiers() {
  expect_token(tINT);
  return type_int();
}

Type *direct_declarator(Type *type) {
  if (read_token(tLBRACKET)) {
    int size = expect_token(tINT_CONST)->int_value;
    expect_token(tRBRACKET);
    type = type_array_of(direct_declarator(type), size);
  }
  return type;
}

Symbol *declarator(Type *type) {
  while (read_token(tMUL)) {
    type = type_pointer_to(type);
  }

  Token *token = expect_token(tIDENTIFIER);

  type = direct_declarator(type);

  Symbol *symbol = symbol_new();
  symbol->identifier = token->identifier;
  symbol->value_type = type;

  return symbol;
}

Symbol *init_declarator(Type *specifier) {
  return declarator(specifier);
}

Node *declaration(Type *specifier, Symbol *first) {
  Vector *var_symbols = vector_new();
  vector_push(var_symbols, first);
  while (read_token(tCOMMA)) {
    Symbol *symbol = init_declarator(specifier);
    vector_push(var_symbols, symbol);
  }
  expect_token(tSEMICOLON);

  Node *node = node_new();
  node->type = VAR_DECL;
  node->var_symbols = var_symbols;

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
    if (peek_token()->type == tINT) {
      Type *specifier = declaration_specifiers();
      Symbol *symbol = init_declarator(specifier);
      vector_push(node->statements, declaration(specifier, symbol));
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
    node->init = peek_token()->type != tSEMICOLON ? expression() : NULL;
    expect_token(tSEMICOLON);
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
  Vector *param_symbols = vector_new();
  expect_token(tLPAREN);
  if (peek_token()->type != tRPAREN) {
    do {
      expect_token(tINT);
      Type *param_type = type_int();
      while (read_token(tMUL)) {
        param_type = type_pointer_to(param_type);
      }
      Token *token = expect_token(tIDENTIFIER);
      Symbol *symbol = symbol_new();
      symbol->identifier = token->identifier;
      symbol->value_type = param_type;
      vector_push(param_symbols, symbol);
    } while (read_token(tCOMMA));
  }
  expect_token(tRPAREN);

  Node *node = node_new();
  node->type = FUNC_DEF;
  node->identifier = func_symbol->identifier;
  node->symbol = func_symbol;
  node->function_body = compound_statement();
  node->param_symbols = param_symbols;

  return node;
}

Node *external_definition() {
  Type *specifier = declaration_specifiers();
  Symbol *symbol = init_declarator(specifier);

  Node *node;
  if (peek_token()->type == tLPAREN) {
    node = function_definition(symbol);
  } else {
    node = declaration(specifier, symbol);
  }

  return node;
}

Node *translate_unit() {
  Node *node = node_new();
  node->type = TLANS_UNIT;
  node->definitions = vector_new();

  while (peek_token()->type != tEND) {
    Node *def = external_definition();
    vector_push(node->definitions, def);
  }

  return node;
}

Node *parse() {
  return translate_unit();
}
