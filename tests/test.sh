#!/bin/bash

target=$1

# exit with error message.
failed() {
  echo "$1"
  exit 1
}

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

gcc -std=c11 -Wall -c tests/func_call_stub.c -o tmp/func_call_stub.o

gcc -std=c11 -Wall -P -E tests/test.c > tmp/test.c
expect_return 0 < tmp/test.c

expect_return 0 <<-EOS
int x;
int y[20];
int *p;

void func1() {
  x = 8;
}

void func2() {
  y[5] = 3;
}

void func3(int *p) {
  *p = 29;
}

void func4() {
  *p = 6;
}

int main() {
  func1();
  if (x != 8) return 1;

  func2();
  if (y[5] != 3) return 1;

  func3(&x);
  if (x != 29) return 1;

  p = &x;
  func4();
  if (x != 6) return 1;

  return 0;
}
EOS

expect_return 0 <<-EOS
int x;

int func1() {
  int x = 12;
  { int x = 34; }
  if (x != 12) return 1;

  return 0;
}

int func2() {
  int x = 1;
  {
    int x = 2;
    { x = 3; }
    { int x = 4; }
    if (x != 3) return 1;
  }
  if (x != 1) return 1;

  return 0;
}

int main() {
  if (func1() != 0) return 1;
  if (func2() != 0) return 1;
  if (x != 0) return 1;

  return 0;
}
EOS

expect_return 0 <<-EOS
typedef int MyInt1;

int main() {
  MyInt1 x = 55;
  if (x != 55) return 1;

  {
    typedef int MyInt2;
    MyInt2 x = 55;
    if (x != 55) return 1;
  }

  return 0;
}
EOS

expect_return 0 <<-EOS
typedef struct {
  int x, y;
} Vector2;

struct abc {
  struct abc *p;
  int v;
};

int main() {
  Vector2 p;
  p.x = 5;
  p.y = 3;
  if (p.x != 5) return 1;
  if (p.y != 3) return 1;

  struct abc x, y;
  x.p = &y;
  x.v = 18;
  if (x.p != &y) return 1;
  if (x.v != 18) return 1;

  return 0;
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

expect_return 0 <<-EOS
enum { U, L, D, R };
enum { E, F, G, H } d;

int main() {
  if (U != 0) return 1;
  if (L != 1) return 1;
  if (D != 2) return 1;
  if (R != 3) return 1;
  d = E; if (d != 0) return 1;
  d = F; if (d != 1) return 1;
  d = G; if (d != 2) return 1;
  d = H; if (d != 3) return 1;
  return 0;
}
EOS

expect_stdout "abc 123\n" <<-EOS
#define va_start __builtin_va_start
#define va_end __builtin_va_end

typedef __builtin_va_list va_list;

typedef struct _IO_FILE FILE;
extern FILE *stdout;

int vfprintf(FILE *s, char *format, va_list arg);

void print(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  vfprintf(stdout, format, ap);
  va_end(ap);
}

int main() {
  print("%s %d\n", "abc", 123);
  return 0;
}
EOS

expect_return 0 <<-EOS
int x = 40;
int a[] = { 1, 2, 55, 91 };
int main() {
  if (x != 40) return 1;
  if (a[0] != 1) return 1;
  if (a[1] != 2) return 1;
  if (a[2] != 55) return 1;
  if (a[3] != 91) return 1;
  return 0;
}
EOS

expect_return 0 <<-EOS
int main() {
  // this is comment.
  /* this is comment */
  return /* this is comment */ 0;
}
EOS

expect_return 0 <<-EOS
#define true 1
#define false 0

#define NULL ((void *) 0)

#define func1(a, b) ((a) * (b) + 1)
#define func2(a, b) ((a) + (b))

#define empty1
#define empty2 
#define empty3  
#define empty4(a, b)
#define empty5(a, b) 
#define empty6(a, b)  

#define test(expr, expected) \\
  do { \\
    int actual = (expr); \\
    if (actual != (expected)) { \\
      return 1; \\
    } \\
    return 0; \\
  } while (0)

#define file __FILE__
#define line __LINE__

int strcmp(char *s1, char *s2);

int main() {
  if (true != 1) return 1;
  if (false != 0) return 1;

  int *p = NULL;
  if (p) return 1;

  if (func1(2, 3) != 7) return 1;
  if (func2(2, 3) != 5) return 1;

  empty1;
  empty2;
  empty3;
  empty4(1, 2);
  empty5(1, 2);
  empty6(1, 2);

  test(1 + 2, 3);

  if (strcmp(__FILE__, "tmp/cc_test.c") != 0) return 1;
  if (strcmp(file, "tmp/cc_test.c") != 0) return 1;
  if (__LINE__ != 44) return 1;
  if (line != 45) return 1;

  return 0;
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

expect_return 62 <<-EOS
static int x = 62;
static int func() {
  return x;
}
int main() {
 return func();
}
EOS

# testing error case
test_error "int main() { 2 * (3 + 4; }"
test_error "int main() { 5 + *; }"
test_error "int main() { 5 }"
test_error "int main() { 5 (4); }"
test_error "int main() { 1 ? 2; }"
test_error "int main() { 1 = 2 + 3; }"
test_error "int main() { func_call(1, 2, 3; }"
test_error "int main() { sizeof(void); }"
test_error "int main() { sizeof(int []); }"
test_error "int main() { sizeof(void (void)); }"
test_error "int main()"
test_error "int main() { 2;"
test_error "123"
test_error "int f() { 1; } int f() { 2; } int main() { 1; }"
test_error "int main(int abc"
test_error "int main(int 123) { 0; }"
test_error "int main(int x, int x) { 0; }"
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
test_error "int f(int x)[4] { int a[4]; return a; }"
test_error "struct abc { struct abc p; }; int main() { return 0; }"
test_error "int main() { int a[4] = { 4, 5, 6, 7, 8 }; return 0; }"
test_error "int main() { goto label; }"
test_error "int main() { label: return 0; label: return 1; }"
test_error "int f() { label: return 0; } int main() { goto label; }"

exit 0
