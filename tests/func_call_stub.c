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

int* alloc() {
  int *p = (int *) calloc(3, sizeof(int));
  p[0] = 53;
  p[1] = 29;
  p[2] = 64;
  return p;
}

struct test_struct {
  char c;
  int n;
  int a[4];
  struct {
    int x, y, z;
  } v;
};

int test_struct(struct test_struct *s) {
  if (s->c != 'A') return 1;
  if (s->n != 45) return 1;
  if (s->a[0] != 5) return 1;
  if (s->a[1] != 3) return 1;
  if (s->a[2] != 8) return 1;
  if (s->a[3] != 7) return 1;
  if (s->v.x != 67) return 1;
  if (s->v.y != 1) return 1;
  if (s->v.z != -41) return 1;

  s->c = 'Z';
  s->n = 82;
  s->a[0] = 4;
  s->a[1] = 4;
  s->a[2] = 1234;
  s->a[3] = -571;
  s->v.x = 98;
  s->v.y = 12;
  s->v.z = 1;

  return 0;
}

_Bool bool_ret(int b) {
  return b;
}
