#!/bin/bash

target=$1

# exit with error message.
failed() {
  echo "$1"
  exit 1
}

# unit test for string, vector and map.
gcc -std=c11 -Wall -Wno-builtin-declaration-mismatch string.c tests/string_driver.c -o tmp/string_test
./tmp/string_test || failed "assertion of string.c was failed."

gcc -std=c11 -Wall -Wno-builtin-declaration-mismatch vector.c tests/vector_driver.c -o tmp/vector_test
./tmp/vector_test || failed "assertion of vector.c was failed."

gcc -std=c11 -Wall -Wno-builtin-declaration-mismatch map.c tests/map_driver.c -o tmp/map_test
./tmp/map_test || failed "assertion of map.c was failed."

# unit test for token.
gcc -std=c11 -Wall -Wno-builtin-declaration-mismatch token.c tests/token_driver.c -o tmp/token_test
./tmp/token_test || failed "assertion of token.c was failed."

# failed to compile
error() {
  echo "[failed]"
  echo "exit with error."
  echo "[input]"
  head  tmp/cc_test.c
  echo "[log]"
  cat tmp/cc_test.log
  exit 1
}

# failed to link with gcc
invalid() {
  echo "[failed]"
  echo "failed to link with gcc."
  echo "[input]"
  head -n 10 tmp/cc_test.c
  echo "[output]"
  head -n 10 tmp/cc_test.s
  exit 1
}

# execution result is not expected
miscompile() {
  expect=$1
  actual=$2
  echo "[failed]"
  echo "$expect is expected, but got $actual."
  echo "[input]"
  head -n 10 tmp/cc_test.c
  echo "[output]"
  head -n 10 tmp/cc_test.s
  exit 1
}

expect_return() {
  expect=$1
  cat - > tmp/cc_test.c
  $target tmp/cc_test.c > tmp/cc_test.s 2> tmp/cc_test.log || error
  gcc -no-pie tmp/cc_test.s tmp/func_call_stub.o -o tmp/cc_test || invalid
  ./tmp/cc_test
  actual=$?
  [ $actual -ne $expect ] && miscompile $expect $actual
  return 0
}

expect_stdout() {
  expect=$1
  cat - > tmp/cc_test.c
  $target tmp/cc_test.c > tmp/cc_test.s 2> tmp/cc_test.log || error
  gcc -no-pie tmp/cc_test.s tmp/func_call_stub.o -o tmp/cc_test || invalid
  actual=`./tmp/cc_test | sed -z 's/\\n/\\\\n/g'`
  [ "$actual" != "$expect" ] && miscompile "$expect" "$actual"
  return 0
}

test_error() {
  prog=$1
  echo "$prog" > tmp/cc_test.c
  $target tmp/cc_test.c > /dev/null 2> /dev/null
  ret=$?
  if [ $ret -eq 0 ]; then
    echo "[failed]"
    echo "compilation is unexpectedly succeeded."
    echo "[input]"
    head -n 10 tmp/cc_test.c
    exit 1
  fi
  if [ $ret -ne 1 ]; then
    echo "[failed]"
    echo "compilation is failed."
    echo "[input]"
    head -n 10 tmp/cc_test.c
    exit 1
  fi
  return 0
}

gcc -std=c11 -Wall -Wno-builtin-declaration-mismatch -c tests/func_call_stub.c -o tmp/func_call_stub.o

gcc -std=c11 -Wall -Wno-builtin-declaration-mismatch -P -E tests/test.c > tmp/test.c
expect_return 0 < tmp/test.c

expect_return 0 <<-EOS
int main() {
  int x;
  x = 0;
  return x;
  x = 1;
  return x;
}
EOS

expect_return 42 <<-EOS
int main() {
  for (int i = 0; i < 100; i++)
    if (i == 42)
      return i;
  return 123;
}
EOS

expect_return 6 <<-EOS
int f() {
  return 2;
}
int g() {
  return 3;
}
int main() {
  return f() * g();
}
EOS

expect_return 24 <<-EOS
int f() {
  int x = 2;
  int y = 3;
  return x + 2 * y;
}
int main() {
  int t = f();
  return t * 3;
}
EOS

expect_return 16 <<-EOS
int f() {
  int x = 2;
  return x * x;
}
int main() {
  int t = f();
  return t * f();
}
EOS

expect_return 14 <<-EOS
int f(int x) {
  return x * x;
}
int main() {
  return f(1) + f(2) + f(3);
}
EOS

expect_return 10 <<-EOS
int f(int x, int y, int z) {
  x = x * y;
  y = x + z;
  return x + y + z;
}
int main() {
  return f(1, 2, 3);
}
EOS

expect_return 32 <<-EOS
int *f(int *p) {
  return p;
}
int main() {
  int x = 32;
  int *y = f(&x);
  return *y;
}
EOS

expect_return 54 <<-EOS
int *f(int *p) {
  *p = 54;
  return p;
}
int main() {
  int x = 87;
  int *y = f(&x);
  return *y;
}
EOS

expect_return 0 <<-EOS
int x;
int main() {
  return 0;
}
EOS

expect_return 0 <<-EOS
int x, y[20];
int main() {
  return 0;
}
EOS

expect_return 8 <<-EOS
int x;
int func() {
  x = 8;
}
int main() {
  func();
  return x;
}
EOS

expect_return 3 <<-EOS
int y[20];
int func() {
  y[5] = 3;
}
int main() {
  func();
  return y[5];
}
EOS

expect_return 29 <<-EOS
int x;
int func(int *p) {
  *p = 29;
}
int main() {
  func(&x);
  return x;
}
EOS

expect_return 6 <<-EOS
int x, *y;
int func() {
  *y = 6;
}
int main() {
  y = &x;
  func();
  return x;
}
EOS

expect_return 0 <<-EOS
int x;
int func() {
  int x = 123;
}
int main() {
  func();
  return x;
}
EOS

expect_return 0 <<-EOS
char c, s[20];
int main() {
  return 0;
}
EOS

expect_return 0 <<-EOS
int main() {
  char c, s[20];
  return 0;
}
EOS

expect_return 78 <<-EOS
int main() {
  char c1 = 13, c2 = 65;
  char c3 = c1 + c2;
  return c3;
}
EOS

expect_return 65 <<-EOS
char f(char c) {
  return c;
}
int main() {
  return f('A');
}
EOS

expect_stdout "ABC\n" <<-EOS
int puts(char *s);
int main() {
  char s[4];
  s[0] = 65;
  s[1] = 66;
  s[2] = 67;
  s[3] = 0;
  puts(s);
  return 0;
}
EOS

expect_stdout "ABCDEFG\n" <<-EOS
int puts(char *s);
char s[8];
int main() {
  for (int i = 0; i < 7; i++) s[i] = i + 65;
  s[7] = 0;
  puts(s);
  return 0;
}
EOS

expect_stdout "hello world\n" <<-EOS
int puts(char *s);
int main() {
  puts("hello world");
  return 0;
}
EOS

expect_stdout "hello world\n" <<-EOS
int puts(char *s);
int main() {
  char *s = "hello world";
  puts(s);
  return 0;
}
EOS

expect_stdout "w\n" <<-EOS
int printf(char *format, ...);
int main() {
  char *s = "hello world";
  printf("%c\n", s[6]);
  return 0;
}
EOS

expect_return 12 <<-EOS
int main() {
  int x = 12;
  if (1) {
    int x = 34;
  }
  return x;
}
EOS

expect_return 12 <<-EOS
int main() {
  int x = 12, y = 14;
  if (1) {
    int *x = &y;
  }
  return x;
}
EOS

expect_stdout "3\n1\n" <<-EOS
int printf(char *format, ...);
int x = 0;
int main() {
  int x = 1;
  {
    int x = 2;
    { x = 3; }
    { int x = 4; }
    printf("%d\n", x);
  }
  printf("%d\n", x);
}
EOS

expect_stdout "0\n1\n2\n3\n4\n123\n" <<-EOS
int printf(char *format, ...);
int main() {
  int i = 123;
  for (int i = 0; i < 5; i++) {
    printf("%d\n", i);
    int i = 42;
  }
  printf("%d\n", i);
  return 0;
}
EOS

expect_return 56 <<-EOS
struct object {
  int x, y;
};
int main() {
  struct object p;
  p.x = 56;
  return p.x;
}
EOS

expect_return 56 <<-EOS
int main() {
  struct object {
    int x, y;
  };
  struct object p;
  p.x = 56;
  return p.x;
}
EOS

expect_return 56 <<-EOS
struct object {
  int x, y;
} q;
int main() {
  struct object p;
  p.x = 56;
  q.y = 97;
  return p.x;
}
EOS

expect_return 56 <<-EOS
int main() {
  struct object {
    int x, y;
  } q;
  struct object p;
  p.x = 56;
  q.y = 97;
  return p.x;
}
EOS

expect_return 55 <<-EOS
typedef int MyInt;
int main() {
  MyInt x = 55;
  return x;
}
EOS

expect_return 55 <<-EOS
int main() {
  typedef int MyInt;
  MyInt x = 55;
  return x;
}
EOS

expect_return 15 <<-EOS
typedef struct {
  int x, y;
} Vector2;
int main() {
  Vector2 p;
  p.x = 5;
  p.y = 3;
  return p.x * p.y;
}
EOS

expect_return 18 <<-EOS
struct abc {
  struct abc *p;
  int v;
};
int main() {
  struct abc x, y;
  x.p = &y;
  y.v = 18;
  return x.p->v;
}
EOS

expect_return 68 <<-EOS
char f(char c, int a);
int main() {
  return f('A', 3);
}
char f(char c, int a) {
  return c + a;
}
EOS

expect_return 4 <<-EOS
int f(), g(int x);
int main() {
  return g(f());
}
int f() {
  return 2;
}
int g(int x) {
  return x * x;
}
EOS

expect_stdout "123abc\n" <<-EOS
int puts(char *s);
int main() {
  puts("123abc");
}
EOS

expect_return 0 <<-EOS
enum { U, L, D, R };
int main() {
  return U;
}
EOS

expect_return 1 <<-EOS
enum { U, L, D, R };
int main() {
  return L;
}
EOS

expect_return 2 <<-EOS
enum { U, L, D, R };
int main() {
  return D;
}
EOS

expect_return 3 <<-EOS
enum { U, L, D, R };
int main() {
  return R;
}
EOS

expect_return 0 <<-EOS
enum { U, L, D, R } d;
int main() {
  d = U;
  return d;
}
EOS

expect_return 1 <<-EOS
enum { U, L, D, R } d;
int main() {
  d = L;
  return d;
}
EOS

expect_return 2 <<-EOS
enum { U, L, D, R } d;
int main() {
  d = D;
  return d;
}
EOS

expect_return 3 <<-EOS
enum { U, L, D, R } d;
int main() {
  d = R;
  return d;
}
EOS

expect_return 2 <<-EOS
typedef enum {
  U, L, D, R
} Dir;
Dir d;
int main() {
  d = D;
  return d;
}
EOS

expect_return 3 <<-EOS
typedef enum node_type {
  CONST, MUL, DIV, ADD, SUB
} NodeType;
int main() {
  NodeType type = ADD;
  return type;
}
EOS

expect_return 35 <<-EOS
extern int external_obj;
int main() {
  return external_obj;
}
EOS

expect_stdout "abcd 1234\n" <<-EOS
int printf(char *format, ...);
int main() {
  printf("%s %d\n", "abcd", 1234);
}
EOS

expect_return 0 <<-EOS
extern _Noreturn void error(char *format, ...);
int main() {
  return 0;
}
EOS

expect_stdout "16\n" <<-EOS
int printf(char *format, ...);
typedef struct string {
  int capacity;
  int length;
  char *buffer;
} String;
int main() {
  return printf("%d\n", sizeof(String));
}
EOS

expect_stdout "16\n" <<-EOS
int printf(char *format, ...);
typedef struct vector {
  int capacity;
  int length;
  void **buffer;
} Vector;
int main() {
  return printf("%d\n", sizeof(Vector));
}
EOS

expect_stdout "16392\n" <<-EOS
int printf(char *format, ...);
typedef struct map {
  int count;
  char *keys[1024];
  void *values[1024];
} Map;
int main() {
  return printf("%d\n", sizeof(Map));
}
EOS

expect_stdout "abc 123\n" <<-EOS
#define va_start __builtin_va_start
#define va_end __builtin_va_end

typedef __builtin_va_list va_list;

typedef struct _IO_FILE FILE;
extern FILE *stdout;

int fprintf(FILE *stream, char *format, ...);
int vfprintf(FILE *s, char *format, va_list arg);

void print(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  vfprintf(stdout, format, ap);
  va_end(ap);
}

int main() {
  print("%s %d\n", "abc", 123);
}
EOS

expect_return 40 <<-EOS
int x = 40;
int main() {
  return x;
}
EOS

expect_stdout "abcde\n" <<-EOS
int printf(char *format, ...);
char *s = "abcde";
int main() {
  printf("%s\n", s);
}
EOS

expect_return 1 <<-EOS
int a[] = { 1, 2, 55, 91 };
int main() {
  return a[0];
}
EOS

expect_return 2 <<-EOS
int a[] = { 1, 2, 55, 91 };
int main() {
  return a[1];
}
EOS

expect_return 55 <<-EOS
int a[] = { 1, 2, 55, 91 };
int main() {
  return a[2];
}
EOS

expect_return 91 <<-EOS
int a[] = { 1, 2, 55, 91 };
int main() {
  return a[3];
}
EOS

expect_return 1 <<-EOS
int main() {
  int a[] = { 1, 2, 55, 91 };
  return a[0];
}
EOS

expect_return 2 <<-EOS
int main() {
  int a[] = { 1, 2, 55, 91 };
  return a[1];
}
EOS

expect_return 55 <<-EOS
int main() {
  int a[] = { 1, 2, 55, 91 };
  return a[2];
}
EOS

expect_return 91 <<-EOS
int main() {
  int a[] = { 1, 2, 55, 91 };
  return a[3];
}
EOS

expect_stdout "rdi\nrsi\nrdx\nrcx\nr8\nr9\n" <<-EOS
int printf(char *format, ...);
char *reg[] = {
  "rdi",
  "rsi",
  "rdx",
  "rcx",
  "r8",
  "r9"
};
int main() {
  for (int i = 0; i < 6; i++)
    printf("%s\n", reg[i]);
  return 0;
}
EOS

expect_return 1 <<-EOS
#define bool _Bool
#define true 1
#define false 0
int main() {
  bool b = true;
  return b;
}
EOS

expect_return 1 <<-EOS
#define NULL ((void *) 0)
int main() {
  int *p = NULL;
  return !p;
}
EOS

expect_return 5 <<-EOS
#define xxx 5
int main() {
  return xxx;
}
EOS

expect_return 60 <<-EOS
#define xxx 5
int yyy = 2;
#define zzz 6
int main() {
  return xxx * yyy * zzz;
}
EOS

expect_return 0 <<-EOS
int main() {
  /* this is comment */
  return 0;
}
EOS

expect_return 0 <<-EOS
int main() {
  return /* this is comment */ 0;
}
EOS

expect_return 0 <<-EOS
int main() {
  // this is comment.
  return 0;
}
EOS

expect_return 5 <<-EOS
#define func() (a + b)
int main() {
  int a = 2, b = 3;
  return func();
}
EOS

expect_return 13 <<-EOS
#define func(a, b) ((a) * (b) + 1)
int main() {
  return func(3, 4);
}
EOS

expect_return 10 <<-EOS
#define func(a, b) ((a) + (b))
int f(int x, int y) {
  return x * y;
}
int main() {
  return func(f(2, 3), 4);
}
EOS

expect_return 0 <<-EOS
#define empty1
#define empty2 
#define empty3  
#define empty4(a, b)
#define empty5(a, b) 
#define empty6(a, b)  
int main() { empty1; empty2; empty3; empty4(1, 2); empty5(1, 2); empty6(1, 2); return 0; }
EOS

expect_return 0 <<-EOS
#define test(expr, expected) \\
  do { \\
    int actual = (expr); \\
    if (actual != (expected)) { \\
      return 1; \\
    } \\
    return 0; \\
  } while (0)

int main() {
  test(1 + 2, 3);
}
EOS

expect_stdout "42\n" <<-EOS
int prin\\
tf(cha\\
r *fo\\
rmat, ...);
i\\
nt main() { pr\\
intf("%\\
d\n", 4\\
2); ret\\
urn 0; }
EOS

expect_stdout "40000000000\n" <<-EOS
int printf(char *format, ...);
int main() {
  long x = (long) 200000 * 200000;
  printf("%ld\n", x);
}
EOS

expect_stdout "40000000000\n" <<-EOS
int printf(char *format, ...);
int main() {
  unsigned long x = (unsigned long) 200000 * 200000;
  printf("%lu\n", x);
}
EOS

# testing error case
test_error "int main() { 2 * (3 + 4; }"
test_error "int main() { 5 + *; }"
test_error "int main() { 5 }"
test_error "int main() { 5 (4); }"
test_error "int main() { 1 ? 2; }"
test_error "int main() { 1 = 2 + 3; }"
test_error "int main() { func_call(1, 2, 3, 4, 5, 6, 7); }"
test_error "int main() { func_call(1, 2, 3; }"
test_error "int main()"
test_error "int main() { 2;"
test_error "123"
test_error "int f() { 1; } int f() { 2; } int main() { 1; }"
test_error "int main(int abc"
test_error "int main(int 123) { 0; }"
test_error "int main(int x, int x) { 0; }"
test_error "int main(int a, int b, int c, int d, int e, int f, int g) { 0; }"
test_error "int main(x) { return x; }"
test_error "int main() { x; }"
test_error "int main() { continue; }"
test_error "int main() { break; }"
test_error "int main() { return &123; }"
test_error "int main() { return *123; }"
test_error "int x, main() { return 0; };"
test_error "int main() { return 0; }, x;"
test_error "int func() { return x; } int x; int main() { func(); }"
test_error "int main() { 1++; }"
test_error "int f(int x) { int x; }"
test_error "int f[4](int x) { int a[4]; return a; }"
test_error "struct abc { struct abc p; }; int main() { return 0; }"
test_error "int main() { int a[4] = { 4, 5, 6, 7, 8 }; return 0; }"

exit 0
