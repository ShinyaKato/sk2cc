#define bool _Bool

#define va_start __builtin_va_start
#define va_arg __builtin_va_arg
#define va_end __builtin_va_end
typedef __builtin_va_list va_list;

typedef long long intptr_t;

typedef struct _IO_FILE FILE;
extern FILE *stderr;

int fprintf(FILE *stream, char *format, ...);
int strcmp(char *s1, char *s2);
void exit(int status);

#define expect(expr, expected) \
  do { \
    int actual = (expr); \
    if (actual != (expected)) { \
      fprintf(stderr, "%s:%d: \"%s\" should be %d, but got %d.\n", __FILE__, __LINE__, #expr, (expected), actual); \
      exit(1); \
    } \
  } while (0)

void test_expression() {
  // character-constant
  expect('A', 65);
  expect('\n', 10);
  expect('\'', 39);

  // array subscription
  {
    int x[5]; for (int i = 0; i < 5; i++) {
      x[i] = i * i;
    }
    expect(*(x + 0), 0);
    expect(*(x + 2), 4);
    expect(*(x + 4), 16);
  }
  {
    int x[3][3];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        x[i][j] = i * j;
      }
    }
    expect(*(*(x + 2) + 1), 2);
    expect(*(*(x + 2) + 2), 4);
  }


  // postfix increment / decrement
  { int x = 1; expect(x++, 1); expect(x, 2); }
  { int x = 1; expect(x--, 1); expect(x, 0); }

  { int *p, a[3] = { 1, 2, 3 }; p = &a[1]; expect(*(p++), 2); expect(*p, 3); }
  { int *p, a[3] = { 1, 2, 3 }; p = &a[1]; expect(*(p--), 2); expect(*p, 1); }

  // prefix increment / decrement
  { int x = 1; expect(++x, 2); expect(x, 2); }
  { int x = 1; expect(--x, 0); expect(x, 0); }

  { int *p, a[3] = { 1, 2, 3 }; p = &a[1]; expect(*(++p), 3); expect(*p, 3); }
  { int *p, a[3] = { 1, 2, 3 }; p = &a[1]; expect(*(--p), 1); expect(*p, 1); }

  // unary & / unary *
  { int x = 7; expect(*&x, 7); }
  { int x = 7, y = 5; expect(*&x * *&y, 35); }
  { int x = 123, *y = &x; expect(*y, 123); }
  { int x = 123, *y = &x, **z = &y; expect(**z, 123); }
  { int x = 123, *y = &x; *y = 231; expect(x, 231); }
  { int x1 = 123, x2 = 231, *y = &x1, **z = &y; *z = &x2; *y = 12; expect(x2, 12); }

  // unary +
  expect(+5, 5);

  // unary -
  expect(-5, 0 - 5);

  // unary !
  expect(!0, 1);
  expect(!1, 0);
  expect(!!0, 0);

  { int a, *p = &a; expect(!p, 0); }
  { int *p = 0; expect(!p, 1); }

  // unary ~
  expect(~183, -184);

  // sizeof
  expect(sizeof(123), 4);
  expect(sizeof 123, 4);
  expect(sizeof 123 + 13, 17);

  { char x; expect(sizeof(x), 1); }
  { int x; expect(sizeof(x), 4); }
  { int *x; expect(sizeof(x), 8); }
  { int **x; expect(sizeof(x), 8); }
  { int x[4]; expect(sizeof(x), 16); }
  { int x[4][5]; expect(sizeof(x), 80); }
  { int *x[4][5]; expect(sizeof(x), 160); }
  { int x[4][5]; expect(sizeof(x[0]), 20); }
  { char *x = "abc"; expect(sizeof(x), 8); }

  expect(sizeof("abc"), 4);
  expect(sizeof("abc\n"), 5);
  expect(sizeof("abc\0abc\n"), 9);

  expect(sizeof(char), 1);
  expect(sizeof(int), 4);
  expect(sizeof(void *), 8);
  expect(sizeof(char *), 8);
  expect(sizeof(int *), 8);
  expect(sizeof(int **), 8);
  expect(sizeof(struct { char c1, c2; }), 2);
  expect(sizeof(struct { char c1; int n; char c2; }), 12);
  expect(sizeof(struct { char c1, c2; int n; }), 8);
  expect(sizeof(struct { char c1, c2; int *p, n; }), 24);
  expect(sizeof(struct { char c1, c2; int n, *p; }), 16);

  // alignof
  expect(_Alignof(char), 1);
  expect(_Alignof(int), 4);
  expect(_Alignof(void *), 8);
  expect(_Alignof(char *), 8);
  expect(_Alignof(int *), 8);
  expect(_Alignof(int **), 8);
  expect(_Alignof(struct { char c1, c2; }), 1);
  expect(_Alignof(struct { char c1; int n; char c2; }), 4);
  expect(_Alignof(struct { char c1, c2; int n; }), 4);
  expect(_Alignof(struct { char c1, c2; int *p, n; }), 8);
  expect(_Alignof(struct { char c1, c2; int n, *p; }), 8);

  // cast operator
  { signed long x = (signed long) 200000 * 200000; expect(x == 40000000000l, 1); }
  { unsigned long x = (unsigned long) 200000 * 200000; expect(x == 40000000000ul, 1); }

  { void *p = (void *) (intptr_t) 42; expect((int) (intptr_t) p, 42); }

  // multiplicative operator
  expect(3 * 5, 15);
  expect(6 / 2, 3);
  expect(123 % 31, 30);

  // additive operator
  expect(2 + 3, 5);
  expect(11 - 7, 4);

  // shift operator
  expect(1 << 6, 64);
  expect(64 >> 6, 1);
  expect(64 >> 8, 0);
  expect(41 << 2, 164);
  expect(41 >> 3, 5);

  // relational operator
  expect(35 < 36, 1);
  expect(35 < 35, 0);
  expect(35 > 35, 0);
  expect(35 > 34, 1);
  expect(35 <= 35, 1);
  expect(35 <= 34, 0);
  expect(35 >= 36, 0);
  expect(35 >= 35, 1);

  expect(100 > -1, 1);
  expect(-1 < 100, 1);
  expect(100 >= -1, 1);
  expect(-1 <= 100, 1);

  { int a[3], *p = &a[1], *q = &a[0]; expect(p < q, 0); }
  { int a[3], *p = &a[1], *q = &a[1]; expect(p < q, 0); }
  { int a[3], *p = &a[1], *q = &a[2]; expect(p < q, 1); }
  { int a[3], *p = &a[1], *q = &a[0]; expect(p > q, 1); }
  { int a[3], *p = &a[1], *q = &a[1]; expect(p < q, 0); }
  { int a[3], *p = &a[1], *q = &a[2]; expect(p > q, 0); }
  { int a[3], *p = &a[1], *q = &a[0]; expect(p <= q, 0); }
  { int a[3], *p = &a[1], *q = &a[1]; expect(p <= q, 1); }
  { int a[3], *p = &a[1], *q = &a[2]; expect(p <= q, 1); }
  { int a[3], *p = &a[1], *q = &a[0]; expect(p >= q, 1); }
  { int a[3], *p = &a[1], *q = &a[1]; expect(p >= q, 1); }
  { int a[3], *p = &a[1], *q = &a[2]; expect(p >= q, 0); }

  // equality operator
  expect(15 == 15, 1);
  expect(15 == 19, 0);

  expect(15 != 15, 0);
  expect(15 != 19, 1);

  { int a, b, *p = &a, *q = &b; expect(p == q, 0); }
  { int a, b, *p = &a, *q = &b; expect(p != q, 1); }
  { int a, *p = &a, *q = &a; expect(p == q, 1); }
  { int a, *p = &a, *q = &a; expect(p != q, 0); }

  // bitwise operator
  expect(183 & 109, 37);

  expect(183 | 109, 255);

  expect(183 ^ 109, 218);

  // logical operator
  expect(1 && 1, 1);
  expect(0 && 1, 0);
  expect(1 && 0, 0);
  expect(0 && 0, 0);

  expect(1 || 1, 1);
  expect(0 || 1, 1);
  expect(1 || 0, 1);
  expect(0 || 0, 0);

  // assignment operator
  { int x; x = 123; expect(x, 123); }
  { int x; x = 3; x = x * x + 1; expect(x, 10); }
  { int x; expect(x = 2 * 3 * 4, 24); }
  { int x, y; x = 2; y = x + 5; expect(y, 7); }
  { int x, y, z; expect(x = y = z = 3, 3); }
  { int x, y, z; x = (y = (z = 1) + 2) + 3; expect(x, 6); expect(y, 3); expect(z, 1); }
  { int x = 19; expect(x *= 3, 57); expect(x, 57); }
  { int x = 19; expect(x /= 3, 6); expect(x, 6); }
  { int x = 19; expect(x %= 3, 1); expect(x, 1); }
  { int x = 19; expect(x += 3, 22); expect(x, 22); }
  { int x = 19; expect(x -= 3, 16); expect(x, 16); }

  // conditional operator
  expect(3 > 2 ? 13 : 31, 13);
  expect(3 < 2 ? 13 : 31, 31);
  expect(1 ? 1 ? 6 : 7 : 1 ? 8 : 9, 6);
  expect(1 ? 0 ? 6 : 7 : 1 ? 8 : 9, 7);
  expect(0 ? 1 ? 6 : 7 : 1 ? 8 : 9, 8);
  expect(0 ? 1 ? 6 : 7 : 0 ? 8 : 9, 9);

  { int a = 1, b = 2, *p = &a, *q = &b; expect(*(1 ? p : q), 1); }
  { int a = 1, b = 2, *p = &a, *q = &b; expect(*(0 ? p : q), 2); }

  { int a, *p = &a; expect(p ? 1 : 2, 1); }
  { int *p = 0; expect(p ? 1 : 2, 2); }

  // test expression
  expect((((123))), 123);
  expect((3 - 5) * 3 + 7, 1);
  expect(6 / (2 - 3) + 7, 1);
  expect(32 / 4 + 5 * (8 - 5) + 5 % 2, 24);
  expect(3 * 5 + 7 * 8 - 3 * 4, 59);
  expect(123 + 43 + 1 + 21, 188);
  expect(123 - 20 - 3, 100);
  expect(123 + 20 - 3 + 50 - 81, 109);
  expect(!(2 * 3 <= 2 + 3), 1);
  expect(3 * 7 > 20 && 5 < 10 && 6 + 2 <= 3 * 3 && 5 * 2 % 3 == 1, 1);
  expect(3 * 7 > 20 && 5 < 10 && 6 + 2 >= 3 * 3 && 5 * 2 % 3 == 1, 0);
  expect(8 * 7 < 50 || 10 > 2 * 5 || 3 % 2 == 1 || 5 * 7 < 32, 1);
  expect(8 * 7 < 50 || 10 > 2 * 5 || 3 % 2 != 1 || 5 * 7 < 32, 0);
  expect(3 * 7 > 20 && 2 * 4 <= 7 || 8 % 2 == 0 && 5 / 2 >= 2, 1);
}

void test_declaration() {
  // boolean
  { _Bool b = 0; expect(b, 0); }
  { _Bool b = 1; expect(b, 1); }
  { _Bool b = 8; expect(b, 1); }
  { _Bool b1 = 0, b2 = !b1; expect(b2, 1); }
  { _Bool b1 = 1, b2 = !b1; expect(b2, 0); }
  { _Bool b1 = 0, b2 = b1 + 50; expect(b2, 1); }

  // struct
  { struct { int x, y; } a, *p; p = &a; p->x = 12; a.y = 7; expect(a.x == 12 && p->y == 7, 1); }
  { struct { int x, y; } t; t.x = 10; expect(*&*&t.x, 10); }
  { struct { int x, y; } t, *p; p = &t; t.x = 10; expect(*&*&p->x, 10); }

  // enumeration
  { enum { U, L, D, R }; expect(U, 0); }
  { enum { U, L, D, R }; expect(L, 1); }
  { enum { U, L, D, R }; expect(D, 2); }
  { enum { U, L, D, R }; expect(R, 3); }
  { enum { U, L, D, R, } d; d = U; expect(d, 0); }
  { enum { U, L, D, R, } d; d = L; expect(d, 1); }
  { enum { U, L, D, R, } d; d = D; expect(d, 2); }
  { enum { U, L, D, R, } d; d = R; expect(d, 3); }
  { enum { U, L = 13, D, R }; expect(U, 0); }
  { enum { U, L = 13, D, R }; expect(L, 13); }
  { enum { U, L = 13, D, R }; expect(D, 14); }
  { enum { U, L = 13, D, R }; expect(R, 15); }

  // unsigned
  { unsigned int x = 12; expect(x, 12); }
  { unsigned int x = 12, y = 34; expect(x + y, 46); }
  { unsigned int x = 12, y = 34; expect(x * y, 408); }

  // initializer list
  {
    int a[4] = { 1, 2, 3 };
    expect(a[0], 1);
    expect(a[1], 2);
    expect(a[2], 3);
  }
  {
    int a[4] = { 1, 2, 3, 4, };
    expect(a[0], 1);
    expect(a[1], 2);
    expect(a[2], 3);
    expect(a[3], 4);
  }
  {
    int a[] = { 1, 2, 3, 4 };
    expect(a[0], 1);
    expect(a[1], 2);
    expect(a[2], 3);
    expect(a[3], 4);
    expect(sizeof(a), 16);
  }
  {
    int a[4][4] = {
      { 0, 1, 2, 3 },
      { 4, 5, 6, 7 },
      { 8, 9, 10, 11 },
      { 12, 13, 14, 15 }
    };
    expect(a[0][0], 0);
    expect(a[1][1], 5);
    expect(a[2][2], 10);
    expect(a[3][3], 15);
  }
  {
    char *s[2][2] = {
      { "ab", "cd" },
      { "ef", "gh" }
    };
    expect(strcmp(s[0][0], "ab"), 0);
    expect(strcmp(s[1][1], "gh"), 0);
  }
  {
    char *reg[] = {
      "rdi",
      "rsi",
      "rdx",
      "rcx",
      "r8",
      "r9"
    };
    expect(strcmp(reg[0], "rdi"), 0);
    expect(strcmp(reg[1], "rsi"), 0);
    expect(strcmp(reg[2], "rdx"), 0);
    expect(strcmp(reg[3], "rcx"), 0);
    expect(strcmp(reg[4], "r8"), 0);
    expect(strcmp(reg[5], "r9"), 0);
  }
}

void test_statement() {
  // if-statement
  { int x = 0; if (1) { x = 1; } expect(x, 1); }
  { int x = 0; if (0) { x = 1; } expect(x, 0); }

  { int x = 0; if (1) { x = 1; } else { x = 2; } expect(x, 1); }
  { int x = 0; if (0) { x = 1; } else { x = 2; } expect(x, 2); }

  { int x; if (0) { if (0) { x = 1; } else { x = 2; } } else { if (0) { x = 3; } else { x = 4; } } expect(x, 4); }
  { int x; if (0) { if (0) { x = 1; } else { x = 2; } } else { if (1) { x = 3; } else { x = 4; } } expect(x, 3); }
  { int x; if (0) { if (1) { x = 1; } else { x = 2; } } else { if (0) { x = 3; } else { x = 4; } } expect(x, 4); }
  { int x; if (0) { if (1) { x = 1; } else { x = 2; } } else { if (1) { x = 3; } else { x = 4; } } expect(x, 3); }
  { int x; if (1) { if (0) { x = 1; } else { x = 2; } } else { if (0) { x = 3; } else { x = 4; } } expect(x, 2); }
  { int x; if (1) { if (0) { x = 1; } else { x = 2; } } else { if (1) { x = 3; } else { x = 4; } } expect(x, 2); }
  { int x; if (1) { if (1) { x = 1; } else { x = 2; } } else { if (0) { x = 3; } else { x = 4; } } expect(x, 1); }
  { int x; if (1) { if (1) { x = 1; } else { x = 2; } } else { if (1) { x = 3; } else { x = 4; } } expect(x, 1); }

  { int x = 0, a, *p = &a; if (p) x = 1; else x = 2; expect(x, 1); }
  { int x = 0, *p = 0; if (p) x = 1; else x = 2; expect(x, 2); }

  // switch-statement
  {
    int x = 0, y = 0, z = 0, w = 0;
    switch (2) {
      case 1: x = 1;
      case 2: y = 1;
      case 3: z = 1;
      default: w = 1;
    }
    expect(x, 0);
    expect(y, 1);
    expect(z, 1);
    expect(w, 1);
  }
  {
    int x = 0, y = 0, z = 0, w = 0;
    switch (7) {
      case 1: x = 1;
      case 2: y = 1;
      case 3: z = 1;
      default: w = 1;
    }
    expect(x, 0);
    expect(y, 0);
    expect(z, 0);
    expect(w, 1);
  }

  // while-statement
  { int x = 15; while (x < 10) { x++; } expect(x, 15); }
  { int x = 15; while (x < 15) { x++; } expect(x, 15); }
  { int x = 15; while (x < 20) { x++; } expect(x, 20); }

  { int x = 0, a, *p = &a; while (p) { x = 1; break; } expect(x, 1); }
  { int x = 0, *p = 0; while (p) { x = 1; break; } expect(x, 0); }

  // do-statement
  { int x = 15; do { x++; } while (x < 10); expect(x, 16); }
  { int x = 15; do { x++; } while (x < 15); expect(x, 16); }
  { int x = 15; do { x++; } while (x < 20); expect(x, 20); }

  // for-statement
  { int x; for (x = 15; x < 10; x++); expect(x, 15); }
  { int x; for (x = 15; x < 15; x++); expect(x, 15); }
  { int x; for (x = 15; x < 20; x++); expect(x, 20); }

  { int x = 0; for (; x < 10; x++); expect(x, 10); }
  { int x = 0; for (; x < 10;) x++; expect(x, 10); }

  { int x = 0; for (int i = 0; i < 10; i++) x = i; expect(x, 9); }

  { int x = 0, a, *p = &a; for (; p;) { x = 1; break; } expect(x, 1); }
  { int x = 0, *p = 0; for (; p;) { x = 1; break; } expect(x, 0); }

  // goto-statement
  {
    int x = 10;
lbl1:
    x--;
    if (x > 0) goto lbl1;
    expect(x, 0);
  }
  {
    int x = 0, y = 0;
    goto lbl2;
    x = 1;
lbl2:
    y = 1;
    expect(x, 0);
    expect(y, 1);
  }

  // continue-statement
  {
    int x = 0, sum = 0;
    while (x++ < 10) {
      if (x % 2 == 0) continue;
      sum += x;
    }
    expect(sum, 25);
  }
  {
    int x = 0, sum = 0;
    do {
      if (x % 2 == 0) continue;
      sum += x;
    } while (x++ < 10);
    expect(sum, 25);
  }
  {
    int x = 0, sum = 0;
    for (; x++ < 10;) {
      if (x % 2 == 0) continue;
      sum += x;
    }
    expect(sum, 25);
  }

  {
    int x = 0, sum = 0;
    for (; x < 10; x++) {
      continue;
      sum += x;
    }
    expect(sum, 0);
  }

  // break-statement
  {
    int x = 0;
    while (x < 10) {
      if (x == 5) break;
      x++;
    }
    expect(x, 5);
  }
  {
    int x = 0;
    do {
      if (x == 5) break;
      x++;
    } while (x < 10);
    expect(x, 5);
  }
  {
    int x = 0;
    for (; x < 10;) {
      if (x == 5) break;
      x++;
    }
    expect(x, 5);
  }

  {
    int x = 0;
    while (x < 10) {
      for (int i = 0; i < 10; i++) {
        if (i == 5) break;
      }
      x++;
    }
    expect(x, 10);
  }

  {
    int x = 0;
    switch (2) {
      case 1: x = 1; break;
      case 2: x = 2; break;
      case 3: x = 3; break;
      default: x = 4;
    }
    expect(x, 2);
  }
}

void test_call_abi() {
  int stub_arg1(int a);
  int stub_arg2(int a, int b);
  int stub_arg3(int a, int b, int c);
  int stub_arg4(int a, int b, int c, int d);
  int stub_arg5(int a, int b, int c, int d, int e);
  int stub_arg6(int a, int b, int c, int d, int e, int f);
  int stub_arg7(int a, int b, int c, int d, int e, int f, int g);
  int stub_arg8(int a, int b, int c, int d, int e, int f, int g, int h);

  expect(stub_arg1(1), 1);
  expect(stub_arg2(1, 2), 5);
  expect(stub_arg3(1, 2, 3), 14);
  expect(stub_arg4(1, 2, 3, 4), 30);
  expect(stub_arg5(1, 2, 3, 4, 5), 55);
  expect(stub_arg6(1, 2, 3, 4, 5, 6), 91);
  expect(stub_arg7(1, 2, 3, 4, 5, 6, 7), 140);
  expect(stub_arg8(1, 2, 3, 4, 5, 6, 7, 8), 204);
}

void test_bool_abi() {
  bool stub_bool(int b);

  expect(stub_bool(0), 0);
  expect(stub_bool(1048576), 1);
  expect(stub_bool(1048577), 1);
}

void test_struct_abi() {
  struct stub_struct {
    char c;
    int n;
    int a[4];
    struct {
      int x, y, z;
    } v;
  };

  void stub_struct(struct stub_struct *s);

  struct stub_struct s;
  s.c = 'A';
  s.n = 45;
  s.a[0] = 5;
  s.a[1] = 3;
  s.a[2] = 8;
  s.a[3] = 7;
  s.v.x = 67;
  s.v.y = 1;
  s.v.z = -41;

  stub_struct(&s);

  expect(s.c, 'Z');
  expect(s.n, 82);
  expect(s.a[0], 4);
  expect(s.a[1], 4);
  expect(s.a[2], 1234);
  expect(s.a[3], -571);
  expect(s.v.x, 98);
  expect(s.v.y, 12);
  expect(s.v.z, 1);
}

void test_va_list1(int a, int b, ...) {
  va_list ap;
  va_start(ap, b);

  int c = va_arg(ap, int);
  int d = va_arg(ap, int);

  va_end(ap);

  expect(a, 1);
  expect(b, 2);
  expect(c, 3);
  expect(d, 4);
}

void test_va_list2(int a, int b, ...) {
  va_list ap;
  va_start(ap, b);

  int c = va_arg(ap, int);
  int d = va_arg(ap, int);
  int e = va_arg(ap, int);
  int f = va_arg(ap, int);
  int g = va_arg(ap, int);

  va_end(ap);

  expect(a, 1);
  expect(b, 2);
  expect(c, 3);
  expect(d, 4);
  expect(e, 5);
  expect(f, 6);
  expect(g, 7);
}

void test_va_list3(int a, int b, int c, int d, int e, int f, int g, ...) {
  va_list ap;
  va_start(ap, b);

  int h = va_arg(ap, int);
  int i = va_arg(ap, int);

  va_end(ap);

  expect(a, 1);
  expect(b, 2);
  expect(c, 3);
  expect(d, 4);
  expect(e, 5);
  expect(f, 6);
  expect(g, 7);
  expect(h, 8);
  expect(i, 9);
}

int main() {
  test_expression();
  test_declaration();
  test_statement();

  test_call_abi();
  test_bool_abi();
  test_struct_abi();

  test_va_list1(1, 2, 3, 4);
  test_va_list2(1, 2, 3, 4, 5, 6, 7);
  test_va_list3(1, 2, 3, 4, 5, 6, 7, 8, 9);

  return 0;
}
