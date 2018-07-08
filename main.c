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
  printf("  push $%d\n", n);

  while(!feof(stdin)) {
    char op = fgetc(stdin);
    if(op == '\n') break;
    if(op != '+') exit(1);

    int m = get_int();
    printf("  mov $%d, %%eax\n", m);
    printf("  pop %%rdx\n");
    printf("  add %%edx, %%eax\n");
    printf("  push %%rax\n");
  }

  printf("  pop %%rax\n");

  printf("  pop %%rbp\n");
  printf("  ret\n");

  return 0;
}
