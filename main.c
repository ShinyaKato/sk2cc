#include <stdio.h>
#include <stdlib.h>

void get_int() {
  int n = 0;
  while(!feof(stdin)) {
    char c = fgetc(stdin);
    if(!('0' <= c && c <= '9')) {
      ungetc(c, stdin);
      break;
    }
    n = n * 10 + (c - '0');
  }

  printf("  sub $4, %%rsp\n");
  printf("  movl $%d, 0(%%rsp)\n", n);
}

void term() {
  get_int();

  while(!feof(stdin)) {
    char op = fgetc(stdin);
    if(op != '*') {
      ungetc(op, stdin);
      break;
    }

    get_int();

    printf("  movl 0(%%rsp), %%edx\n  add $4, %%rsp\n");
    printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");

    if(op == '*') {
      printf("  mull %%edx\n");
    }

    printf("  sub $4, %%rsp\n  movl %%eax, 0(%%rsp)\n");
  }
}

void expression() {
  term();

  while(!feof(stdin)) {
    char op = fgetc(stdin);
    if(op != '+' && op != '-') {
      ungetc(op, stdin);
      break;
    }

    term();

    printf("  movl 0(%%rsp), %%edx\n  add $4, %%rsp\n");
    printf("  movl 0(%%rsp), %%eax\n  add $4, %%rsp\n");

    if(op == '+') {
      printf("  addl %%edx, %%eax\n");
    } else if(op == '-') {
      printf("  subl %%edx, %%eax\n");
    }

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
