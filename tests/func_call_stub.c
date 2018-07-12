#include <stdio.h>

int func_call(void) {
  printf("in func_call function.\n");
  return 0;
}

int func_arg_1(int a) {
  printf("%d\n", a);
  return 0;
}

int func_arg_2(int a, int b) {
  printf("%d %d\n", a, b);
  return 0;
}

int func_arg_3(int a, int b, int c) {
  printf("%d %d %d\n", a, b, c);
  return 0;
}

int func_arg_4(int a, int b, int c, int d) {
  printf("%d %d %d %d\n", a, b, c, d);
  return 0;
}

int func_arg_5(int a, int b, int c, int d, int e) {
  printf("%d %d %d %d %d\n", a, b, c, d, e);
  return 0;
}

int func_arg_6(int a, int b, int c, int d, int e, int f) {
  printf("%d %d %d %d %d %d\n", a, b, c, d, e, f);
  return 0;
}

int func_retval(int x) {
  return x * x;
}
