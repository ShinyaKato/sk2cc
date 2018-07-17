#include "cc.h"

Map *func_symbols, *symbols;

Symbol *symbol_new() {
  Symbol *symbol = (Symbol *) malloc(sizeof(Symbol));
  return symbol;
}

Node *node_new() {
  Node *node = (Node *) malloc(sizeof(Node));
  return node;
}

Node *assignment_expression();
Node *expression();

Node *primary_expression() {
  Token *token = get_token();
  Node *node;

  if (token->type == tINT) {
    node = node_new();
    node->type = CONST;
    node->int_value = token->int_value;
  } else if (token->type == tIDENTIFIER) {
    node = node_new();
    node->type = IDENTIFIER;
    node->identifier = token->identifier;
    if (map_lookup(func_symbols, token->identifier)) {
      node->symbol = (Symbol *) map_lookup(func_symbols, token->identifier);
    } else {
      if (!map_lookup(symbols, token->identifier)) {
        Symbol *symbol = symbol_new();
        symbol->position = -(map_count(symbols) * 4 + 4);
        map_put(symbols, token->identifier, symbol);
      }
      node->symbol = (Symbol *) map_lookup(symbols, token->identifier);
    }
  } else if (token->type == tLPAREN) {
    node = expression();
    expect_token(tRPAREN);
  } else {
    error("unexpected primary expression.");
  }

  return node;
}

Node *postfix_expression() {
  Node *node = primary_expression();

  while (1) {
    if (read_token(tLPAREN)) {
      if (node->type != IDENTIFIER) {
        error("invalid function call.");
      }

      Node *parent = node_new();
      parent->type = FUNC_CALL;
      parent->left = node;
      parent->args_count = 0;
      if (peek_token()->type != tRPAREN) {
        do {
          if (parent->args_count >= 6) {
            error("too many arguments.");
          }
          parent->args[parent->args_count++] = assignment_expression();
        } while (read_token(tCOMMA));
      }
      expect_token(tRPAREN);

      node = parent;
    } else {
      break;
    }
  }

  return node;
}

Node *unary_expression() {
  Node *node;

  if (read_token(tADD)) {
    node = node_new();
    node->type = UPLUS;
    node->left = unary_expression();
  } else if (read_token(tSUB)) {
    node = node_new();
    node->type = UMINUS;
    node->left = unary_expression();
  } else if (read_token(tNOT)) {
    node = node_new();
    node->type = NOT;
    node->left = unary_expression();
  } else if (read_token(tLNOT)) {
    node = node_new();
    node->type = LNOT;
    node->left = unary_expression();
  } else {
    node = postfix_expression();
  }

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

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = unary_expression();

    node = parent;
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

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = multiplicative_expression(unary_expression());

    node = parent;
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

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = additive_expression(unary_expression());

    node = parent;
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

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = shift_expression(unary_expression());

    node = parent;
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

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = relational_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *and_expression(Node *unary_exp) {
  Node *node = equality_expression(unary_exp);

  while (read_token(tAND)) {
    Node *parent = node_new();
    parent->type = AND;
    parent->left = node;
    parent->right = equality_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *exclusive_or_expression(Node *unary_exp) {
  Node *node = and_expression(unary_exp);

  while (read_token(tXOR)) {
    Node *parent = node_new();
    parent->type = XOR;
    parent->left = node;
    parent->right = and_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *inclusive_or_expression(Node *unary_exp) {
  Node *node = exclusive_or_expression(unary_exp);

  while (read_token(tOR)) {
    Node *parent = node_new();
    parent->type = OR;
    parent->left = node;
    parent->right = exclusive_or_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *logical_and_expression(Node *unary_exp) {
  Node *node = inclusive_or_expression(unary_exp);

  while (read_token(tLAND)) {
    Node *parent = node_new();
    parent->type = LAND;
    parent->left = node;
    parent->right = inclusive_or_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *logical_or_expression(Node *unary_exp) {
  Node *node = logical_and_expression(unary_exp);

  while (read_token(tLOR)) {
    Node *parent = node_new();
    parent->type = LOR;
    parent->left = node;
    parent->right = logical_and_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *conditional_expression(Node *unary_exp) {
  Node *node = logical_or_expression(unary_exp);

  if (read_token(tQUESTION)) {
    Node *parent = node_new();
    parent->type = CONDITION;
    parent->control = node;
    parent->left = expression();
    expect_token(tCOLON);
    parent->right = conditional_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *assignment_expression() {
  Node *node;

  Node *unary_exp = unary_expression();
  if (read_token(tASSIGN)) {
    if (unary_exp->type != IDENTIFIER) {
      error("left side of assignment operator should be identifier.");
    }

    node = node_new();
    node->type = ASSIGN;
    node->left = unary_exp;
    node->right = assignment_expression();
  } else {
    node = conditional_expression(unary_exp);
  }

  return node;
}

Node *expression() {
  return assignment_expression();
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
    Node *stmt = statement();
    vector_push(node->statements, (void *) stmt);
  }
  expect_token(tRBRACE);

  return node;
}

Node *expression_statement() {
  Node *node = node_new();
  node->type = EXPR_STMT;
  node->expression = expression();
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

Node *statement() {
  Node *node;

  if (peek_token()->type == tLBRACE) {
    node = compound_statement();
  } else if (peek_token()->type == tIF) {
    node = selection_statement();
  } else if (peek_token()->type == tWHILE) {
    node = iteration_statement();
  } else if (peek_token()->type == tFOR) {
    node = iteration_statement();
  } else {
    node = expression_statement();
  }

  return node;
}

Node *function_definition() {
  int params_count = 0;
  map_clear(symbols);

  Token *id = expect_token(tIDENTIFIER);
  Symbol *func_sym = symbol_new();
  if (map_lookup(func_symbols, id->identifier)) {
    error("duplicated function definition.");
  }
  map_put(func_symbols, id->identifier, (void *) func_sym);

  expect_token(tLPAREN);
  if (peek_token()->type != tRPAREN) {
    do {
      if (params_count >= 6) {
        error("too many parameters.");
      }
      Token *token = expect_token(tIDENTIFIER);
      if (map_lookup(symbols, token->identifier)) {
        error("duplicated parameter definition.");
      }
      Symbol *param = symbol_new();
      param->position = -(map_count(symbols) * 4 + 4);
      map_put(symbols, token->identifier, param);
      params_count++;
    } while (read_token(tCOMMA));
  }
  expect_token(tRPAREN);

  Node *node = node_new();
  node->type = FUNC_DEF;
  node->identifier = id->identifier;
  node->function_body = compound_statement();
  node->vars_count = map_count(symbols);
  node->params_count = params_count;

  return node;
}

Node *translate_unit() {
  Node *node = node_new();
  node->type = TLANS_UNIT;
  node->definitions = vector_new();

  while (peek_token()->type != tEND) {
    Node *def = function_definition();
    vector_push(node->definitions, (void *) def);
  }

  return node;
}

Node *parse() {
  return translate_unit();
}

void parse_init() {
  func_symbols = map_new();
  symbols = map_new();
}
