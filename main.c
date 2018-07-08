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

void additive_expression();

void primary_expression() {
  Token token = get_token();

  if(token.type == tINT) {
    printf("  sub $4, %%rsp\n  movl $%d, 0(%%rsp)\n", token.int_value);
  } else if(token.type == tLPAREN) {
    additive_expression();
    if(get_token().type != tRPAREN) {
      exit(1);
    }
  } else {
    exit(1);
  }
}

void multiplicative_expression() {
  primary_expression();

  while(1) {
    Token op = peek_token();
    if(op.type != tMUL) break;
    get_token();

    primary_expression();

    printf("  movl 0(%%rsp), %%edx\n  add $4, %%rsp\n");
    printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");

    if(op.type == tMUL) printf("  imull %%edx\n");

    printf("  sub $4, %%rsp\n  movl %%eax, 0(%%rsp)\n");
  }
}

void additive_expression() {
  multiplicative_expression();

  while(1) {
    Token op = peek_token();
    if(op.type != tADD && op.type != tSUB) break;
    get_token();

    multiplicative_expression();

    printf("  movl 0(%%rsp), %%edx\n  add $4, %%rsp\n");
    printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");

    if(op.type == tADD) printf("  addl %%edx, %%eax\n");
    if(op.type == tSUB) printf("  subl %%edx, %%eax\n");

    printf("  sub $4, %%rsp\n  movl %%eax, 0(%%rsp)\n");
  }
}

int main(void) {
  printf("  .global main\n");
  printf("main:\n");
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");

  additive_expression();

  printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");

  printf("  pop %%rbp\n");
  printf("  ret\n");

  return 0;
}
