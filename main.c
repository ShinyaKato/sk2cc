#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

char peek_char() {
  char c = fgetc(stdin);
  ungetc(c, stdin);
  return c;
}

char get_char() {
  return fgetc(stdin);
}

void integer_constant() {
  int n = 0;
  while(peek_char() != EOF) {
    char c = peek_char();
    if(!isdigit(c)) break;
    get_char();
    n = n * 10 + (c - '0');
  }

  printf("  sub $4, %%rsp\n");
  printf("  movl $%d, 0(%%rsp)\n", n);
}

void expression();

void primary_expression() {
  char c = peek_char();
  if(isdigit(c)) {
    integer_constant();
  } else if(c == '(') {
    get_char();
    expression();
    if(get_char() != ')') exit(1);
  } else {
    exit(1);
  }
}

void term() {
  primary_expression();

  while(peek_char() != EOF) {
    char op = peek_char();
    if(op != '*') break;
    get_char();

    primary_expression();

    printf("  movl 0(%%rsp), %%edx\n  add $4, %%rsp\n");
    printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");

    if(op == '*') printf("  imull %%edx\n");

    printf("  sub $4, %%rsp\n  movl %%eax, 0(%%rsp)\n");
  }
}

void expression() {
  term();

  while(peek_char() != EOF) {
    char op = peek_char();
    if(op != '+' && op != '-') break;
    get_char();

    term();

    printf("  movl 0(%%rsp), %%edx\n  add $4, %%rsp\n");
    printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");

    if(op == '+') printf("  addl %%edx, %%eax\n");
    if(op == '-') printf("  subl %%edx, %%eax\n");

    printf("  sub $4, %%rsp\n  movl %%eax, 0(%%rsp)\n");
  }
}

int main(void) {
  printf("  .global main\n");
  printf("main:\n");
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");

  expression();

  printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");

  printf("  pop %%rbp\n");
  printf("  ret\n");

  return 0;
}
