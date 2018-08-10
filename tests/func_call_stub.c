#include <stdio.h>
#include <stdlib.h>

int func_arg1(int a) {
  return a * a;
}

int func_arg2(int a, int b) {
  return a * a + b * b;
}

int func_arg3(int a, int b, int c) {
  return a * a + b * b + c * c;
}

int func_arg4(int a, int b, int c, int d) {
  return a * a + b * b + c * c + d * d;
}

int func_arg5(int a, int b, int c, int d, int e) {
  return a * a + b * b + c * c + d * d + e * e;
}

int func_arg6(int a, int b, int c, int d, int e, int f) {
  return a * a + b * b + c * c + d * d + e * e + f * f;
}

void alloc(int **p) {
  *p = (int *) calloc(3, sizeof(int));
  (*p)[0] = 53;
  (*p)[1] = 29;
  (*p)[2] = 64;
}
