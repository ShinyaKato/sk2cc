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
  movq \$53, -144(%rcx)
  movq -144(%rcx), %rax
  ret
EOS

echo "[OK]"
exit 0
