#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

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

  if(peek_char() == '\n') {
    token.type = tEND;
    return token;
  }

  char c = get_char();
  if(isdigit(c)) {
    int n = c - '0';
    while(1) {
      char d = peek_char();
      if(!isdigit(d)) break;
      get_char();
      n = n * 10 + (d - '0');
    }
    token.type = tINT;
    token.int_value = n;
  } else if(c == '+') {
    token.type = tADD;
  } else if(c == '-') {
    token.type = tSUB;
  } else if(c == '*') {
    token.type = tMUL;
  } else if(c == '(') {
    token.type = tLPAREN;
  } else if(c == ')') {
    token.type = tRPAREN;
  } else {
    exit(1);
  }

  return token;
}

bool has_next_token = false;
Token next_token;

Token peek_token() {
  if(has_next_token) {
    return next_token;
  }
  has_next_token = true;
  return next_token = lex();
}

Token get_token() {
  if(has_next_token) {
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

Node *additive_expression();

Node *primary_expression() {
  Token token = get_token();
  Node *node;

  if(token.type == tINT) {
    node = node_new();
    node->token = token;
  } else if(token.type == tLPAREN) {
    node = additive_expression();
    if(get_token().type != tRPAREN) {
      exit(1);
    }
  } else {
    exit(1);
  }

  return node;
}

Node *multiplicative_expression() {
  Node *node = primary_expression();

  while(1) {
    Token op = peek_token();
    if(op.type != tMUL) break;
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

  while(1) {
    Token op = peek_token();
    if(op.type != tADD && op.type != tSUB) break;
    get_token();

    Node *parent = node_new();
    parent->token = op;
    parent->left = node;
    parent->right = multiplicative_expression();

    node = parent;
  }

  return node;
}

Node *parse() {
  return additive_expression();
}

void generate_expression(Node *node) {
  if(node->token.type == tINT) {
    printf("  sub $4, %%rsp\n  movl $%d, 0(%%rsp)\n", node->token.int_value);
  } else {
    generate_expression(node->left);
    generate_expression(node->right);
    printf("  movl 0(%%rsp), %%edx\n  add $4, %%rsp\n");
    printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");
    if(node->token.type == tADD) printf("  addl %%edx, %%eax\n");
    if(node->token.type == tSUB) printf("  subl %%edx, %%eax\n");
    if(node->token.type == tMUL) printf("  imull %%edx\n");
    printf("  sub $4, %%rsp\n  movl %%eax, 0(%%rsp)\n");
  }
}

void generate(Node *node) {
  printf("  .global main\n");
  printf("main:\n");
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");

  generate_expression(node);

  printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");

  printf("  pop %%rbp\n");
  printf("  ret\n");
}

int main(void) {
  Node *node = parse();
  generate(node);

  return 0;
}
