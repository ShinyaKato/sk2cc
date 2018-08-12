struct test_struct {
  char c;
  int n;
  int a[4];
  struct {
    int x, y, z;
  } v;
};

int func_arg1(int a);
int func_arg2(int a, int b);
int func_arg3(int a, int b, int c);
int func_arg4(int a, int b, int c, int d);
int func_arg5(int a, int b, int c, int d, int e);
int func_arg6(int a, int b, int c, int d, int e, int f);
int *alloc();
int test_struct(struct test_struct *s);

#define test(expr, expected) \
  do { \
    int actual = (expr); \
    if (actual != (expected)) { \
      printf("\"%s\" should be %d, but got %d.\n", #expr, (expected), actual); \
      return 1; \
    } \
  } while (0)

int main() {
  test(2 + 3, 5);
  test(123 + 43 + 1 + 21, 188);

  test(11 - 7, 4);
  test(123 - 20 - 3, 100);
  test(123 + 20 - 3 + 50 - 81, 109);

  test(3 * 5, 15);
  test(3 * 5 + 7 * 8 - 3 * 4, 59);

  test((3 - 5) * 3 + 7, 1);
  test((((123))), 123);

  test(6 / 2, 3);
  test(6 / (2 - 3) + 7, 1);
  test(123 % 31, 30);
  test(32 / 4 + 5 * (8 - 5) + 5 % 2, 24);

  test(3 * 5 == 7 + 8, 1);
  test(3 * 5 == 123, 0);
  test(3 * 5 != 7 + 8, 0);
  test(3 * 5 != 123, 1);

  test(5 * 7 < 36, 1);
  test(5 * 7 < 35, 0);
  test(5 * 7 > 35, 0);
  test(5 * 7 > 34, 1);

  test(5 * 7 <= 35, 1);
  test(5 * 7 <= 34, 0);
  test(5 * 7 >= 36, 0);
  test(5 * 7 >= 35, 1);
  test(12 > -1, 1);

  test(1 && 1, 1);
  test(0 && 1, 0);
  test(1 && 0, 0);
  test(0 && 0, 0);
  test(3 * 7 > 20 && 5 < 10 && 6 + 2 <= 3 * 3 && 5 * 2 % 3 == 1, 1);
  test(3 * 7 > 20 && 5 < 10 && 6 + 2 >= 3 * 3 && 5 * 2 % 3 == 1, 0);

  test(1 || 1, 1);
  test(0 || 1, 1);
  test(1 || 0, 1);
  test(0 || 0, 0);
  test(8 * 7 < 50 || 10 > 2 * 5 || 3 % 2 == 1 || 5 * 7 < 32, 1);
  test(8 * 7 < 50 || 10 > 2 * 5 || 3 % 2 != 1 || 5 * 7 < 32, 0);
  test(3 * 7 > 20 && 2 * 4 <= 7 || 8 % 2 == 0 && 5 / 2 >= 2, 1);

  test(+5, 5);
  test(5 + (-5), 0);
  test(3 - + - + - + - 2, 5);

  test(!0, 1);
  test(!1, 0);
  test(!!0, 0);
  test(!(2 * 3 <= 2 + 3), 1);

  test(3 > 2 ? 13 : 31, 13);
  test(3 < 2 ? 13 : 31, 31);
  test(1 ? 1 ? 6 : 7 : 1 ? 8 : 9, 6);
  test(1 ? 0 ? 6 : 7 : 1 ? 8 : 9, 7);
  test(0 ? 1 ? 6 : 7 : 1 ? 8 : 9, 8);
  test(0 ? 1 ? 6 : 7 : 0 ? 8 : 9, 9);

  test(1 << 6, 64);
  test(64 >> 6, 1);
  test(64 >> 8, 0);
  test(41 << 2, 164);
  test(41 >> 3, 5);

  test(183 & 109, 37);
  test(183 | 109, 255);
  test(183 ^ 109, 218);

  test(~183 & 255, 72);

  { int x = 123; test(100 <= x && x < 200, 1); }
  { int x= 3; x = x * x + 1; test(x + 3, 13); }
  { int x; test(x = 2 * 3 * 4, 24); }
  { int x = 2, y = x + 5; test(y, 7); }
  { int x, y, z; test(x = y = z = 3, 3); }
  { int x, y, z; x = (y = (z = 1) + 2) + 3; test(x, 6); }
  { int x, y, z; x = (y = (z = 1) + 2) + 3; test(y, 3); }
  { int x, y, z; x = (y = (z = 1) + 2) + 3; test(z, 1); }

  test(func_arg1(1), 1);
  test(func_arg2(1, 2), 5);
  test(func_arg3(1, 2, 3), 14);
  test(func_arg4(1, 2, 3, 4), 30);
  test(func_arg5(1, 2, 3, 4, 5), 55);
  test(func_arg6(1, 2, 3, 4, 5, 6), 91);

  { int x = 5; if (3 * 4 > 10) { x = 7; } test(x, 7); }
  { int x = 5; if (3 * 4 < 10) { x = 7; } test(x, 5); }
  { int x = 5; if (3 * 4 > 10) if (0) x = 7; test(x, 5); }
  { int x; if (3 * 4 > 10) x = 3; else x = 4; test(x, 3); }
  { int x; if (3 * 4 < 10) x = 3; else x = 4; test(x, 4); }
  { int x; if (0) if (0) x = 3; else x = 2; else if (0) x = 1; else x = 0; test(x, 0); }
  { int x; if (0) if (0) x = 3; else x = 2; else if (1) x = 1; else x = 0; test(x, 1); }
  { int x; if (0) if (1) x = 3; else x = 2; else if (0) x = 1; else x = 0; test(x, 0); }
  { int x; if (0) if (1) x = 3; else x = 2; else if (1) x = 1; else x = 0; test(x, 1); }
  { int x; if (1) if (0) x = 3; else x = 2; else if (0) x = 1; else x = 0; test(x, 2); }
  { int x; if (1) if (0) x = 3; else x = 2; else if (1) x = 1; else x = 0; test(x, 2); }
  { int x; if (1) if (1) x = 3; else x = 2; else if (0) x = 1; else x = 0; test(x, 3); }
  { int x; if (1) if (1) x = 3; else x = 2; else if (1) x = 1; else x = 0; test(x, 3); }

  { int x = 1, s = 0; while (x <= 0) { s += x; x++; } test(s, 0); }
  { int x = 1, s = 0; while (x <= 1) { s += x; x++; } test(s, 1); }
  { int x = 1, s = 0; while (x <= 10) { s += x; x++; } test(s, 55); }
  { int x = 1, s = 0; while (x <= 10) { s += x; x++; } test(s, 55); }
  { int x = 1, s = 0; while (x <= 10) { if (x % 2 == 0) s += x; x++; } test(s, 30); }
  { int x = 1, s = 0; while (x <= 10) { if (x % 2 == 1) s += x; x++; } test(s, 25); }

  { int i = 0, s = 0; do { s += i; i++; } while (i < 10); test(s, 45); }
  { int i = 10, s = 0; do { s += i; i++; } while (i < 10); test(s, 10); }

  { int s = 0, i = 0; for (; i < 10;) { s = s + i; i++; } test(s, 45); }
  { int s = 0, i = 0; for (; i < 10; i++) { s = s + i; } test(s, 45); }
  { int s = 0, i; for (i = 0; i < 10;) { s = s + i; i++; } test(s, 45); }
  { int s = 0, i; for (i = 0; i < 10; i++) { s = s + i; } test(s, 45); }

  { int i; i = 0; for (;; i++) { if (i < 100) continue; break; } test(i, 100); }
  { int i; i = 0; do { i++; if (i < 100) continue; break; } while (1); test(i, 100); }
  { int i; i = 0; do { i++; if (i < 100) continue; } while (i < 50); test(i, 50); }
  { int i; i = 0; while (1) { i++; if (i < 100) continue; break; } test(i, 100); }

  { int *x = alloc(); test(*x, 53); }
  { int *x = alloc(); test(*(x + 1), 29); }
  { int *x = alloc(); test(*(x + 2), 64); }
  { int *x = alloc(); *(x + 1) = 5; test(*(x + 1), 5); }
  { int *x = alloc(); *(x + 1) = *(x + 2); test(*(x + 1), 64); }
  { int *x = alloc(); int *y; y = x + 2; test(*(y - 1), 29); }

  { int x = 7; test(*&x, 7); }
  { int x = 7, y = 5; test(*&x * *&y, 35); }
  { int x = 123, *y = &x; test(*y, 123); }
  { int x = 123, *y = &x, **z = &y; test(**z, 123); }
  { int x = 123, *y = &x; *y = 231; test(x, 231); }
  { int x1 = 123, x2 = 231, *y = &x1, **z = &y; *z = &x2; *y = 12; test(x2, 12); }

  { int t, x = 2, *y = &t; *y = x + 3; test(*y + 4, 9); }
  { int x[5]; for (int i = 0; i < 5; i++) { *(x + i) = i * i; } test(*(x + 0), 0); }
  { int x[5]; for (int i = 0; i < 5; i++) { *(x + i) = i * i; } test(*(x + 2), 4); }
  { int x[5]; for (int i = 0; i < 5; i++) { *(x + i) = i * i; } test(*(x + 4), 16); }
  { int a = 2, *x[4]; *(x + 3) = &a; test(**(x + 3), 2); }
  { int x[3][4]; **x = 5; test(**x, 5); }
  { int x[3][4]; *(*(x + 2) + 3) = 5; test(*(*(x + 2) + 3), 5); }
  { int x[3][3]; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) *(*(x + i) + j) = i * j; test(*(*(x + 2) + 1), 2); }

  { int x; x = 1; test(x++, 1); }
  { int x; x = 1; test(++x, 2); }
  { int x; x = 1; x++; test(x, 2); }
  { int x; x = 1; ++x; test(x, 2); }
  { int x; x = 1; test(x--, 1); }
  { int x; x = 1; test(--x, 0); }
  { int x; x = 1; x--; test(x, 0); }
  { int x; x = 1; --x; test(x, 0); }
  { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; test(*(p++), 92); }
  { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; test(*(++p), 93); }
  { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; p++; test(*p, 93); }
  { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; ++p; test(*p, 93); }
  { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; test(*(p--), 92); }
  { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; test(*(--p), 91); }
  { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; p--; test(*p, 91); }
  { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; --p; test(*p, 91); }

  test(sizeof(123), 4);
  test(sizeof 123, 4);
  test(sizeof 123 + 13, 17);
  { char x; test(sizeof(x), 1); }
  { int x; test(sizeof(x), 4); }
  { int *x; test(sizeof(x), 8); }
  { int **x; test(sizeof(x), 8); }
  { int x[4]; test(sizeof(x), 16); }
  { int x[4][5]; test(sizeof(x), 80); }
  { int *x[4][5]; test(sizeof(x), 160); }
  { int x[4][5]; test(sizeof(x[0]), 20); }
  { char x; test(sizeof("abc"), 4); }
  { char x; test(sizeof("abc\n"), 5); }
  { char x; test(sizeof("abc\0abc\n"), 9); }
  { char *x; x = "abc"; test(sizeof(x), 8); }
  test(sizeof(char), 1);
  test(sizeof(int), 4);
  test(sizeof(void *), 8);
  test(sizeof(char *), 8);
  test(sizeof(int *), 8);
  test(sizeof(int **), 8);
  test(sizeof(struct { char c1, c2; }), 2);
  test(sizeof(struct { char c1; int n; char c2; }), 12);
  test(sizeof(struct { char c1, c2; int n; }), 8);
  test(sizeof(struct { char c1, c2; int *p, n; }), 24);
  test(sizeof(struct { char c1, c2; int n, *p; }), 16);

  test(_Alignof(char), 1);
  test(_Alignof(int), 4);
  test(_Alignof(void *), 8);
  test(_Alignof(char *), 8);
  test(_Alignof(int *), 8);
  test(_Alignof(int **), 8);
  test(_Alignof(struct { char c1, c2; }), 1);
  test(_Alignof(struct { char c1; int n; char c2; }), 4);
  test(_Alignof(struct { char c1, c2; int n; }), 4);
  test(_Alignof(struct { char c1, c2; int *p, n; }), 8);
  test(_Alignof(struct { char c1, c2; int n, *p; }), 8);

  { int x = 5; test(x, 5); }
  { int x = 5, y = x + 3; test(y, 8); }
  { int x = 5; int y = x + 3; test(y, 8); }
  { int x = 3, y = 5, z = x + y * 8; test(z, 43); }
  { int x = 42, *y = &x; test(*y, 42); }

  { int x = 2; x += 3; test(x, 5); }
  { int A[3]; A[0] = 30; A[1] = 31; A[2] = 32; int *p = A; p += 2; test(*p, 32); }
  { int x = 5; x -= 3; test(x, 2); }
  { int A[3]; A[0] = 30; A[1] = 31; A[2] = 32; int *p = A + 2; p -= 2; test(*p, 30); }

  test('A', 65);
  test('\n', 10);
  test('\'', 39);

  { int a, b, *p = &a, *q = &b; test(p == q, 0); }
  { int a, b, *p = &a, *q = &b; test(p != q, 1); }
  { int a, *p = &a, *q = &a; test(p == q, 1); }
  { int a, *p = &a, *q = &a; test(p != q, 0); }

  { int a[3], *p = a + 1, *q = a + 0; test(p < q, 0); }
  { int a[3], *p = a + 1, *q = a + 1; test(p < q, 0); }
  { int a[3], *p = a + 1, *q = a + 2; test(p < q, 1); }
  { int a[3], *p = a + 1, *q = a + 0; test(p > q, 1); }
  { int a[3], *p = a + 1, *q = a + 1; test(p < q, 0); }
  { int a[3], *p = a + 1, *q = a + 2; test(p > q, 0); }
  { int a[3], *p = a + 1, *q = a + 0; test(p <= q, 0); }
  { int a[3], *p = a + 1, *q = a + 1; test(p <= q, 1); }
  { int a[3], *p = a + 1, *q = a + 2; test(p <= q, 1); }
  { int a[3], *p = a + 1, *q = a + 0; test(p >= q, 1); }
  { int a[3], *p = a + 1, *q = a + 1; test(p >= q, 1); }
  { int a[3], *p = a + 1, *q = a + 2; test(p >= q, 0); }

  { int a = 34, b = 58, *p = &a, *q = &b; test(*(1 ? p : q), 34); }
  { int a = 34, b = 58, *p = &a, *q = &b; test(*(0 ? p : q), 58); }

  { int a, *p = &a; test(p ? 123 : 231, 123); }
  { int *p = 0; test(p ? 123 : 231, 231); }
  { int x, a, *p = &a; if (p) x = 123; else x = 231; test(x, 123); }
  { int x, *p = 0; if (p) x = 123; else x = 231; test(x, 231); }
  { int x = 231, a, *p = &a; while (p) { x = 123; break; } test(x, 123); }
  { int x = 231, *p = 0; while (p) { x = 123; break; } test(x, 231); }
  { int x = 231, a, *p = &a; for (; p;) { x = 123; break; } test(x, 123); }
  { int x = 231, *p = 0; for (; p;) { x = 123; break; } test(x, 231); }

  { int a[3]; a[0] = 0; a[1] = 1; a[2] = 2; test(1[a], 1); }

  { int a, *p = &a; test(!p, 0); }
  { int *p = 0; test(!p, 1); }

  test(100 > -1, 1);
  test(-1 < 100, 1);
  test(100 >= -1, 1);
  test(-1 <= 100, 1);

  {
    struct test_struct s;
    s.c = 'A';
    s.n = 45;
    s.a[0] = 5;
    s.a[1] = 3;
    s.a[2] = 8;
    s.a[3] = 7;
    s.v.x = 67;
    s.v.y = 1;
    s.v.z = -41;

    test(test_struct(&s), 0);

    test(s.c, 'Z');
    test(s.n, 82);
    test(s.a[0], 4);
    test(s.a[1], 4);
    test(s.a[2], 1234);
    test(s.a[3], -571);
    test(s.v.x, 98);
    test(s.v.y, 12);
    test(s.v.z, 1);
  }
  { struct { int x, y; } a, *p; p = &a; p->x = 12; a.y = 7; test(a.x == 12 && p->y == 7, 1); }

  return 0;
}
