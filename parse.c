#include "cc.h"

Map *symbols;

Symbol *symbol_new() {
  Symbol *symbol = (Symbol *) malloc(sizeof(Symbol));

  return symbol;
}

int symbols_count() {
  return map_count(symbols);
}

bool symbols_put(char *key, Symbol *symbol) {
  return map_put(symbols, key, (void *) symbol);
}

Symbol *symbols_lookup(char *key) {
  return (Symbol *) map_lookup(symbols, key);
}

Node *node_new() {
  Node *node = (Node *) malloc(sizeof(Node));
  return node;
}

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

      if (get_token()->type != tRPAREN) {
        error("tRPAREN is expected.");
      }

      Node *parent = node_new();
      parent->type = FUNC_CALL;
      parent->left = node;

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

    if (!symbols_lookup(unary_exp->identifier)) {
      Symbol *symbol = symbol_new();
      symbol->position = -(symbols_count() * 4 + 4);
      symbols_put(unary_exp->identifier, symbol);
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

Node *expression_statement() {
  Node *node = expression();

  if (get_token()->type != tSEMICOLON) {
    error("tSEMICOLON is expected.");
  }

  return node;
}

Node *parse() {
  Node *node = expression_statement();

  while (peek_token()->type != tEND) {
    Node *parent = node_new();
    parent->type = BLOCK_ITEM;
    parent->left = node;
    parent->right = expression_statement();

    node = parent;
  }

  return node;
}

void parse_init() {
  symbols = map_new();
}
