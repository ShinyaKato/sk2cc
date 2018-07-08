#include <stdio.h>
#include <stdlib.h>

int get_int() {
  int n = 0;
  while(!feof(stdin)) {
    char c = fgetc(stdin);
    if(!('0' <= c && c <= '9')) {
      ungetc(c, stdin);
      break;
    }
    n = n * 10 + (c - '0');
  }
  return n;
}

int main(void) {
  printf("  .global main\n");
  printf("main:\n");
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");

  int n = get_int();
  printf("  sub $4, %%rsp\n");
  printf("  movl $%d, 0(%%rsp)\n", n);

  while(!feof(stdin)) {
    char op = fgetc(stdin);
    if(op == '\n') break;

    int m = get_int();
    printf("  movl $%d, %%edx\n", m);

    printf("  movl 0(%%rsp), %%eax\n");
    printf("  add $4, %%rsp\n");

    if(op == '+') {
      printf("  addl %%edx, %%eax\n");
    } else if(op == '-') {
      printf("  subl %%edx, %%eax\n");
    } else {
      exit(1);
    }

    printf("  sub $4, %%rsp\n");
    printf("  movl %%eax, 0(%%rsp)\n");
  }

  printf("  movl 0(%%rsp), %%eax\n");
  printf("  add $4, %%rsp\n");

  printf("  pop %%rbp\n");
  printf("  ret\n");

  return 0;
}
