#!/bin/bash

failed() {
  echo "$1"
  exit 1
}

compile() {
  prog=$1
  echo "$prog" | ./cc > tmp/out.s || failed "failed to compile \"$prog\"."
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
  echo "$prog" | ./cc > /dev/null 2> tmp/stderr.txt && failed "compilation of \"$prog\" was unexpectedly succeeded."
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
test_stdout "int main() { char s[3]; s[0] = 65; s[1] = 66; s[2] = 67; puts(s); return 0; }" "ABC"
test_stdout "char s[8]; int main() { int i; for (i = 0; i < 7; i++) s[i] = i + 65; s[7] = 0; puts(s); return 0; }" "ABCDEFG"
test_stdout "int main() { puts(\"hello world\"); return 0; }" "hello world"
test_stdout "int main() { char *s; s = \"hello world\"; puts(s); return 0; }" "hello world"
test_stdout "int main() { char *s; s = \"hello world\"; printf(\"%c\n\", s[6]); return 0; }" "w"

test_return "int main() { int x = 12; if (1) { int x = 34; } return x; }" 12
test_return "int main() { int x = 12, y = 14; if (1) { int *x = &y; } return x; }" 12
test_stdout "int x = 0; int main() { int x = 1; { int x = 2; { x = 3; } { int x = 4; } printf(\"%d\n\", x); } printf(\"%d\n\", x); }" "3\n1\n"
test_stdout "int main() { int i = 123; for (int i = 0; i < 5; i++) { printf(\"%d\n\", i); int i = 42; } printf(\"%d\n\", i); return 0; }" "0\n1\n2\n3\n4\n123\n"

test_error "int main() { 2 * (3 + 4; }" "tRPAREN is expected."
test_error "int main() { 5 + *; }" "unexpected primary expression."
test_error "int main() { 5 }" "tSEMICOLON is expected."
test_error "int main() { 5 (4); }" "invalid function call."
test_error "int main() { 1 ? 2; }" "tCOLON is expected."
test_error "int main() { 1 = 2 + 3; }" "left side of = operator should be lvalue."
test_error "int main() { func_call(1, 2, 3, 4, 5, 6, 7); }" "too many arguments."
test_error "int main() { func_call(1, 2, 3; }" "tRPAREN is expected."
test_error "int main()" "tLBRACE is expected."
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
test_error "int f(int x[4]) { return 0; }" "type of function parameter should not be array type."
