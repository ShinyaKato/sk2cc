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
    if (get_token()->type != tRPAREN) {
      error("tRPAREN is expected.");
    }
  } else {
    error("unexpected primary expression.");
  }

  return node;
}

Node *postfix_expression() {
  Node *node = primary_expression();

  while (1) {
    Token *token = peek_token();
    if (token->type == tLPAREN) {
      get_token();

      if (node->type != IDENTIFIER) {
        error("unexpected function call.");
      }

      Node *parent = node_new();
      parent->type = FUNC_CALL;
      parent->left = node;
      parent->args_count = 0;

      if (peek_token()->type != tRPAREN) {
        parent->args[0] = assignment_expression();
        parent->args_count++;

        while (peek_token()->type == tCOMMA) {
          get_token();

          if (parent->args_count >= 6) {
            error("too many arguments.");
          }

          parent->args[parent->args_count++] = assignment_expression();
        }
      }

      if (get_token()->type != tRPAREN) {
        error("tRPAREN is expected.");
      }

      node = parent;
    } else {
      break;
    }
  }

  return node;
}

Node *unary_expression() {
  Token *token = peek_token();
  Node *node;

  if (token->type == tADD) {
    get_token();
    node = node_new();
    node->type = UPLUS;
    node->left = unary_expression();
  } else if (token->type == tSUB) {
    get_token();
    node = node_new();
    node->type = UMINUS;
    node->left = unary_expression();
  } else if (token->type == tNOT) {
    get_token();
    node = node_new();
    node->type = NOT;
    node->left = unary_expression();
  } else if (token->type == tLNOT) {
    get_token();
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
    Token *op = peek_token();
    NodeType type;
    if (op->type == tMUL) type = MUL;
    else if (op->type == tDIV) type = DIV;
    else if (op->type == tMOD) type = MOD;
    else break;
    get_token();

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
    Token *op = peek_token();
    NodeType type;
    if (op->type == tADD) type = ADD;
    else if (op->type == tSUB) type = SUB;
    else break;
    get_token();

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
    Token *op = peek_token();
    NodeType type;
    if (op->type == tLSHIFT) type = LSHIFT;
    else if (op->type == tRSHIFT) type = RSHIFT;
    else break;
    get_token();

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
    Token *op = peek_token();
    NodeType type;
    if (op->type == tLT) type = LT;
    else if (op->type == tGT) type = GT;
    else if (op->type == tLTE) type = LTE;
    else if (op->type == tGTE) type = GTE;
    else break;
    get_token();

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
    Token *op = peek_token();
    NodeType type;
    if (op->type == tEQ) type = EQ;
    else if (op->type == tNEQ) type = NEQ;
    else break;
    get_token();

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

  while (1) {
    Token *op = peek_token();
    NodeType type;
    if (op->type == tAND) type = AND;
    else break;
    get_token();

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = equality_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *exclusive_or_expression(Node *unary_exp) {
  Node *node = and_expression(unary_exp);

  while (1) {
    Token *op = peek_token();
    NodeType type;
    if (op->type == tXOR) type = XOR;
    else break;
    get_token();

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = and_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *inclusive_or_expression(Node *unary_exp) {
  Node *node = exclusive_or_expression(unary_exp);

  while (1) {
    Token *op = peek_token();
    NodeType type;
    if (op->type == tOR) type = OR;
    else break;
    get_token();

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = exclusive_or_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *logical_and_expression(Node *unary_exp) {
  Node *node = inclusive_or_expression(unary_exp);

  while (1) {
    Token *op = peek_token();
    NodeType type;
    if (op->type == tLAND) type = LAND;
    else break;
    get_token();

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = inclusive_or_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *logical_or_expression(Node *unary_exp) {
  Node *node = logical_and_expression(unary_exp);

  while (1) {
    Token *op = peek_token();
    NodeType type;
    if (op->type == tLOR) type = LOR;
    else break;
    get_token();

    Node *parent = node_new();
    parent->type = type;
    parent->left = node;
    parent->right = logical_and_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *conditional_expression(Node *unary_exp) {
  Node *node = logical_or_expression(unary_exp);

  if (peek_token()->type == tQUESTION) {
    get_token();

    Node *parent = node_new();
    parent->type = CONDITION;
    parent->condition = node;
    parent->left = expression();
    if (get_token()->type != tCOLON) {
      error("tCOLON is expected.");
    }
    parent->right = conditional_expression(unary_expression());

    node = parent;
  }

  return node;
}

Node *assignment_expression() {
  Node *node;

  Node *unary_exp = unary_expression();
  if (peek_token()->type == tASSIGN) {
    get_token();

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

  if (get_token()->type != tLBRACE) {
    error("tLBRACE is expected.");
  }

  while (1) {
    Token *token = peek_token();
    if (token->type == tRBRACE || token->type == tEND) break;

    Node *stmt = statement();
    vector_push(node->statements, (void *) stmt);
  }

  if (get_token()->type != tRBRACE) {
    error("tRBRACE is expected.");
  }

  return node;
}

Node *expression_statement() {
  Node *expr = expression();
  if (get_token()->type != tSEMICOLON) {
    error("tSEMICOLON is expected.");
  }

  Node *node = node_new();
  node->type = EXPR_STMT;
  node->left = expr;

  return node;
}

Node *selection_statement() {
  Node *node = node_new();
  node->type = IF_STMT;

  get_token();
  if (get_token()->type != tLPAREN) {
    error("tLPAREN is expected.");
  }
  node->condition = expression();
  if (get_token()->type != tRPAREN) {
    error("tRPAREN is expected.");
  }
  node->left = statement();

  if (peek_token()->type == tELSE) {
    get_token();
    node->type = IF_ELSE_STMT;
    node->right = statement();
  }

  return node;
}

Node *statement() {
  Node *node;

  Token *token = peek_token();
  if (token->type == tLBRACE) {
    node = compound_statement();
  } else if (token->type == tIF) {
    node = selection_statement();
  } else {
    node = expression_statement();
  }

  return node;
}

Node *function_definition() {
  Token *id = get_token();
  if (id->type != tIDENTIFIER) {
    error("function definition should begin with tIDENTIFIER.");
  }

  Symbol *func_sym = symbol_new();
  if (map_lookup(func_symbols, id->identifier)) {
    error("duplicated function definition.");
  }
  map_put(func_symbols, id->identifier, (void *) func_sym);

  if (get_token()->type != tLPAREN) {
    error("tLPAREN is expected.");
  }

  int params_count = 0;
  map_clear(symbols);
  if (peek_token()->type != tRPAREN) {
    do {
      if (params_count >= 6) {
        error("too many parameters.");
      }
      Token *token = get_token();
      if (token->type != tIDENTIFIER) {
        error("tIDENTIFIER is expected.");
      }
      if (map_lookup(symbols, token->identifier)) {
        error("duplicated parameter definition.");
      }
      Symbol *param = symbol_new();
      param->position = -(map_count(symbols) * 4 + 4);
      map_put(symbols, token->identifier, param);
      params_count++;
    } while (peek_token()->type == tCOMMA && get_token());
  }

  if (get_token()->type != tRPAREN) {
    error("tRPAREN is expected.");
  }

  Node *comp_stmt = compound_statement();

  Node *node = node_new();
  node->type = FUNC_DEF;
  node->identifier = id->identifier;
  node->left = comp_stmt;
  node->params_count = params_count;
  node->vars_count = map_count(symbols);

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
