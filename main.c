#include "cc.h"

enum token_type {
  tINT,
  tIDENTIFIER,
  tNOT,
  tLNOT,
  tADD,
  tSUB,
  tMUL,
  tDIV,
  tMOD,
  tLSHIFT,
  tRSHIFT,
  tLT,
  tGT,
  tLTE,
  tGTE,
  tEQ,
  tNEQ,
  tAND,
  tOR,
  tXOR,
  tLAND,
  tLOR,
  tQUESTION,
  tCOLON,
  tASSIGN,
  tSEMICOLON,
  tLPAREN,
  tRPAREN,
  tEND
};

struct token {
  enum token_type type;
  int int_value;
  char *identifier;
};
typedef struct token Token;

Token *token_new() {
  Token *token = (Token *) malloc(sizeof(Token));
  return token;
}

Token *token_end;

Token *lex() {
  while (1) {
    char c = peek_char();
    if (c != ' ' && c != '\n') break;
    get_char();
  }

  if (peek_char() == EOF) {
    return token_end;
  }

  Token *token = token_new();

  char c = get_char();
  if (isdigit(c)) {
    int n = c - '0';
    while (1) {
      char d = peek_char();
      if (!isdigit(d)) break;
      get_char();
      n = n * 10 + (d - '0');
    }
    token->type = tINT;
    token->int_value = n;
  }  else if (isalpha(c)) {
    String *identifier = string_new();
    string_push(identifier, c);
    while (1) {
      char d = peek_char();
      if (!isalnum(d)) break;
      get_char();
      string_push(identifier, d);
    }
    token->type = tIDENTIFIER;
    token->identifier = identifier->buffer;
  } else if (c == '~') {
    token->type = tNOT;
  } else if (c == '+') {
    token->type = tADD;
  } else if (c == '-') {
    token->type = tSUB;
  } else if (c == '*') {
    token->type = tMUL;
  } else if (c == '/') {
    token->type = tDIV;
  } else if (c == '%') {
    token->type = tMOD;
  } else if (c == '<') {
    char d = peek_char();
    if (d == '=') {
      token->type = tLTE;
      get_char();
    } else if (d == '<') {
      token->type = tLSHIFT;
      get_char();
    } else {
      token->type = tLT;
    }
  } else if (c == '>') {
    char d = peek_char();
    if (d == '=') {
      token->type = tGTE;
      get_char();
    } else if (d == '>') {
      token->type = tRSHIFT;
      get_char();
    } else {
      token->type = tGT;
    }
  } else if (c == '=') {
    if (peek_char() == '=') {
      token->type = tEQ;
      get_char();
    } else {
      token->type = tASSIGN;
    }
  } else if (c == '!') {
    if (peek_char() == '=') {
      token->type = tNEQ;
      get_char();
    } else {
      token->type = tLNOT;
    }
  } else if (c == '&') {
    if (peek_char() == '&') {
      token->type = tLAND;
      get_char();
    } else {
      token->type = tAND;
    }
  } else if (c == '|') {
    if (peek_char() == '|') {
      token->type = tLOR;
      get_char();
    } else {
      token->type = tOR;
    }
  } else if (c == '^') {
    token->type = tXOR;
  } else if (c == '?') {
    token->type = tQUESTION;
  } else if (c == ':') {
    token->type = tCOLON;
  } else if (c == ';') {
    token->type = tSEMICOLON;
  } else if (c == '(') {
    token->type = tLPAREN;
  } else if (c == ')') {
    token->type = tRPAREN;
  } else {
    error("unexpected character.");
  }

  return token;
}

bool has_next_token = false;
Token *next_token;

Token *peek_token() {
  if (has_next_token) {
    return next_token;
  }
  has_next_token = true;
  return next_token = lex();
}

Token *get_token() {
  if (has_next_token) {
    has_next_token = false;
    return next_token;
  }
  return lex();
}

void lex_init() {
  token_end = token_new();
  token_end->type = tEND;
}

struct symbol {
  int position;
};
typedef struct symbol Symbol;

Symbol *symbol_new() {
  Symbol *symbol = (Symbol *) malloc(sizeof(Symbol));

  return symbol;
}

Map *symbols;

int symbols_count() {
  return map_count(symbols);
}

bool symbols_put(char *key, Symbol *symbol) {
  return map_put(symbols, key, (void *) symbol);
}

Symbol *symbols_lookup(char *key) {
  return (Symbol *) map_lookup(symbols, key);
}

void symbols_init() {
  symbols = map_new();
}

enum node_type {
  CONST,
  IDENTIFIER,
  UPLUS,
  UMINUS,
  NOT,
  LNOT,
  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
  LSHIFT,
  RSHIFT,
  LT,
  GT,
  LTE,
  GTE,
  EQ,
  NEQ,
  AND,
  OR,
  XOR,
  LAND,
  LOR,
  CONDITION,
  ASSIGN,
  BLOCK_ITEM
};

struct node {
  enum node_type type;
  int int_value;
  char *identifier;
  struct node *condition;
  struct node *left;
  struct node *right;
};
typedef struct node Node;

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
    node = primary_expression();
  }

  return node;
}

Node *multiplicative_expression(Node *unary_exp) {
  Node *node = unary_exp;

  while (1) {
    Token *op = peek_token();
    enum node_type type;
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
    enum node_type type;
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
    enum node_type type;
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
    enum node_type type;
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
    enum node_type type;
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
    enum node_type type;
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
    enum node_type type;
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
    enum node_type type;
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
    enum node_type type;
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
    enum node_type type;
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

void generate_immediate(int value) {
  printf("  sub $4, %%rsp\n");
  printf("  movl $%d, 0(%%rsp)\n", value);
}

void generate_push(char *reg) {
  printf("  sub $4, %%rsp\n");
  printf("  movl %%%s, 0(%%rsp)\n", reg);
}

void generate_pop(char *reg) {
  printf("  movl 0(%%rsp), %%%s\n", reg);
  printf("  add $4, %%rsp\n");
}

int label_no = 0;

void generate_expression(Node *node) {
  if (node->type == CONST) {
    generate_immediate(node->int_value);
  } else if (node->type == IDENTIFIER) {
    int pos = symbols_lookup(node->identifier)->position;
    printf("  movl %d(%%rbp), %%eax\n", pos);
    generate_push("eax");
  } else if (node->type == LAND) {
    int label_false = label_no++;
    int label_end = label_no++;
    generate_expression(node->left);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  je .L%d\n", label_false);
    generate_expression(node->right);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  je .L%d\n", label_false);
    printf("  movl $1, %%eax\n");
    printf("  jmp .L%d\n", label_end);
    printf(".L%d:\n", label_false);
    printf("  movl $0, %%eax\n");
    printf(".L%d:\n", label_end);
    generate_push("eax");
  } else if (node->type == LOR) {
    int label_true = label_no++;
    int label_end = label_no++;
    generate_expression(node->left);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  jne .L%d\n", label_true);
    generate_expression(node->right);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  jne .L%d\n", label_true);
    printf("  movl $0, %%eax\n");
    printf("  jmp .L%d\n", label_end);
    printf(".L%d:\n", label_true);
    printf("  movl $1, %%eax\n");
    printf(".L%d:\n", label_end);
    generate_push("eax");
  } else if (node->type == CONDITION) {
    int label_false = label_no++;
    int label_end = label_no++;
    generate_expression(node->condition);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  je .L%d\n", label_false);
    generate_expression(node->left);
    printf("  jmp .L%d\n", label_end);
    printf(".L%d:\n", label_false);
    generate_expression(node->right);
    printf(".L%d:\n", label_end);
  } else if (node->type == UPLUS) {
    generate_expression(node->left);
  } else if (node->type == UMINUS) {
    generate_expression(node->left);
    generate_pop("eax");
    printf("  negl %%eax\n");
    generate_push("eax");
  } else if (node->type == NOT) {
    generate_expression(node->left);
    generate_pop("eax");
    printf("  notl %%eax\n");
    generate_push("eax");
  } else if (node->type == LNOT) {
    generate_expression(node->left);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  sete %%al\n");
    printf("  movzbl %%al, %%eax\n");
    generate_push("eax");
  } else if (node->type == ASSIGN) {
    int pos = symbols_lookup(node->left->identifier)->position;
    generate_expression(node->right);
    generate_pop("eax");
    printf("  movl %%eax, %d(%%rbp)\n", pos);
    generate_push("eax");
  } else {
    generate_expression(node->left);
    generate_expression(node->right);
    generate_pop("ecx");
    generate_pop("eax");
    if (node->type == ADD) {
      printf("  addl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->type == SUB) {
      printf("  subl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->type == MUL) {
      printf("  imull %%ecx\n");
      generate_push("eax");
    } else if (node->type == DIV) {
      printf("  movl $0, %%edx\n");
      printf("  idivl %%ecx\n");
      generate_push("eax");
    } else if (node->type == MOD) {
      printf("  movl $0, %%edx\n");
      printf("  idivl %%ecx\n");
      generate_push("edx");
    } else if (node->type == LSHIFT) {
      printf("  sall %%cl, %%eax\n");
      generate_push("eax");
    } else if (node->type == RSHIFT) {
      printf("  sarl %%cl, %%eax\n");
      generate_push("eax");
    } else if (node->type == LT) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setl %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == GT) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setg %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == LTE) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setle %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == GTE) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setge %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == EQ) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  sete %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == NEQ) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setne %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == AND) {
      printf("  andl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->type == OR) {
      printf("  orl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->type == XOR) {
      printf("  xorl %%ecx, %%eax\n");
      generate_push("eax");
    }
  }
}

void generate_block_items(Node *node) {
  if (node->type == BLOCK_ITEM) {
    generate_block_items(node->left);
    generate_pop("eax");
    generate_expression(node->right);
  } else {
    generate_expression(node);
  }
}

void generate(Node *node) {
  printf("  .global main\n");
  printf("main:\n");
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");

  printf("  sub $%d, %%rsp\n", 4 * symbols_count());

  generate_block_items(node);

  generate_pop("eax");

  printf("  add $%d, %%rsp\n", 4 * symbols_count());

  printf("  pop %%rbp\n");
  printf("  ret\n");
}

int main(void) {
  lex_init();
  symbols_init();

  Node *node = parse();
  generate(node);

  return 0;
}
