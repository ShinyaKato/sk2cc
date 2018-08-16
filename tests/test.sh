#!/bin/bash

target=$1

failed() {
  echo "$1"
  exit 1
}

compile() {
  prog=$1
  echo "$prog" > tmp/out.c
  $target tmp/out.c > tmp/out.s || failed "failed to compile \"$prog\"."
  gcc -no-pie tmp/out.s tmp/func_call_stub.o -o tmp/out || failed "failed to link \"$prog\" and stubs."
}

test_return() {
  prog=$1
  expect=$2
  compile "$prog"
  ./tmp/out
  retval=$?
  [ $retval -ne $expect ] && failed "\"$prog\" should return $expect, but got $retval."
}

test_stdout() {
  prog=$1
  expect="`echo -e "$2"`"
  compile "$prog"
  stdout=`"./tmp/out"`
  [ "$stdout" != "$expect" ] && failed "stdout of \"$prog\" should be \"$expect\", but got \"$stdout\"."
}

test_error() {
  prog=$1
  expect="error: $2"
  echo "$prog" > tmp/out.c
  $target tmp/out.c > /dev/null 2> tmp/stderr.txt && failed "compilation of \"$prog\" was unexpectedly succeeded."
  echo "$expect" | diff - tmp/stderr.txt || failed "error message of \"$prog\" should be \"$expect\"."
}

gcc -std=c11 -Wall string.c tests/string_driver.c -o tmp/string_test
./tmp/string_test || failed "assertion of string.c was failed."

gcc -std=c11 -Wall vector.c tests/vector_driver.c -o tmp/vector_test
./tmp/vector_test || failed "assertion of vector.c was failed."

gcc -std=c11 -Wall map.c tests/map_driver.c -o tmp/map_test
./tmp/map_test || failed "assertion of map.c was failed."

gcc -c tests/func_call_stub.c -o tmp/func_call_stub.o

compile "$(gcc -P -E tests/test.c)"
./tmp/out || failed "test.c failed."

test_return "int main() { int x; x = 0; return x; x = 1; return x; }" 0
test_return "int main() { for (int i = 0; i < 100; i++) if (i == 42) return i; return 123; }" 42

test_return "int f() { return 2; } int g() { return 3; } int main() { return f() * g(); }" 6
test_return "int f() { int x; int y; x = 2; y = 3; return x + 2 * y; } int main() { int t; t = f(); return t * 3; }" 24
test_return "int f() { int x; x = 2; return x * x; } int main() { int t; t = f(); return t * f(); }" 16
test_return "int f(int x) { return x * x; } int main() { return f(1) + f(2) + f(3); }" 14
test_return "int f(int x, int y, int z) { x = x * y; y = x + z; return x + y + z; } int main() { return f(1, 2, 3); }" 10
test_return "int *f(int *p) { return p; } int main() { int x; x = 123; int *y; y = f(&x); return *y; }" 123
test_return "int *f(int *p) { *p = 231; return p; } int main() { int x; x = 123; int *y; y = f(&x); return *y; }" 231

test_return "int x; int main() { return 0; }" 0
test_return "int x, y[20]; int main() { return 0; }" 0
test_return "int x; int func() { x = 8; } int main() { func(); return x; }" 8
test_return "int y[20]; int func() { y[5] = 3; } int main() { func(); return y[5]; }" 3
test_return "int x; int func(int *p) { *p = 123; } int main() { func(&x); return x; }" 123
test_return "int x, *y; int func() { *y = 123; } int main() { y = &x; func(); return x; }" 123
test_return "int x; int func() { int x; x = 123; } int main() { x = 0; func(); return x; }" 0
test_return "int x; int func() { return x; } int y[4], z; int main() { x = 21; return func(); }" 21

test_return "char c, s[20]; int main() { return 0; }" 0
test_return "int main() { char c, s[20]; return 0; }" 0
test_return "int main() { char c1, c2, c3; c1 = 13; c2 = 65; c3 = c1 + c2; return c3; }" 78
test_return "char f(char c) { return c; } int main() { return f('A'); }" 65
test_stdout "int main() { char s[4]; s[0] = 65; s[1] = 66; s[2] = 67; s[3] = 0; puts(s); return 0; }" "ABC"
test_stdout "char s[8]; int main() { int i; for (i = 0; i < 7; i++) s[i] = i + 65; s[7] = 0; puts(s); return 0; }" "ABCDEFG"
test_stdout "int main() { puts(\"hello world\"); return 0; }" "hello world"
test_stdout "int main() { char *s; s = \"hello world\"; puts(s); return 0; }" "hello world"
test_stdout "int main() { char *s; s = \"hello world\"; printf(\"%c\n\", s[6]); return 0; }" "w"

test_return "int main() { int x = 12; if (1) { int x = 34; } return x; }" 12
test_return "int main() { int x = 12, y = 14; if (1) { int *x = &y; } return x; }" 12
test_stdout "int x = 0; int main() { int x = 1; { int x = 2; { x = 3; } { int x = 4; } printf(\"%d\n\", x); } printf(\"%d\n\", x); }" "3\n1\n"
test_stdout "int main() { int i = 123; for (int i = 0; i < 5; i++) { printf(\"%d\n\", i); int i = 42; } printf(\"%d\n\", i); return 0; }" "0\n1\n2\n3\n4\n123\n"

test_return "struct object { int x, y; }; int main() { struct object p; p.x = 56; return p.x; }" 56
test_return "int main() { struct object { int x, y; }; struct object p; p.x = 56; return p.x; }" 56
test_return "struct object { int x, y; } q; int main() { struct object p; p.x = 56; q.y = 97; return p.x; }" 56
test_return "int main() { struct object { int x, y; } q; struct object p; p.x = 56; q.y = 97; return p.x; }" 56
test_return "typedef int MyInt; int main() { MyInt x = 55; return x; }" 55
test_return "int main() { typedef int MyInt; MyInt x = 55; return x; }" 55
test_return "typedef struct { int x, y; } Vector2; int main() { Vector2 p; p.x = 5; p.y = 3; return p.x * p.y; }" 15
test_return "struct abc { struct abc *p; int v; }; int main() { struct abc x, y; x.p = &y; y.v = 18; return x.p->v; }" 18

test_return "char f(char c, int a); int main() { return f('A', 3); } char f(char c, int a)  { return c + a; }" 68
test_return "int f(), g(int x); int main() { return g(f()); } int f() { return 2; } int g(int x) { return x * x; }" 4
test_stdout "int puts(char *s); int main() { puts(\"123abc\"); }" "123abc"

test_return "enum { U, L, D, R }; int main() { return U; }" 0
test_return "enum { U, L, D, R }; int main() { return L; }" 1
test_return "enum { U, L, D, R }; int main() { return D; }" 2
test_return "enum { U, L, D, R }; int main() { return R; }" 3
test_return "enum { U, L, D, R } d; int main() { d = U; return d; }" 0
test_return "enum { U, L, D, R } d; int main() { d = L; return d; }" 1
test_return "enum { U, L, D, R } d; int main() { d = D; return d; }" 2
test_return "enum { U, L, D, R } d; int main() { d = R; return d; }" 3
test_return "typedef enum { U, L, D, R } Dir; Dir d; int main() { d = D; return d; }" 2
test_return "typedef enum node_type { CONST, MUL, DIV, ADD, SUB } NodeType; int main() { NodeType type = ADD; return ADD; }" 3

test_return "extern int external_obj; int main() { return external_obj; }" 35

test_stdout "int printf(char *format, ...); int main() { printf(\"%s %d\n\", \"abcd\", 1234); }" "abcd 1234"

test_return "extern _Noreturn void error(char *format, ...); int main() { return 0; }" 0

test_stdout "
typedef struct string {
  int size, length;
  char *buffer;
} String;

int main() { return printf(\"%d\n\", sizeof(String)); }
" "16"

test_stdout "
typedef struct vector {
  int size, length;
  void **array;
} Vector;

int main() { return printf(\"%d\n\", sizeof(Vector)); }
" "16"

test_stdout "
typedef struct map {
  int count;
  char *keys[1024];
  void *values[1024];
} Map;

int main() { return printf(\"%d\n\", sizeof(Map)); }
" "16392"

test_stdout "
#define va_start __builtin_va_start
#define va_end __builtin_va_end

typedef struct {
  int gp_offset;
  int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
} va_list[1];

typedef struct _IO_FILE FILE;
extern FILE *stdout;

void print(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  vfprintf(stdout, format, ap);
  va_end(ap);
}

int main() {
  print(\"%s %d\n\", \"abc\", 123);
}
" "abc 123"

test_return "int x = 211; int main() { return x; }" 211
test_stdout "char *s = \"abcde\"; int main() { printf(\"%s\n\", s); }" "abcde"
test_return "int a[4] = { 1, 2, 55, 91 }; int main() { return a[0]; }" 1
test_return "int a[4] = { 1, 2, 55, 91 }; int main() { return a[1]; }" 2
test_return "int a[4] = { 1, 2, 55, 91 }; int main() { return a[2]; }" 55
test_return "int a[4] = { 1, 2, 55, 91 }; int main() { return a[3]; }" 91
test_stdout "char *reg[6] = { \"rdi\", \"rsi\", \"rdx\", \"rcx\", \"r8\", \"r9\" }; int main() { for (int i = 0; i < 6; i++) printf(\"%s\n\", reg[i]); return 0; }" "rdi\nrsi\nrdx\nrcx\nr8\nr9\n"

test_return "int x = 211; int main() { return x; }" 211
test_stdout "char *s = \"abcde\"; int main() { printf(\"%s\n\", s); }" "abcde"
test_return "int a[] = { 1, 2, 55, 91 }; int main() { return a[0]; }" 1
test_return "int a[] = { 1, 2, 55, 91 }; int main() { return a[1]; }" 2
test_return "int a[] = { 1, 2, 55, 91 }; int main() { return a[2]; }" 55
test_return "int a[] = { 1, 2, 55, 91 }; int main() { return a[3]; }" 91
test_return "int main() { int a[] = { 1, 2, 55, 91 }; return a[0]; }" 1
test_return "int main() { int a[] = { 1, 2, 55, 91 }; return a[1]; }" 2
test_return "int main() { int a[] = { 1, 2, 55, 91 }; return a[2]; }" 55
test_return "int main() { int a[] = { 1, 2, 55, 91 }; return a[3]; }" 91
test_stdout "char *reg[] = { \"rdi\", \"rsi\", \"rdx\", \"rcx\", \"r8\", \"r9\" }; int main() { for (int i = 0; i < 6; i++) printf(\"%s\n\", reg[i]); return 0; }" "rdi\nrsi\nrdx\nrcx\nr8\nr9\n"

test_return "
#define bool _Bool
#define true 1
#define false 0
int main() {
  bool b = true;
  return b;
}
" 1
test_return "
#define NULL ((void *) 0)
int main() {
  int *p = NULL;
  return !p;
}
" 1
test_return "
#define xxx 5
int main() {
  return xxx;
}
" 5
test_return "
#define xxx 5
int yyy = 2;
#define zzz 6
int main() {
  return xxx * yyy * zzz;
}
" 60

test_error "int main() { 2 * (3 + 4; }" "tRPAREN is expected."
test_error "int main() { 5 + *; }" "unexpected primary expression."
test_error "int main() { 5 }" "tSEMICOLON is expected."
test_error "int main() { 5 (4); }" "invalid function call."
test_error "int main() { 1 ? 2; }" "tCOLON is expected."
test_error "int main() { 1 = 2 + 3; }" "left side of = operator should be lvalue."
test_error "int main() { func_call(1, 2, 3, 4, 5, 6, 7); }" "too many arguments."
test_error "int main() { func_call(1, 2, 3; }" "tRPAREN is expected."
test_error "int main()" "tSEMICOLON is expected."
test_error "int main() { 2;" "tRBRACE is expected."
test_error "123" "type specifier is expected."
test_error "int f() { 1; } int f() { 2; } int main() { 1; }" "duplicated function or variable definition of 'f'."
test_error "int main(int abc" "tRPAREN is expected."
test_error "int main(int 123) { 0; }" "tIDENTIFIER is expected."
test_error "int main(int x, int x) { 0; }" "duplicated function or variable definition of 'x'."
test_error "int main(int a, int b, int c, int d, int e, int f, int g) { 0; }" "too many parameters."
test_error "int main(x) { return x; }" "type specifier is expected."
test_error "int main() { x; }" "undefined identifier."
test_error "int main() { continue; }" "continue statement should appear in loops."
test_error "int main() { break; }" "break statement should appear in loops."
test_error "int main() { return &123; }" "operand of unary & operator should be identifier."
test_error "int main() { return *123; }" "operand of unary * operator should have pointer type."
test_error "int x, main() { return 0; };" "tSEMICOLON is expected."
test_error "int main() { return 0; }, x;" "type specifier is expected."
test_error "int func() { return x; } int x; int main() { func(); }" "undefined identifier."
test_error "int main() { 1++; }" "operand of postfix ++ operator should be lvalue."
test_error "int f(int x) { int x; }" "duplicated function or variable definition of 'x'."
test_error "int f[4](int x) { int a[4]; return a; }" "returning type of function should not be array type."
test_error "struct abc { struct abc p; }; int main() { return 0; }" "declaration with incomplete type."
test_error "int main() { int a[4] = { 4, 5, 6, 7, 8 }; return 0; }" "too many initializers."

exit 0
