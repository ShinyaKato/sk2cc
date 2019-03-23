#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

int stub_arg1(int a) {
  assert(a == 1);

  return a * a;
}

int stub_arg2(int a, int b) {
  assert(a == 1);
  assert(b == 2);

  return a * a + b * b;
}

int stub_arg3(int a, int b, int c) {
  assert(a == 1);
  assert(b == 2);
  assert(c == 3);

  return a * a + b * b + c * c;
}

int stub_arg4(int a, int b, int c, int d) {
  assert(a == 1);
  assert(b == 2);
  assert(c == 3);
  assert(d == 4);

  return a * a + b * b + c * c + d * d;
}

int stub_arg5(int a, int b, int c, int d, int e) {
  assert(a == 1);
  assert(b == 2);
  assert(c == 3);
  assert(d == 4);
  assert(e == 5);

  return a * a + b * b + c * c + d * d + e * e;
}

int stub_arg6(int a, int b, int c, int d, int e, int f) {
  assert(a == 1);
  assert(b == 2);
  assert(c == 3);
  assert(d == 4);
  assert(e == 5);
  assert(f == 6);

  return a * a + b * b + c * c + d * d + e * e + f * f;
}

int stub_arg7(int a, int b, int c, int d, int e, int f, int g) {
  assert(a == 1);
  assert(b == 2);
  assert(c == 3);
  assert(d == 4);
  assert(e == 5);
  assert(f == 6);
  assert(g == 7);

  return a * a + b * b + c * c + d * d + e * e + f * f + g * g;
}

int stub_arg8(int a, int b, int c, int d, int e, int f, int g, int h) {
  assert(a == 1);
  assert(b == 2);
  assert(c == 3);
  assert(d == 4);
  assert(e == 5);
  assert(f == 6);
  assert(g == 7);
  assert(h == 8);

  return a * a + b * b + c * c + d * d + e * e + f * f + g * g + h * h;
}

bool stub_bool(int b) {
  return b;
}

struct stub_struct {
  char c;
  int n;
  int a[4];
  struct {
    int x, y, z;
  } v;
};

void stub_struct(struct stub_struct *s) {
  assert(s->c == 'A');
  assert(s->n == 45);
  assert(s->a[0] == 5);
  assert(s->a[1] == 3);
  assert(s->a[2] == 8);
  assert(s->a[3] == 7);
  assert(s->v.x == 67);
  assert(s->v.y == 1);
  assert(s->v.z == -41);

  s->c = 'Z';
  s->n = 82;
  s->a[0] = 4;
  s->a[1] = 4;
  s->a[2] = 1234;
  s->a[3] = -571;
  s->v.x = 98;
  s->v.y = 12;
  s->v.z = 1;
}
