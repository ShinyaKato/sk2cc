#!/bin/bash

[ -d tmp ] || mkdir tmp

failed() {
  echo "$1"
  exit 1
}

compile() {
  prog=$1
  echo "$prog" | ./cc > tmp/out.s || failed "failed to compile \"$prog\"."
  gcc -no-pie tmp/out.s tmp/func_call_stub.o -o tmp/out || failed "failed to link \"$prog\" and stubs."
}

test_prog_retval() {
  prog=$1
  expect=$2
  compile "$prog"
  ./tmp/out
  retval=$?
  [ $retval -ne $expect ] && failed "\"$prog\" should return $expect, but got $retval."
}

test_expr_retval() {
  test_prog_retval "int main() { return $1; }" $2
}

test_stmts_retval() {
  test_prog_retval "int main() { $1 }" $2
}

test_prog_stdout() {
  prog=$1
  expect="`echo -e "$2"`"
  compile "$prog"
  stdout=`"./tmp/out"`
  [ "$stdout" != "$expect" ] && failed "stdout of \"$prog\" should be \"$expect\", but got \"$stdout\"."
}

test_stmts_stdout() {
  test_prog_stdout "int main() { $1 }" "$2"
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

gcc -c tests/func_call_stub.c -o tmp/func_call_stub.o || failed "failed to compile stub file."

test_expr_retval "2" 2
test_expr_retval "71" 71
test_expr_retval "234" 234

test_expr_retval "2 + 3" 5
test_expr_retval "123 + 43 + 1 + 21" 188

test_expr_retval "11 - 7" 4
test_expr_retval "123 - 20 - 3" 100
test_expr_retval "123 + 20 - 3 + 50 - 81" 109

test_expr_retval "3 * 5" 15
test_expr_retval "3 * 5 + 7 * 8 - 3 * 4" 59

test_expr_retval "(3 - 5) * 3 + 7" 1
test_expr_retval "(((123)))" 123

test_expr_retval "6 / 2" 3
test_expr_retval "6 / (2 - 3) + 7" 1
test_expr_retval "123 % 31" 30
test_expr_retval "32 / 4 + 5 * (8 - 5) + 5 % 2" 24

test_expr_retval "3 * 5 == 7 + 8" 1
test_expr_retval "3 * 5 == 123" 0
test_expr_retval "3 * 5 != 7 + 8" 0
test_expr_retval "3 * 5 != 123" 1

test_expr_retval "5 * 7 < 36" 1
test_expr_retval "5 * 7 < 35" 0
test_expr_retval "5 * 7 > 35" 0
test_expr_retval "5 * 7 > 34" 1

test_expr_retval "5 * 7 <= 35" 1
test_expr_retval "5 * 7 <= 34" 0
test_expr_retval "5 * 7 >= 36" 0
test_expr_retval "5 * 7 >= 35" 1

test_expr_retval "1 && 1" 1
test_expr_retval "0 && 1" 0
test_expr_retval "1 && 0" 0
test_expr_retval "0 && 0" 0
test_expr_retval "3 * 7 > 20 && 5 < 10 && 6 + 2 <= 3 * 3 && 5 * 2 % 3 == 1" 1
test_expr_retval "3 * 7 > 20 && 5 < 10 && 6 + 2 >= 3 * 3 && 5 * 2 % 3 == 1" 0

test_expr_retval "1 || 1" 1
test_expr_retval "0 || 1" 1
test_expr_retval "1 || 0" 1
test_expr_retval "0 || 0" 0
test_expr_retval "8 * 7 < 50 || 10 > 2 * 5 || 3 % 2 == 1 || 5 * 7 < 32" 1
test_expr_retval "8 * 7 < 50 || 10 > 2 * 5 || 3 % 2 != 1 || 5 * 7 < 32" 0
test_expr_retval "3 * 7 > 20 && 2 * 4 <= 7 || 8 % 2 == 0 && 5 / 2 >= 2" 1

test_expr_retval "5*4" 20
test_expr_retval "5  *  4" 20
test_expr_retval "  5 * 4  " 20

test_expr_retval "+5" 5
test_expr_retval "5 + (-5)" 0
test_expr_retval "3 - + - + - + - 2" 5

test_expr_retval "!0" 1
test_expr_retval "!1" 0
test_expr_retval "!!0" 0
test_expr_retval "!(2 * 3 <= 2 + 3)" 1

test_expr_retval "3 > 2 ? 13 : 31" 13
test_expr_retval "3 < 2 ? 13 : 31" 31
test_expr_retval "1 ? 1 ? 6 : 7 : 1 ? 8 : 9" 6
test_expr_retval "1 ? 0 ? 6 : 7 : 1 ? 8 : 9" 7
test_expr_retval "0 ? 1 ? 6 : 7 : 1 ? 8 : 9" 8
test_expr_retval "0 ? 1 ? 6 : 7 : 0 ? 8 : 9" 9

test_expr_retval "1 << 6" 64
test_expr_retval "64 >> 6" 1
test_expr_retval "64 >> 8" 0
test_expr_retval "41 << 2" 164
test_expr_retval "41 >> 3" 5

test_expr_retval "183 & 109" 37
test_expr_retval "183 | 109" 255
test_expr_retval "183 ^ 109" 218

test_expr_retval "~183 & 255" 72

test_stmts_retval "789; 456; return 123;" 123
test_stmts_retval "5 + 7 - 9; 3 >= 2 && 4 <= 5; 1 << 2; return (6 -4) * 32;" 64

test_stmts_retval "int x; x = 123; return 100 <= x && x < 200;" 1
test_stmts_retval "int x; x = 3; x = x * x + 1; return x + 3;" 13
test_stmts_retval "int x; return x = 2 * 3 * 4;" 24
test_stmts_retval "int x; return x = x = x = 3;" 3
test_stmts_retval "int x; int y; x = 2; y = x + 5; return y;" 7
test_stmts_retval "int x; int y; int z; return x = y = z = 3;" 3
test_stmts_retval "int x; int y; int z; x = (y = (z = 1) + 2) + 3; return x;" 6
test_stmts_retval "int x; int y; int z; x = (y = (z = 1) + 2) + 3; return y;" 3
test_stmts_retval "int x; int y; int z; x = (y = (z = 1) + 2) + 3; return z;" 1

test_stmts_stdout "func_call();" "in func_call function."
test_stmts_stdout "func_arg_1(1);" "1"
test_stmts_stdout "func_arg_2(1, 2);" "1 2"
test_stmts_stdout "func_arg_3(1, 2, 3);" "1 2 3"
test_stmts_stdout "func_arg_4(1, 2, 3, 4);" "1 2 3 4"
test_stmts_stdout "func_arg_5(1, 2, 3, 4, 5);" "1 2 3 4 5"
test_stmts_stdout "func_arg_6(1, 2, 3, 4, 5, 6);" "1 2 3 4 5 6"
test_stmts_stdout "func_arg_2(2 * 3 < 6 ? 7 : 8, 13 / 2);" "8 6"
test_stmts_stdout "func_arg_1(func_retval(3) + func_retval(4));" "25"

test_prog_retval "int f() { return 2; } int g() { return 3; } int main() { return f() * g(); }" 6
test_prog_retval "int f() { int x; int y; x = 2; y = 3; return x + 2 * y; } int main() { int t; t = f(); return t * 3; }" 24
test_prog_retval "int f() { int x; x = 2; return x * x; } int main() { int t; t = f(); return t * f(); }" 16

test_prog_retval "int f(int x) { return x * x; } int main() { return f(1) + f(2) + f(3); }" 14
test_prog_retval "int f(int x, int y, int z) { x = x * y; y = x + z; return x + y + z; } int main() { return f(1, 2, 3); }" 10

test_stmts_retval "int x; x = 5; if (3 * 4 > 10) { x = 7; } return x;" 7
test_stmts_retval "int x; x = 5; if (3 * 4 < 10) { x = 7; } return x;" 5
test_stmts_retval "int x; x = 5; if (3 * 4 > 10) if (0) x = 7; return x;" 5
test_stmts_retval "int x; if (3 * 4 > 10) x = 3; else x = 4; return x;" 3
test_stmts_retval "int x; if (3 * 4 < 10) x = 3; else x = 4; return x;" 4
test_stmts_retval "int x; if (0) if (0) x = 3; else x = 2; else if (0) x = 1; else x = 0; return x;" 0
test_stmts_retval "int x; if (0) if (0) x = 3; else x = 2; else if (1) x = 1; else x = 0; return x;" 1
test_stmts_retval "int x; if (0) if (1) x = 3; else x = 2; else if (0) x = 1; else x = 0; return x;" 0
test_stmts_retval "int x; if (0) if (1) x = 3; else x = 2; else if (1) x = 1; else x = 0; return x;" 1
test_stmts_retval "int x; if (1) if (0) x = 3; else x = 2; else if (0) x = 1; else x = 0; return x;" 2
test_stmts_retval "int x; if (1) if (0) x = 3; else x = 2; else if (1) x = 1; else x = 0; return x;" 2
test_stmts_retval "int x; if (1) if (1) x = 3; else x = 2; else if (0) x = 1; else x = 0; return x;" 3
test_stmts_retval "int x; if (1) if (1) x = 3; else x = 2; else if (1) x = 1; else x = 0; return x;" 3

test_stmts_retval "int x; int s; x = 1; s = 0; while (x <= 0) { s = s + x; x = x + 1; } return s;" 0
test_stmts_retval "int x; int s; x = 1; s = 0; while (x <= 1) { s = s + x; x = x + 1; } return s;" 1
test_stmts_retval "int x; int s; x = 1; s = 0; while (x <= 10) { s = s + x; x = x + 1; } return s;" 55
test_stmts_retval "int x; int s; x = 1; s = 0; while (x <= 10) { s = s + x; x = x + 1; } return s;" 55
test_stmts_retval "int x; int s; x = 1; s = 0; while (x <= 10) { if (x % 2 == 0) s = s + x; x = x + 1; } return s;" 30
test_stmts_retval "int x; int s; x = 1; s = 0; while (x <= 10) { if (x % 2 == 1) s = s + x; x = x + 1; } return s;" 25

test_stmts_retval "int i; int s; i = 0; s = 0; do { s = s + i; i++; } while (i < 10); return s;" 45
test_stmts_retval "int i; int s; i = 10; s = 0; do { s = s + i; i++; } while (i < 10); return s;" 10

test_stmts_retval "int s; int i; s = 0; i = 0; for (; i < 10;) { s = s + i; i++; } return s;" 45
test_stmts_retval "int s; int i; s = 0; i = 0; for (; i < 10; i++) { s = s + i; } return s;" 45
test_stmts_retval "int s; int i; s = 0; for (i = 0; i < 10;) { s = s + i; i++; } return s;" 45
test_stmts_retval "int s; int i; s = 0; for (i = 0; i < 10; i++) { s = s + i; } return s;" 45

test_stmts_retval "int i; i = 0; for (;; i++) { if (i < 100) continue; break; } return i;" 100
test_stmts_retval "int i; i = 0; do { i++; if (i < 100) continue; break; } while(1); return i;" 100
test_stmts_retval "int i; i = 0; do { i++; if (i < 100) continue; } while(i < 50); return i;" 50
test_stmts_retval "int i; i = 0; while (1) { i++; if (i < 100) continue; break; } return i;" 100

test_stmts_retval "int x; x = 0; return x; x = 1; return x;" 0
test_stmts_retval "int i; for (i = 0; i < 100; i++) if (i == 42) return i; return 123;" 42

test_stmts_retval "int x; x = 7; return *&x;" 7
test_stmts_retval "int x; int y; x = 7; y = 5; return *&x * *&y;" 35

test_stmts_retval "int x; x = 123; int *y; y = &x; return *y;" 123
test_stmts_retval "int x; x = 123; int *y; y = &x; int **z; z = &y; return **z;" 123

test_stmts_retval "int x; x = 123; int *y; y = &x; *y = 231; return x;" 231
test_stmts_retval "int x1; int x2; x1 = 123; x2 = 231; int *y; y = &x1; int **z; z = &y; *z = &x2; *y = 12; return x2;" 12

test_prog_retval "int *f(int *p) { return p; } int main() { int x; x = 123; int *y; y = f(&x); return *y; }" 123
test_prog_retval "int *f(int *p) { *p = 231; return p; } int main() { int x; x = 123; int *y; y = f(&x); return *y; }" 231

test_stmts_stdout "int *x; alloc(&x, 53, 29, 64); print_int(*x);" "53"
test_stmts_stdout "int *x; alloc(&x, 53, 29, 64); print_int(*(x + 1));" "29"
test_stmts_stdout "int *x; alloc(&x, 53, 29, 64); print_int(*(x + 2));" "64"
test_stmts_stdout "int *x; alloc(&x, 53, 29, 64); *(x + 1) = 5; print_int(*(x + 1));" "5"
test_stmts_stdout "int *x; alloc(&x, 53, 29, 64); *(x + 1) = *(x + 2); print_int(*(x + 1));" "64"
test_stmts_stdout "int *x; alloc(&x, 53, 29, 64); int *y; y = x + 2; print_int(*(y - 1));" "29"

test_stmts_retval "int x, *y; x = 2; *y = x + 3; return *y + 4;" 9
test_stmts_retval "int i, x[5]; for (i = 0; i < 5; i++) { *(x + i) = i * i; } return *(x + 0);" 0
test_stmts_retval "int i, x[5]; for (i = 0; i < 5; i++) { *(x + i) = i * i; } return *(x + 2);" 4
test_stmts_retval "int i, x[5]; for (i = 0; i < 5; i++) { *(x + i) = i * i; } return *(x + 4);" 16
test_stmts_retval "int a, *x[4]; a = 2; *(x + 3) = &a; return **(x + 3);" 2
test_stmts_retval "int x[3][4]; **x = 5; return **x;" 5
test_stmts_retval "int x[3][4]; *(*(x + 2) + 3) = 5; return *(*(x + 2) + 3);" 5
test_stmts_retval "int x[3][3], i, j; for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) *(*(x + i) + j) = i * j; return *(*(x + 2) + 1);" 2

test_stmts_stdout "
  int A[3][3], B[3][3], C[3][3];

  A[0][0] = 2; A[0][1] = 3; A[0][2] = 4;
  A[1][0] = 3; A[1][1] = 4; A[1][2] = 5;
  A[2][0] = 4; A[2][1] = 5; A[2][2] = 6;

  B[0][0] = 1; B[0][1] = 2; B[0][2] = 3;
  B[1][0] = 2; B[1][1] = 4; B[1][2] = 6;
  B[2][0] = 3; B[2][1] = 6; B[2][2] = 9;

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      C[i][j] = 0;
      for (int k = 0; k < 3; k++) {
        C[i][j] = C[i][j] + A[i][k] * B[k][j];
      }
    }
  }

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      print_int(C[i][j]);
    }
  }

  return 0;
" "20\n40\n60\n26\n52\n78\n32\n64\n96\n"

test_prog_retval "int x; int main() { return 0; }" 0
test_prog_retval "int x, y[20]; int main() { return 0; }" 0
test_prog_retval "int x; int func() { x = 8; } int main() { func(); return x; }" 8
test_prog_retval "int y[20]; int func() { y[5] = 3; } int main() { func(); return y[5]; }" 3
test_prog_retval "int x; int func(int *p) { *p = 123; } int main() { func(&x); return x; }" 123
test_prog_retval "int x, *y; int func() { *y = 123; } int main() { y = &x; func(); return x; }" 123
test_prog_retval "int x; int func() { int x; x = 123; } int main() { x = 0; func(); return x; }" 0
test_prog_retval "int x; int func() { return x; } int y[4], z; int main() { x = 21; return func(); }" 21

test_prog_retval "char c, s[20]; int main() { return 0; }" 0
test_prog_retval "int main() { char c, s[20]; return 0; }" 0
test_prog_retval "int main() { char c1, c2, c3; c1 = 13; c2 = 65; c3 = c1 + c2; return c3; }" 78
test_prog_stdout "int main() { char s[3]; s[0] = 65; s[1] = 66; s[2] = 67; print_string(s); return 0; }" "ABC"
test_prog_stdout "char s[8]; int main() { int i; for (i = 0; i < 7; i++) s[i] = i + 65; s[7] = 0; puts(s); return 0; }" "ABCDEFG"
test_prog_stdout "int main() { puts(\"hello world\"); return 0; }" "hello world"
test_prog_stdout "int main() { char *s; s = \"hello world\"; puts(s); return 0; }" "hello world"
test_prog_stdout "int main() { char *s; s = \"hello world\"; print_char(s[6]); return 0; }" "w"

test_prog_retval "int main() { int x; x = 1; return x++; }" 1
test_prog_retval "int main() { int x; x = 1; return ++x; }" 2
test_prog_retval "int main() { int x; x = 1; x++; return x; }" 2
test_prog_retval "int main() { int x; x = 1; ++x; return x; }" 2
test_prog_retval "int main() { int x; x = 1; return x--; }" 1
test_prog_retval "int main() { int x; x = 1; return --x; }" 0
test_prog_retval "int main() { int x; x = 1; x--; return x; }" 0
test_prog_retval "int main() { int x; x = 1; --x; return x; }" 0
test_prog_retval "int main() { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; return *(p++); }" 92
test_prog_retval "int main() { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; return *(++p); }" 93
test_prog_retval "int main() { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; p++; return *p; }" 93
test_prog_retval "int main() { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; ++p; return *p; }" 93
test_prog_retval "int main() { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; return *(p--); }" 92
test_prog_retval "int main() { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; return *(--p); }" 91
test_prog_retval "int main() { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; p--; return *p; }" 91
test_prog_retval "int main() { int *p, a[3]; a[0] = 91; a[1] = 92; a[2] = 93; p = a + 1; --p; return *p; }" 91

test_prog_retval "int main() { return sizeof(123); }" 4
test_prog_retval "int main() { return sizeof 123; }" 4
test_prog_retval "int main() { return sizeof 123 + 13; }" 17
test_prog_retval "int main() { char x; return sizeof(x); }" 1
test_prog_retval "int main() { int x; return sizeof(x); }" 4
test_prog_retval "int main() { int *x; return sizeof(x); }" 8
test_prog_retval "int main() { int **x; return sizeof(x); }" 8
test_prog_retval "int main() { int x[4]; return sizeof(x); }" 16
test_prog_retval "int main() { int x[4][5]; return sizeof(x); }" 80
test_prog_retval "int main() { int *x[4][5]; return sizeof(x); }" 160
test_prog_retval "int main() { int x[4][5]; return sizeof(x[0]); }" 20
test_prog_retval "int main() { char x; return sizeof(\"abc\"); }" 4
test_prog_retval "int main() { char *x; x = \"abc\"; return sizeof(x); }" 8

test_prog_retval "int main() { int x = 5; return x; }" 5
test_prog_retval "int main() { int x = 5, y = x + 3; return y; }" 8
test_prog_retval "int main() { int x = 5; int y = x + 3; return y; }" 8
test_prog_retval "int main() { int x = 3, y = 5, z = x + y * 8; return z; }" 43
test_prog_retval "int main() { int x = 42, *y = &x; return *y; }" 42

test_prog_retval "int main() { int x = 12; if (1) { int x = 34; } return x; }" 12
test_prog_retval "int main() { int x = 12, y = 14; if (1) { int *x = &y; } return x; }" 12
test_prog_stdout "
int x = 0;
int main() {
  int x = 1;
  {
    int x = 2;
    { x = 3; }
    { int x = 4; }
    printf(\"%d\n\", x);
  }
  printf(\"%d\n\", x);
}" "3\n1\n"

test_prog_stdout "
int main() {
  int i = 123;
  for (int i = 0; i < 5; i++) {
    printf(\"%d\n\", i);
    int i = 42;
  }
  printf(\"%d\n\", i);
  return 0;
}" "0\n1\n2\n3\n4\n123\n"

test_prog_retval "int main() { int x = 2; x += 3; return x; }" 5
test_prog_retval "int main() { int A[3]; A[0] = 30; A[1] = 31; A[2] = 32; int *p = A; p += 2; return *p; }" 32
test_prog_retval "int main() { int x = 5; x -= 3; return x; }" 2
test_prog_retval "int main() { int A[3]; A[0] = 30; A[1] = 31; A[2] = 32; int *p = A + 2; p -= 2; return *p; }" 30

test_prog_retval "int main() { return 'A'; }" 65
test_prog_retval "int main() { return '\\n'; }" 10
test_prog_retval "int main() { return '\\''; }" 39

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
test_error "int main(int x, int x) { 0; }" "duplicated parameter declaration."
test_error "int main(int a, int b, int c, int d, int e, int f, int g) { 0; }" "too many parameters."
test_error "int main(x) { return x; }" "tINT is expected."
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
