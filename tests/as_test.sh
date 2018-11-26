#!/bin/bash

count=0

error() {
  echo "Case: #$count"
  echo "[failed]"
  echo "exit with error."
  echo "[input]"
  cat tmp/as_test.s
  echo "[log]"
  cat tmp/as_test.log
  exit 1
}

invalid() {
  echo "Case: #$count"
  echo "[failed]"
  echo "invalid output: failed to link with gcc."
  echo "[input]"
  cat tmp/as_test.s
  echo "[output]"
  objdump -d tmp/as_test.o | sed '/^$/d'
  exit 1
}

failed() {
  expect=$1
  actual=$2
  echo "Case: #$count"
  echo "[failed]"
  echo "$expect is expected, but got $actual."
  echo "[input]"
  cat tmp/as_test.s
  echo "[output]"
  objdump -d tmp/as_test.o | sed '/^$/d'
  exit 1
}

expect() {
  expect=$1
  cat - > tmp/as_test.s
  ./as tmp/as_test.s tmp/as_test.o 2> tmp/as_test.log || error
  gcc tmp/as_test.o -o tmp/as_test || invalid
  ./tmp/as_test
  actual=$?
  [ $actual -ne $expect ] && failed $expect $actual
  count=$((count+1))
}

expect 42 << EOS
main:
  movq \$42, %rax
  ret
EOS

expect 12 << EOS
main:
  movq \$12, %rax
  movq \$23, %rcx
  movq \$34, %rdx
  movq \$45, %rbx
  movq \$56, %rsi
  movq \$67, %rdi
  ret
EOS

expect 12 << EOS
main:
  movq \$12, %rax
  movq \$23, %r8
  movq \$34, %r9
  movq \$45, %r10
  movq \$56, %r11
  movq \$67, %r12
  movq \$78, %r13
  movq \$89, %r14
  movq \$90, %r15
  ret
EOS

expect 34 << EOS
main:
  movq \$34, %rdx
  movq %rdx, %r9
  movq %r9, %rax
  ret
EOS

expect 63 << EOS
main:
  movq \$63, %rbx
  pushq %rbx
  popq %rax
  pushq %r13
  popq %r14
  ret
EOS

expect 13 << EOS
main:
  pushq %rbp
  movq %rsp, %rbp
  movq \$13, %rax
  leave
  ret
EOS

expect 85 << EOS
main:
  movq %rsp, %rdx
  movq (%rdx), %r11
  movq \$85, %rcx
  movq %rcx, (%rdx)
  movq (%rdx), %rax
  movq %r11, (%rdx)
  ret
EOS

expect 84 << EOS
main:
  movq %rsp, %r13
  movq (%r13), %r11
  movq \$84, %rcx
  movq %rcx, (%r13)
  movq (%r13), %rax
  movq %r11, (%r13)
  ret
EOS

expect 39 << EOS
main:
  movq %rsp, %rdx
  movq \$39, %rsi
  movq %rsi, -8(%rdx)
  movq -8(%rdx), %rdi
  movq %rdi, -144(%rdx)
  movq -144(%rdx), %rax
  ret
EOS

expect 46 << EOS
func:
  movq %rdi, %rax
  ret
main:
  movq \$64, %rax
  movq \$46, %rdi
  call func
  ret
EOS

expect 53 << EOS
main:
  movq %rsp, %rcx
  movq \$53, -8(%rcx)
  movq -8(%rcx), %rdx
  movq %rdx, -144(%rcx)
  movq -144(%rcx), %rax
  ret
EOS

expect 16 << EOS
main:
  movq \$16, -8(%rsp)
  movq -8(%rsp), %rcx
  movq %rcx, -144(%rsp)
  movq -144(%rsp), %rax
  ret
EOS

expect 22 << EOS
main:
  movq \$2, %rsi
  movq \$8, %r14
  movq \$22, -32(%rsp, %rsi, 8)
  movq -32(%rsp, %rsi, 8), %rcx
  movq %rcx, -144(%rsp, %r14)
  movq -144(%rsp, %r14), %rax
  ret
EOS

expect 25 << EOS
main:
  movl \$25, %r12d
  movl %r12d, %eax
  ret
EOS

expect 31 << EOS
main:
  movw \$31, %r12w
  movw %r12w, %ax
  ret
EOS

expect 57 << EOS
main:
  movb \$57, %r12b
  movb %r12b, %al
  ret
EOS

expect 87 << EOS
main:
  movb \$87, %sil
  movb %sil, %al
  ret
EOS

gcc as_string.c as_vector.c as_map.c as_binary.c as_error.c as_scan.c as_lex.c as_parse.c as_gen.c as_elf.c tests/as_driver.c -o tmp/as_driver

encoding_failed() {
  asm=$1
  expected=$2
  actual=$3
  echo "[failed]"
  echo "instruction encoding check failed."
  echo "input:    $asm"
  echo "expected: $expected"
  echo "actual:   $actual"
  exit 1
}

test_encoding() {
  asm=$1
  expected=$2
  actual=`echo "$asm" | ./tmp/as_driver`
  [ "$actual" != "$expected" ] && encoding_failed "$asm" "$expected" "$actual"
}

test_encoding 'pushq %rax' '50'
test_encoding 'pushq %rdi' '57'
test_encoding 'pushq %r8' '41 50'
test_encoding 'pushq %r15' '41 57'

test_encoding 'popq %rax' '58'
test_encoding 'popq %rdi' '5f'
test_encoding 'popq %r8' '41 58'
test_encoding 'popq %r15' '41 5f'

test_encoding 'leave' 'c9'

test_encoding 'ret' 'c3'

test_encoding 'movq $42, %rax' '48 c7 c0 2a 00 00 00'
test_encoding 'movq $42, %rsp' '48 c7 c4 2a 00 00 00'
test_encoding 'movq $42, %rbp' '48 c7 c5 2a 00 00 00'
test_encoding 'movq $42, %rdi' '48 c7 c7 2a 00 00 00'
test_encoding 'movq $42, (%rax)' '48 c7 00 2a 00 00 00'
test_encoding 'movq $42, (%rsp)' '48 c7 04 24 2a 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq $42, (%rbp)' '48 c7 45 00 2a 00 00 00' # Mod: 1, disp8: 0
test_encoding 'movq $42, (%rdi)' '48 c7 07 2a 00 00 00'
test_encoding 'movq $42, 8(%rax)' '48 c7 40 08 2a 00 00 00'
test_encoding 'movq $42, 8(%rsp)' '48 c7 44 24 08 2a 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq $42, 8(%rbp)' '48 c7 45 08 2a 00 00 00'
test_encoding 'movq $42, 8(%rdi)' '48 c7 47 08 2a 00 00 00'
test_encoding 'movq $42, 144(%rax)' '48 c7 80 90 00 00 00 2a 00 00 00'
test_encoding 'movq $42, 144(%rsp)' '48 c7 84 24 90 00 00 00 2a 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq $42, 144(%rbp)' '48 c7 85 90 00 00 00 2a 00 00 00'
test_encoding 'movq $42, 144(%rdi)' '48 c7 87 90 00 00 00 2a 00 00 00'

echo "[OK]"
exit 0
