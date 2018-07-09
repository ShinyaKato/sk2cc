#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

void error(char *message) {
  fprintf(stderr, "error: %s\n", message);
  exit(1);
}

char peek_char() {
  char c = fgetc(stdin);
  ungetc(c, stdin);
  return c;
}

char get_char() {
  return fgetc(stdin);
}

enum token_type {
  tINT,
  tADD,
  tSUB,
  tMUL,
  tDIV,
  tMOD,
  tLT,
  tGT,
  tLTE,
  tGTE,
  tEQ,
  tNEQ,
  tLAND,
  tLOR,
  tLPAREN,
  tRPAREN,
  tEND
};

struct token {
  enum token_type type;
  int int_value;
};
typedef struct token Token;

Token lex() {
  Token token;

  while (1) {
    char c = peek_char();
    if (c != ' ' && c != '\n') break;
    get_char();
  }

  if (peek_char() == EOF) {
    token.type = tEND;
    return token;
  }

  char c = get_char();
  if (isdigit(c)) {
    int n = c - '0';
    while (1) {
      char d = peek_char();
      if (!isdigit(d)) break;
      get_char();
      n = n * 10 + (d - '0');
    }
    token.type = tINT;
    token.int_value = n;
  } else if (c == '+') {
    token.type = tADD;
  } else if (c == '-') {
    token.type = tSUB;
  } else if (c == '*') {
    token.type = tMUL;
  } else if (c == '/') {
    token.type = tDIV;
  } else if (c == '%') {
    token.type = tMOD;
  } else if (c == '<') {
    if (peek_char() == '=') {
      token.type = tLTE;
      get_char();
    } else {
      token.type = tLT;
    }
  } else if (c == '>') {
    if (peek_char() == '=') {
      token.type = tGTE;
      get_char();
    } else {
      token.type = tGT;
    }
  } else if (c == '=') {
    if (peek_char() == '=') {
      token.type = tEQ;
      get_char();
    }
  } else if (c == '!') {
    if (peek_char() == '=') {
      token.type = tNEQ;
      get_char();
    }
  } else if (c == '&') {
    if (peek_char() == '&') {
      token.type = tLAND;
      get_char();
    }
  } else if (c == '|') {
    if (peek_char() == '|') {
      token.type = tLOR;
      get_char();
    }
  } else if (c == '(') {
    token.type = tLPAREN;
  } else if (c == ')') {
    token.type = tRPAREN;
  } else {
    error("unexpected character.");
  }

  return token;
}

bool has_next_token = false;
Token next_token;

Token peek_token() {
  if (has_next_token) {
    return next_token;
  }
  has_next_token = true;
  return next_token = lex();
}

Token get_token() {
  if (has_next_token) {
    has_next_token = false;
    return next_token;
  }
  return lex();
}

struct node {
  Token token;
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
  Token token = get_token();
  Node *node;

  if (token.type == tINT) {
    node = node_new();
    node->token = token;
  } else if (token.type == tLPAREN) {
    node = expression();
    if (get_token().type != tRPAREN) {
      error("tRPAREN is expected.");
    }
  } else {
    error("unexpected primary expression.");
  }

  return node;
}

Node *multiplicative_expression() {
  Node *node = primary_expression();

  while (1) {
    Token op = peek_token();
    if (op.type != tMUL && op.type != tDIV && op.type != tMOD) break;
    get_token();

    Node *parent = node_new();
    parent->token = op;
    parent->left = node;
    parent->right = primary_expression();

    node = parent;
  }

  return node;
}

Node *additive_expression() {
  Node *node = multiplicative_expression();

  while (1) {
    Token op = peek_token();
    if (op.type != tADD && op.type != tSUB) break;
    get_token();

    Node *parent = node_new();
    parent->token = op;
    parent->left = node;
    parent->right = multiplicative_expression();

    node = parent;
  }

  return node;
}

Node *relational_expression() {
  Node *node = additive_expression();

  while (1) {
    Token op = peek_token();
    if (op.type != tLT && op.type != tGT && op.type != tLTE && op.type != tGTE) break;
    get_token();

    Node *parent = node_new();
    parent->token = op;
    parent->left = node;
    parent->right = additive_expression();

    node = parent;
  }

  return node;
}

Node *equality_expression() {
  Node *node = relational_expression();

  while (1) {
    Token op = peek_token();
    if (op.type != tEQ && op.type != tNEQ) break;
    get_token();

    Node *parent = node_new();
    parent->token = op;
    parent->left = node;
    parent->right = relational_expression();

    node = parent;
  }

  return node;
}

Node *logical_and_expression() {
  Node *node = equality_expression();

  while (1) {
    Token op = peek_token();
    if (op.type != tLAND) break;
    get_token();

    Node *parent = node_new();
    parent->token = op;
    parent->left = node;
    parent->right = equality_expression();

    node = parent;
  }

  return node;
}

Node *logical_or_expression() {
  Node *node = logical_and_expression();

  while (1) {
    Token op = peek_token();
    if (op.type != tLOR) break;
    get_token();

    Node *parent = node_new();
    parent->token = op;
    parent->left = node;
    parent->right = logical_and_expression();

    node = parent;
  }

  return node;
}

Node *expression() {
  return logical_or_expression();
}

Node *parse() {
  Node *node = expression();

  if (peek_token().type != tEND) {
    error("invalid expression.");
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
  if (node->token.type == tINT) {
    generate_immediate(node->token.int_value);
  } else if (node->token.type == tLAND) {
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
  } else if (node->token.type == tLOR) {
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
  } else {
    generate_expression(node->left);
    generate_expression(node->right);
    generate_pop("ecx");
    generate_pop("eax");
    if (node->token.type == tADD) {
      printf("  addl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->token.type == tSUB) {
      printf("  subl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->token.type == tMUL) {
      printf("  imull %%ecx\n");
      generate_push("eax");
    } else if (node->token.type == tDIV) {
      printf("  movl $0, %%edx\n");
      printf("  idivl %%ecx\n");
      generate_push("eax");
    } else if (node->token.type == tMOD) {
      printf("  movl $0, %%edx\n");
      printf("  idivl %%ecx\n");
      generate_push("edx");
    } else if (node->token.type == tLT) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setl %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->token.type == tGT) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setg %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->token.type == tLTE) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setle %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->token.type == tGTE) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setge %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->token.type == tEQ) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  sete %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->token.type == tNEQ) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setne %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    }
  }
}

void generate(Node *node) {
  printf("  .global main\n");
  printf("main:\n");
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");

  generate_expression(node);

  generate_pop("eax");

  printf("  pop %%rbp\n");
  printf("  ret\n");
}

int main(void) {
  Node *node = parse();
  generate(node);

  return 0;
}
