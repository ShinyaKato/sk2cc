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
  gcc -static tmp/as_test.o -o tmp/as_test || invalid
  ./tmp/as_test > /dev/null
  actual=$?
  [ $actual -ne $expect ] && failed $expect $actual
  count=$((count+1))
}

expect 42 << EOS
  .global main
main:
  movq \$42, %rax
  ret
EOS

expect 12 << EOS
  .global main
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
  .global main
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
  .global main
main:
  movq \$34, %rdx
  movq %rdx, %r9
  movq %r9, %rax
  ret
EOS

expect 63 << EOS
  .global main
main:
  movq \$63, %rbx
  pushq %rbx
  popq %rax
  pushq %r13
  popq %r14
  ret
EOS

expect 13 << EOS
  .global main
main:
  pushq %rbp
  movq %rsp, %rbp
  movq \$13, %rax
  leave
  ret
EOS

expect 85 << EOS
  .global main
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
  .global main
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
  .global main
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
  .global func
func:
  movq %rdi, %rax
  ret
  .global main
main:
  movq \$64, %rax
  movq \$46, %rdi
  call func
  ret
EOS

expect 53 << EOS
  .global main
main:
  movq %rsp, %rcx
  movq \$53, -8(%rcx)
  movq -8(%rcx), %rdx
  movq %rdx, -144(%rcx)
  movq -144(%rcx), %rax
  ret
EOS

expect 16 << EOS
  .global main
main:
  movq \$16, -8(%rsp)
  movq -8(%rsp), %rcx
  movq %rcx, -144(%rsp)
  movq -144(%rsp), %rax
  ret
EOS

expect 22 << EOS
  .global main
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
  .global main
main:
  movl \$25, %r12d
  movl %r12d, %eax
  ret
EOS

expect 31 << EOS
  .global main
main:
  movw \$31, %r12w
  movw %r12w, %ax
  ret
EOS

expect 57 << EOS
  .global main
main:
  movb \$57, %r12b
  movb %r12b, %al
  ret
EOS

expect 87 << EOS
  .global main
main:
  movb \$87, %sil
  movb %sil, %al
  ret
EOS

expect 67 << EOS
  .global main
main:
  pushq %rbp
  movq %rsp, %rbp
  leaq -8(%rbp), %rcx
  movq \$67, (%rcx)
  movq (%rcx), %rax
  leave
  ret
EOS

expect 82 << EOS
  .global main
main:
  pushq %rbp
  movq %rsp, %rbp
  subq \$8, %rsp
  leaq -8(%rbp), %rcx
  movl \$82, %eax
  movl %eax, (%rcx)
  leaq -8(%rbp), %rax
  movl (%rax), %eax
  leave
  ret
EOS

expect 34 << EOS
  .global main
main:
  pushq %rbp
  movq %rsp, %rbp
  subq \$16, %rsp
  leaq -16(%rbp), %rax
  pushq %rax
  movl \$2, %eax
  movq \$4, %rcx
  mulq %rcx
  popq %rcx
  addq %rax, %rcx
  pushq %rcx
  movl \$34, %eax
  popq %rcx
  movl %eax, (%rcx)
  leaq -16(%rbp), %rax
  pushq %rax
  movl \$2, %eax
  movq \$4, %rcx
  mulq %rcx
  popq %rcx
  addq %rax, %rcx
  movq %rcx, %rax
  movl (%rax), %eax
  leave
  ret
EOS

expect 15 << EOS
  .global main
main:
  pushq %rbp
  movq %rsp, %rbp
  movl \$3, %eax
  movl \$5, %ecx
  imull %ecx
  leave
  ret
EOS

expect 11 << EOS
  .global main
main:
  pushq %rbp
  movq %rsp, %rbp
  movl \$33, %eax
  movl \$3, %ecx
  movl \$0, %edx
  idivl %ecx
  leave
  ret
EOS

expect 12 << EOS
  .global main
main:
  pushq %rbp
  movq %rsp, %rbp
  subq \$16, %rsp
  movb \$72, -16(%rbp)
  movb \$101, -15(%rbp)
  movb \$108, -14(%rbp)
  movb \$108, -13(%rbp)
  movb \$111, -12(%rbp)
  movb \$32, -11(%rbp)
  movb \$87, -10(%rbp)
  movb \$111, -9(%rbp)
  movb \$114, -8(%rbp)
  movb \$108, -7(%rbp)
  movb \$100, -6(%rbp)
  movb \$10, -5(%rbp)
  movb \$0, -4(%rbp)
  leaq -16(%rbp), %rdi
  movq \$0, %rax
  call printf
  leave
  ret
EOS

expect 87 << EOS
  .global main
main:
  movq \$87, %rax
  jmp .L0
  movq \$0, %rax
.L0:
  ret
EOS

expect 54 << EOS
  .global main
main:
  pushq %rbp
  movq %rsp, %rbp
  subq \$0, %rsp
  movl \$54, %eax
  pushq %rax
  popq %rax
  jmp .L0
.L0:
  leave
  ret
EOS

expect 1 << EOS
  .global main
main:
  movl \$42, %eax
  cmpq \$0, %rax
  je .L1
  movl \$1, %eax
  jmp .L0
.L1:
  movl \$0, %eax
.L0:
  ret
EOS

expect 0 << EOS
  .global main
main:
  movl \$42, %eax
  cmpq \$0, %rax
  jne .L1
  movl \$1, %eax
  jmp .L0
.L1:
  movl \$0, %eax
.L0:
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

# pushq %r64
test_encoding 'pushq %rax' '50'
test_encoding 'pushq %rsp' '54'
test_encoding 'pushq %rbp' '55'
test_encoding 'pushq %rdi' '57'
test_encoding 'pushq %r8' '41 50'
test_encoding 'pushq %r12' '41 54'
test_encoding 'pushq %r13' '41 55'
test_encoding 'pushq %r15' '41 57'

# popq %r64
test_encoding 'popq %rax' '58'
test_encoding 'popq %rsp' '5c'
test_encoding 'popq %rbp' '5d'
test_encoding 'popq %rdi' '5f'
test_encoding 'popq %r8' '41 58'
test_encoding 'popq %r12' '41 5c'
test_encoding 'popq %r13' '41 5d'
test_encoding 'popq %r15' '41 5f'

# leave
test_encoding 'leave' 'c9'

# ret
test_encoding 'ret' 'c3'

# movq $imm32, %r64
test_encoding 'movq $42, %rax' '48 c7 c0 2a 00 00 00'
test_encoding 'movq $42, %rsp' '48 c7 c4 2a 00 00 00'
test_encoding 'movq $42, %rbp' '48 c7 c5 2a 00 00 00'
test_encoding 'movq $42, %rdi' '48 c7 c7 2a 00 00 00'
test_encoding 'movq $42, %r8' '49 c7 c0 2a 00 00 00'
test_encoding 'movq $42, %r12' '49 c7 c4 2a 00 00 00'
test_encoding 'movq $42, %r13' '49 c7 c5 2a 00 00 00'
test_encoding 'movq $42, %r15' '49 c7 c7 2a 00 00 00'

# movq $imm32, (%r64)
test_encoding 'movq $42, (%rax)' '48 c7 00 2a 00 00 00'
test_encoding 'movq $42, (%rsp)' '48 c7 04 24 2a 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq $42, (%rbp)' '48 c7 45 00 2a 00 00 00' # Mod: 1, disp8: 0
test_encoding 'movq $42, (%rdi)' '48 c7 07 2a 00 00 00'
test_encoding 'movq $42, (%r8)' '49 c7 00 2a 00 00 00'
test_encoding 'movq $42, (%r12)' '49 c7 04 24 2a 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq $42, (%r13)' '49 c7 45 00 2a 00 00 00' # Mod: 1, disp8: 0
test_encoding 'movq $42, (%r15)' '49 c7 07 2a 00 00 00'

# movq $imm32, disp8(%r64)
test_encoding 'movq $42, 8(%rax)' '48 c7 40 08 2a 00 00 00'
test_encoding 'movq $42, 8(%rsp)' '48 c7 44 24 08 2a 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq $42, 8(%rbp)' '48 c7 45 08 2a 00 00 00'
test_encoding 'movq $42, 8(%rdi)' '48 c7 47 08 2a 00 00 00'
test_encoding 'movq $42, 8(%r8)' '49 c7 40 08 2a 00 00 00'
test_encoding 'movq $42, 8(%r12)' '49 c7 44 24 08 2a 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq $42, 8(%r13)' '49 c7 45 08 2a 00 00 00'
test_encoding 'movq $42, 8(%r15)' '49 c7 47 08 2a 00 00 00'

# movq $imm32, disp32(%r64)
test_encoding 'movq $42, 144(%rax)' '48 c7 80 90 00 00 00 2a 00 00 00'
test_encoding 'movq $42, 144(%rsp)' '48 c7 84 24 90 00 00 00 2a 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq $42, 144(%rbp)' '48 c7 85 90 00 00 00 2a 00 00 00'
test_encoding 'movq $42, 144(%rdi)' '48 c7 87 90 00 00 00 2a 00 00 00'
test_encoding 'movq $42, 144(%r8)' '49 c7 80 90 00 00 00 2a 00 00 00'
test_encoding 'movq $42, 144(%r12)' '49 c7 84 24 90 00 00 00 2a 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq $42, 144(%r13)' '49 c7 85 90 00 00 00 2a 00 00 00'
test_encoding 'movq $42, 144(%r15)' '49 c7 87 90 00 00 00 2a 00 00 00'

# movq $imm32, (%rax, %r64)
test_encoding 'movq $42, (%rax, %rax)' '48 c7 04 00 2a 00 00 00'
test_encoding 'movq $42, (%rax, %rbp)' '48 c7 04 28 2a 00 00 00'
test_encoding 'movq $42, (%rax, %rdi)' '48 c7 04 38 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r8)' '4a c7 04 00 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r12)' '4a c7 04 20 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r13)' '4a c7 04 28 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r15)' '4a c7 04 38 2a 00 00 00'

# movq $imm32, (%rsp, %r64)
test_encoding 'movq $42, (%rsp, %rax)' '48 c7 04 04 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %rbp)' '48 c7 04 2c 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %rdi)' '48 c7 04 3c 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r8)' '4a c7 04 04 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r12)' '4a c7 04 24 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r13)' '4a c7 04 2c 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r15)' '4a c7 04 3c 2a 00 00 00'

# movq $imm32, (%rbp, %r64)
test_encoding 'movq $42, (%rbp, %rax)' '48 c7 44 05 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %rbp)' '48 c7 44 2d 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %rdi)' '48 c7 44 3d 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r8)' '4a c7 44 05 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r12)' '4a c7 44 25 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r13)' '4a c7 44 2d 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r15)' '4a c7 44 3d 00 2a 00 00 00'

# movq $imm32, (%rdi, %r64)
test_encoding 'movq $42, (%rdi, %rax)' '48 c7 04 07 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %rbp)' '48 c7 04 2f 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %rdi)' '48 c7 04 3f 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r8)' '4a c7 04 07 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r12)' '4a c7 04 27 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r13)' '4a c7 04 2f 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r15)' '4a c7 04 3f 2a 00 00 00'

# movq $imm32, (%8, %r64)
test_encoding 'movq $42, (%r8, %rax)' '49 c7 04 00 2a 00 00 00'
test_encoding 'movq $42, (%r8, %rbp)' '49 c7 04 28 2a 00 00 00'
test_encoding 'movq $42, (%r8, %rdi)' '49 c7 04 38 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r8)' '4b c7 04 00 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r12)' '4b c7 04 20 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r13)' '4b c7 04 28 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r15)' '4b c7 04 38 2a 00 00 00'

# movq $imm32, (%r12, %r64)
test_encoding 'movq $42, (%r12, %rax)' '49 c7 04 04 2a 00 00 00'
test_encoding 'movq $42, (%r12, %rbp)' '49 c7 04 2c 2a 00 00 00'
test_encoding 'movq $42, (%r12, %rdi)' '49 c7 04 3c 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r8)' '4b c7 04 04 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r12)' '4b c7 04 24 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r13)' '4b c7 04 2c 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r15)' '4b c7 04 3c 2a 00 00 00'

# movq $imm32, (%r13, %r64)
test_encoding 'movq $42, (%r13, %rax)' '49 c7 44 05 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %rbp)' '49 c7 44 2d 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %rdi)' '49 c7 44 3d 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r8)' '4b c7 44 05 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r12)' '4b c7 44 25 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r13)' '4b c7 44 2d 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r15)' '4b c7 44 3d 00 2a 00 00 00'

# movq $imm32, (%r15, %r64)
test_encoding 'movq $42, (%r15, %rax)' '49 c7 04 07 2a 00 00 00'
test_encoding 'movq $42, (%r15, %rbp)' '49 c7 04 2f 2a 00 00 00'
test_encoding 'movq $42, (%r15, %rdi)' '49 c7 04 3f 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r8)' '4b c7 04 07 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r12)' '4b c7 04 27 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r13)' '4b c7 04 2f 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r15)' '4b c7 04 3f 2a 00 00 00'

# movq $imm32, (%rax, %r64, 2)
test_encoding 'movq $42, (%rax, %rax, 2)' '48 c7 04 40 2a 00 00 00'
test_encoding 'movq $42, (%rax, %rbp, 2)' '48 c7 04 68 2a 00 00 00'
test_encoding 'movq $42, (%rax, %rdi, 2)' '48 c7 04 78 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r8, 2)' '4a c7 04 40 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r12, 2)' '4a c7 04 60 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r13, 2)' '4a c7 04 68 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r15, 2)' '4a c7 04 78 2a 00 00 00'

# movq $imm32, (%rsp, %r64, 2)
test_encoding 'movq $42, (%rsp, %rax, 2)' '48 c7 04 44 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %rbp, 2)' '48 c7 04 6c 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %rdi, 2)' '48 c7 04 7c 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r8, 2)' '4a c7 04 44 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r12, 2)' '4a c7 04 64 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r13, 2)' '4a c7 04 6c 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r15, 2)' '4a c7 04 7c 2a 00 00 00'

# movq $imm32, (%rbp, %r64, 2)
test_encoding 'movq $42, (%rbp, %rax, 2)' '48 c7 44 45 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %rbp, 2)' '48 c7 44 6d 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %rdi, 2)' '48 c7 44 7d 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r8, 2)' '4a c7 44 45 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r12, 2)' '4a c7 44 65 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r13, 2)' '4a c7 44 6d 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r15, 2)' '4a c7 44 7d 00 2a 00 00 00'

# movq $imm32, (%rdi, %r64, 2)
test_encoding 'movq $42, (%rdi, %rax, 2)' '48 c7 04 47 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %rbp, 2)' '48 c7 04 6f 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %rdi, 2)' '48 c7 04 7f 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r8, 2)' '4a c7 04 47 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r12, 2)' '4a c7 04 67 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r13, 2)' '4a c7 04 6f 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r15, 2)' '4a c7 04 7f 2a 00 00 00'

# movq $imm32, (%8, %r64, 2)
test_encoding 'movq $42, (%r8, %rax, 2)' '49 c7 04 40 2a 00 00 00'
test_encoding 'movq $42, (%r8, %rbp, 2)' '49 c7 04 68 2a 00 00 00'
test_encoding 'movq $42, (%r8, %rdi, 2)' '49 c7 04 78 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r8, 2)' '4b c7 04 40 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r12, 2)' '4b c7 04 60 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r13, 2)' '4b c7 04 68 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r15, 2)' '4b c7 04 78 2a 00 00 00'

# movq $imm32, (%r12, %r64, 2)
test_encoding 'movq $42, (%r12, %rax, 2)' '49 c7 04 44 2a 00 00 00'
test_encoding 'movq $42, (%r12, %rbp, 2)' '49 c7 04 6c 2a 00 00 00'
test_encoding 'movq $42, (%r12, %rdi, 2)' '49 c7 04 7c 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r8, 2)' '4b c7 04 44 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r12, 2)' '4b c7 04 64 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r13, 2)' '4b c7 04 6c 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r15, 2)' '4b c7 04 7c 2a 00 00 00'

# movq $imm32, (%r13, %r64, 2)
test_encoding 'movq $42, (%r13, %rax, 2)' '49 c7 44 45 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %rbp, 2)' '49 c7 44 6d 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %rdi, 2)' '49 c7 44 7d 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r8, 2)' '4b c7 44 45 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r12, 2)' '4b c7 44 65 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r13, 2)' '4b c7 44 6d 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r15, 2)' '4b c7 44 7d 00 2a 00 00 00'

# movq $imm32, (%r15, %r64, 2)
test_encoding 'movq $42, (%r15, %rax, 2)' '49 c7 04 47 2a 00 00 00'
test_encoding 'movq $42, (%r15, %rbp, 2)' '49 c7 04 6f 2a 00 00 00'
test_encoding 'movq $42, (%r15, %rdi, 2)' '49 c7 04 7f 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r8, 2)' '4b c7 04 47 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r12, 2)' '4b c7 04 67 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r13, 2)' '4b c7 04 6f 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r15, 2)' '4b c7 04 7f 2a 00 00 00'

# movq $imm32, (%rax, %r64, 4)
test_encoding 'movq $42, (%rax, %rax, 4)' '48 c7 04 80 2a 00 00 00'
test_encoding 'movq $42, (%rax, %rbp, 4)' '48 c7 04 a8 2a 00 00 00'
test_encoding 'movq $42, (%rax, %rdi, 4)' '48 c7 04 b8 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r8, 4)' '4a c7 04 80 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r12, 4)' '4a c7 04 a0 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r13, 4)' '4a c7 04 a8 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r15, 4)' '4a c7 04 b8 2a 00 00 00'

# movq $imm32, (%rsp, %r64, 4)
test_encoding 'movq $42, (%rsp, %rax, 4)' '48 c7 04 84 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %rbp, 4)' '48 c7 04 ac 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %rdi, 4)' '48 c7 04 bc 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r8, 4)' '4a c7 04 84 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r12, 4)' '4a c7 04 a4 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r13, 4)' '4a c7 04 ac 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r15, 4)' '4a c7 04 bc 2a 00 00 00'

# movq $imm32, (%rbp, %r64, 4)
test_encoding 'movq $42, (%rbp, %rax, 4)' '48 c7 44 85 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %rbp, 4)' '48 c7 44 ad 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %rdi, 4)' '48 c7 44 bd 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r8, 4)' '4a c7 44 85 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r12, 4)' '4a c7 44 a5 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r13, 4)' '4a c7 44 ad 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r15, 4)' '4a c7 44 bd 00 2a 00 00 00'

# movq $imm32, (%rdi, %r64, 4)
test_encoding 'movq $42, (%rdi, %rax, 4)' '48 c7 04 87 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %rbp, 4)' '48 c7 04 af 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %rdi, 4)' '48 c7 04 bf 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r8, 4)' '4a c7 04 87 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r12, 4)' '4a c7 04 a7 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r13, 4)' '4a c7 04 af 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r15, 4)' '4a c7 04 bf 2a 00 00 00'

# movq $imm32, (%8, %r64, 4)
test_encoding 'movq $42, (%r8, %rax, 4)' '49 c7 04 80 2a 00 00 00'
test_encoding 'movq $42, (%r8, %rbp, 4)' '49 c7 04 a8 2a 00 00 00'
test_encoding 'movq $42, (%r8, %rdi, 4)' '49 c7 04 b8 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r8, 4)' '4b c7 04 80 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r12, 4)' '4b c7 04 a0 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r13, 4)' '4b c7 04 a8 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r15, 4)' '4b c7 04 b8 2a 00 00 00'

# movq $imm32, (%r12, %r64, 4)
test_encoding 'movq $42, (%r12, %rax, 4)' '49 c7 04 84 2a 00 00 00'
test_encoding 'movq $42, (%r12, %rbp, 4)' '49 c7 04 ac 2a 00 00 00'
test_encoding 'movq $42, (%r12, %rdi, 4)' '49 c7 04 bc 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r8, 4)' '4b c7 04 84 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r12, 4)' '4b c7 04 a4 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r13, 4)' '4b c7 04 ac 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r15, 4)' '4b c7 04 bc 2a 00 00 00'

# movq $imm32, (%r13, %r64, 4)
test_encoding 'movq $42, (%r13, %rax, 4)' '49 c7 44 85 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %rbp, 4)' '49 c7 44 ad 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %rdi, 4)' '49 c7 44 bd 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r8, 4)' '4b c7 44 85 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r12, 4)' '4b c7 44 a5 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r13, 4)' '4b c7 44 ad 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r15, 4)' '4b c7 44 bd 00 2a 00 00 00'

# movq $imm32, (%r15, %r64, 4)
test_encoding 'movq $42, (%r15, %rax, 4)' '49 c7 04 87 2a 00 00 00'
test_encoding 'movq $42, (%r15, %rbp, 4)' '49 c7 04 af 2a 00 00 00'
test_encoding 'movq $42, (%r15, %rdi, 4)' '49 c7 04 bf 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r8, 4)' '4b c7 04 87 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r12, 4)' '4b c7 04 a7 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r13, 4)' '4b c7 04 af 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r15, 4)' '4b c7 04 bf 2a 00 00 00'

# movq $imm32, (%rax, %r64, 8)
test_encoding 'movq $42, (%rax, %rax, 8)' '48 c7 04 c0 2a 00 00 00'
test_encoding 'movq $42, (%rax, %rbp, 8)' '48 c7 04 e8 2a 00 00 00'
test_encoding 'movq $42, (%rax, %rdi, 8)' '48 c7 04 f8 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r8, 8)' '4a c7 04 c0 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r12, 8)' '4a c7 04 e0 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r13, 8)' '4a c7 04 e8 2a 00 00 00'
test_encoding 'movq $42, (%rax, %r15, 8)' '4a c7 04 f8 2a 00 00 00'

# movq $imm32, (%rsp, %r64, 8)
test_encoding 'movq $42, (%rsp, %rax, 8)' '48 c7 04 c4 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %rbp, 8)' '48 c7 04 ec 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %rdi, 8)' '48 c7 04 fc 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r8, 8)' '4a c7 04 c4 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r12, 8)' '4a c7 04 e4 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r13, 8)' '4a c7 04 ec 2a 00 00 00'
test_encoding 'movq $42, (%rsp, %r15, 8)' '4a c7 04 fc 2a 00 00 00'

# movq $imm32, (%rbp, %r64, 8)
test_encoding 'movq $42, (%rbp, %rax, 8)' '48 c7 44 c5 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %rbp, 8)' '48 c7 44 ed 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %rdi, 8)' '48 c7 44 fd 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r8, 8)' '4a c7 44 c5 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r12, 8)' '4a c7 44 e5 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r13, 8)' '4a c7 44 ed 00 2a 00 00 00'
test_encoding 'movq $42, (%rbp, %r15, 8)' '4a c7 44 fd 00 2a 00 00 00'

# movq $imm32, (%rdi, %r64, 8)
test_encoding 'movq $42, (%rdi, %rax, 8)' '48 c7 04 c7 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %rbp, 8)' '48 c7 04 ef 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %rdi, 8)' '48 c7 04 ff 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r8, 8)' '4a c7 04 c7 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r12, 8)' '4a c7 04 e7 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r13, 8)' '4a c7 04 ef 2a 00 00 00'
test_encoding 'movq $42, (%rdi, %r15, 8)' '4a c7 04 ff 2a 00 00 00'

# movq $imm32, (%8, %r64, 8)
test_encoding 'movq $42, (%r8, %rax, 8)' '49 c7 04 c0 2a 00 00 00'
test_encoding 'movq $42, (%r8, %rbp, 8)' '49 c7 04 e8 2a 00 00 00'
test_encoding 'movq $42, (%r8, %rdi, 8)' '49 c7 04 f8 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r8, 8)' '4b c7 04 c0 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r12, 8)' '4b c7 04 e0 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r13, 8)' '4b c7 04 e8 2a 00 00 00'
test_encoding 'movq $42, (%r8, %r15, 8)' '4b c7 04 f8 2a 00 00 00'

# movq $imm32, (%r12, %r64, 8)
test_encoding 'movq $42, (%r12, %rax, 8)' '49 c7 04 c4 2a 00 00 00'
test_encoding 'movq $42, (%r12, %rbp, 8)' '49 c7 04 ec 2a 00 00 00'
test_encoding 'movq $42, (%r12, %rdi, 8)' '49 c7 04 fc 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r8, 8)' '4b c7 04 c4 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r12, 8)' '4b c7 04 e4 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r13, 8)' '4b c7 04 ec 2a 00 00 00'
test_encoding 'movq $42, (%r12, %r15, 8)' '4b c7 04 fc 2a 00 00 00'

# movq $imm32, (%r13, %r64, 8)
test_encoding 'movq $42, (%r13, %rax, 8)' '49 c7 44 c5 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %rbp, 8)' '49 c7 44 ed 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %rdi, 8)' '49 c7 44 fd 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r8, 8)' '4b c7 44 c5 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r12, 8)' '4b c7 44 e5 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r13, 8)' '4b c7 44 ed 00 2a 00 00 00'
test_encoding 'movq $42, (%r13, %r15, 8)' '4b c7 44 fd 00 2a 00 00 00'

# movq $imm32, (%r15, %r64, 8)
test_encoding 'movq $42, (%r15, %rax, 8)' '49 c7 04 c7 2a 00 00 00'
test_encoding 'movq $42, (%r15, %rbp, 8)' '49 c7 04 ef 2a 00 00 00'
test_encoding 'movq $42, (%r15, %rdi, 8)' '49 c7 04 ff 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r8, 8)' '4b c7 04 c7 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r12, 8)' '4b c7 04 e7 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r13, 8)' '4b c7 04 ef 2a 00 00 00'
test_encoding 'movq $42, (%r15, %r15, 8)' '4b c7 04 ff 2a 00 00 00'

# movq %rcx, %r64
test_encoding 'movq %rcx, %rax' '48 89 c8'
test_encoding 'movq %rcx, %rsp' '48 89 cc'
test_encoding 'movq %rcx, %rbp' '48 89 cd'
test_encoding 'movq %rcx, %rdi' '48 89 cf'
test_encoding 'movq %rcx, %r8' '49 89 c8'
test_encoding 'movq %rcx, %r12' '49 89 cc'
test_encoding 'movq %rcx, %r13' '49 89 cd'
test_encoding 'movq %rcx, %r15' '49 89 cf'

# movq %r9, %r64
test_encoding 'movq %r9, %rax' '4c 89 c8'
test_encoding 'movq %r9, %rsp' '4c 89 cc'
test_encoding 'movq %r9, %rbp' '4c 89 cd'
test_encoding 'movq %r9, %rdi' '4c 89 cf'
test_encoding 'movq %r9, %r8' '4d 89 c8'
test_encoding 'movq %r9, %r12' '4d 89 cc'
test_encoding 'movq %r9, %r13' '4d 89 cd'
test_encoding 'movq %r9, %r15' '4d 89 cf'

# movq %r64, %rcx
test_encoding 'movq %rax, %rcx' '48 89 c1'
test_encoding 'movq %rsp, %rcx' '48 89 e1'
test_encoding 'movq %rbp, %rcx' '48 89 e9'
test_encoding 'movq %rdi, %rcx' '48 89 f9'
test_encoding 'movq %r8, %rcx' '4c 89 c1'
test_encoding 'movq %r12, %rcx' '4c 89 e1'
test_encoding 'movq %r13, %rcx' '4c 89 e9'
test_encoding 'movq %r15, %rcx' '4c 89 f9'

# movq %r64, %r9
test_encoding 'movq %rax, %r9' '49 89 c1'
test_encoding 'movq %rsp, %r9' '49 89 e1'
test_encoding 'movq %rbp, %r9' '49 89 e9'
test_encoding 'movq %rdi, %r9' '49 89 f9'
test_encoding 'movq %r8, %r9' '4d 89 c1'
test_encoding 'movq %r12, %r9' '4d 89 e1'
test_encoding 'movq %r13, %r9' '4d 89 e9'
test_encoding 'movq %r15, %r9' '4d 89 f9'

# movq %rcx, (%r64)
test_encoding 'movq %rcx, (%rax)' '48 89 08'
test_encoding 'movq %rcx, (%rsp)' '48 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq %rcx, (%rbp)' '48 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movq %rcx, (%rdi)' '48 89 0f'
test_encoding 'movq %rcx, (%r8)' '49 89 08'
test_encoding 'movq %rcx, (%r12)' '49 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq %rcx, (%r13)' '49 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movq %rcx, (%r15)' '49 89 0f'

# movq %rcx, (%rsp, %r64)
test_encoding 'movq %rcx, (%rsp, %rax)' '48 89 0c 04'
test_encoding 'movq %rcx, (%rsp, %rbp)' '48 89 0c 2c'
test_encoding 'movq %rcx, (%rsp, %rdi)' '48 89 0c 3c'
test_encoding 'movq %rcx, (%rsp, %r8)' '4a 89 0c 04'
test_encoding 'movq %rcx, (%rsp, %r12)' '4a 89 0c 24'
test_encoding 'movq %rcx, (%rsp, %r13)' '4a 89 0c 2c'
test_encoding 'movq %rcx, (%rsp, %r15)' '4a 89 0c 3c'

# movq %rcx, (%rbp, %r64)
test_encoding 'movq %rcx, (%rbp, %rax)' '48 89 4c 05 00'
test_encoding 'movq %rcx, (%rbp, %rbp)' '48 89 4c 2d 00'
test_encoding 'movq %rcx, (%rbp, %rdi)' '48 89 4c 3d 00'
test_encoding 'movq %rcx, (%rbp, %r8)' '4a 89 4c 05 00'
test_encoding 'movq %rcx, (%rbp, %r12)' '4a 89 4c 25 00'
test_encoding 'movq %rcx, (%rbp, %r13)' '4a 89 4c 2d 00'
test_encoding 'movq %rcx, (%rbp, %r15)' '4a 89 4c 3d 00'

# movq %rcx, (%r12, %r64)
test_encoding 'movq %rcx, (%r12, %rax)' '49 89 0c 04'
test_encoding 'movq %rcx, (%r12, %rbp)' '49 89 0c 2c'
test_encoding 'movq %rcx, (%r12, %rdi)' '49 89 0c 3c'
test_encoding 'movq %rcx, (%r12, %r8)' '4b 89 0c 04'
test_encoding 'movq %rcx, (%r12, %r12)' '4b 89 0c 24'
test_encoding 'movq %rcx, (%r12, %r13)' '4b 89 0c 2c'
test_encoding 'movq %rcx, (%r12, %r15)' '4b 89 0c 3c'

# movq %rcx, (%r13, %r64)
test_encoding 'movq %rcx, (%r13, %rax)' '49 89 4c 05 00'
test_encoding 'movq %rcx, (%r13, %rbp)' '49 89 4c 2d 00'
test_encoding 'movq %rcx, (%r13, %rdi)' '49 89 4c 3d 00'
test_encoding 'movq %rcx, (%r13, %r8)' '4b 89 4c 05 00'
test_encoding 'movq %rcx, (%r13, %r12)' '4b 89 4c 25 00'
test_encoding 'movq %rcx, (%r13, %r13)' '4b 89 4c 2d 00'
test_encoding 'movq %rcx, (%r13, %r15)' '4b 89 4c 3d 00'

# movq %r9, (%r64)
test_encoding 'movq %r9, (%rax)' '4c 89 08'
test_encoding 'movq %r9, (%rsp)' '4c 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq %r9, (%rbp)' '4c 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movq %r9, (%rdi)' '4c 89 0f'
test_encoding 'movq %r9, (%r8)' '4d 89 08'
test_encoding 'movq %r9, (%r12)' '4d 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq %r9, (%r13)' '4d 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movq %r9, (%r15)' '4d 89 0f'

# movq %r9, (%rsp, %r64)
test_encoding 'movq %r9, (%rsp, %rax)' '4c 89 0c 04'
test_encoding 'movq %r9, (%rsp, %rbp)' '4c 89 0c 2c'
test_encoding 'movq %r9, (%rsp, %rdi)' '4c 89 0c 3c'
test_encoding 'movq %r9, (%rsp, %r8)' '4e 89 0c 04'
test_encoding 'movq %r9, (%rsp, %r12)' '4e 89 0c 24'
test_encoding 'movq %r9, (%rsp, %r13)' '4e 89 0c 2c'
test_encoding 'movq %r9, (%rsp, %r15)' '4e 89 0c 3c'

# movq %r9, (%rbp, %r64)
test_encoding 'movq %r9, (%rbp, %rax)' '4c 89 4c 05 00'
test_encoding 'movq %r9, (%rbp, %rbp)' '4c 89 4c 2d 00'
test_encoding 'movq %r9, (%rbp, %rdi)' '4c 89 4c 3d 00'
test_encoding 'movq %r9, (%rbp, %r8)' '4e 89 4c 05 00'
test_encoding 'movq %r9, (%rbp, %r12)' '4e 89 4c 25 00'
test_encoding 'movq %r9, (%rbp, %r13)' '4e 89 4c 2d 00'
test_encoding 'movq %r9, (%rbp, %r15)' '4e 89 4c 3d 00'

# movq %r9, (%r12, %r64)
test_encoding 'movq %r9, (%r12, %rax)' '4d 89 0c 04'
test_encoding 'movq %r9, (%r12, %rbp)' '4d 89 0c 2c'
test_encoding 'movq %r9, (%r12, %rdi)' '4d 89 0c 3c'
test_encoding 'movq %r9, (%r12, %r8)' '4f 89 0c 04'
test_encoding 'movq %r9, (%r12, %r12)' '4f 89 0c 24'
test_encoding 'movq %r9, (%r12, %r13)' '4f 89 0c 2c'
test_encoding 'movq %r9, (%r12, %r15)' '4f 89 0c 3c'

# movq %r9, (%r13, %r64)
test_encoding 'movq %r9, (%r13, %rax)' '4d 89 4c 05 00'
test_encoding 'movq %r9, (%r13, %rbp)' '4d 89 4c 2d 00'
test_encoding 'movq %r9, (%r13, %rdi)' '4d 89 4c 3d 00'
test_encoding 'movq %r9, (%r13, %r8)' '4f 89 4c 05 00'
test_encoding 'movq %r9, (%r13, %r12)' '4f 89 4c 25 00'
test_encoding 'movq %r9, (%r13, %r13)' '4f 89 4c 2d 00'
test_encoding 'movq %r9, (%r13, %r15)' '4f 89 4c 3d 00'

# movq (%r64), %rcx
test_encoding 'movq (%rax), %rcx' '48 8b 08'
test_encoding 'movq (%rsp), %rcx' '48 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq (%rbp), %rcx' '48 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movq (%rdi), %rcx' '48 8b 0f'
test_encoding 'movq (%r8), %rcx' '49 8b 08'
test_encoding 'movq (%r12), %rcx' '49 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq (%r13), %rcx' '49 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movq (%r15), %rcx' '49 8b 0f'

# movq (%rsp, %r64), %rcx
test_encoding 'movq (%rsp, %rax), %rcx' '48 8b 0c 04'
test_encoding 'movq (%rsp, %rbp), %rcx' '48 8b 0c 2c'
test_encoding 'movq (%rsp, %rdi), %rcx' '48 8b 0c 3c'
test_encoding 'movq (%rsp, %r8), %rcx'  '4a 8b 0c 04'
test_encoding 'movq (%rsp, %r12), %rcx' '4a 8b 0c 24'
test_encoding 'movq (%rsp, %r13), %rcx' '4a 8b 0c 2c'
test_encoding 'movq (%rsp, %r15), %rcx' '4a 8b 0c 3c'

# movq (%rbp, %r64), %rcx
test_encoding 'movq (%rbp, %rax), %rcx' '48 8b 4c 05 00'
test_encoding 'movq (%rbp, %rbp), %rcx' '48 8b 4c 2d 00'
test_encoding 'movq (%rbp, %rdi), %rcx' '48 8b 4c 3d 00'
test_encoding 'movq (%rbp, %r8), %rcx' '4a 8b 4c 05 00'
test_encoding 'movq (%rbp, %r12), %rcx' '4a 8b 4c 25 00'
test_encoding 'movq (%rbp, %r13), %rcx' '4a 8b 4c 2d 00'
test_encoding 'movq (%rbp, %r15), %rcx' '4a 8b 4c 3d 00'

# movq (%r12, %r64), %rcx
test_encoding 'movq (%r12, %rax), %rcx' '49 8b 0c 04'
test_encoding 'movq (%r12, %rbp), %rcx' '49 8b 0c 2c'
test_encoding 'movq (%r12, %rdi), %rcx' '49 8b 0c 3c'
test_encoding 'movq (%r12, %r8), %rcx' '4b 8b 0c 04'
test_encoding 'movq (%r12, %r12), %rcx' '4b 8b 0c 24'
test_encoding 'movq (%r12, %r13), %rcx' '4b 8b 0c 2c'
test_encoding 'movq (%r12, %r15), %rcx' '4b 8b 0c 3c'

# movq (%r13, %r64), %rcx
test_encoding 'movq (%r13, %rax), %rcx' '49 8b 4c 05 00'
test_encoding 'movq (%r13, %rbp), %rcx' '49 8b 4c 2d 00'
test_encoding 'movq (%r13, %rdi), %rcx' '49 8b 4c 3d 00'
test_encoding 'movq (%r13, %r8), %rcx' '4b 8b 4c 05 00'
test_encoding 'movq (%r13, %r12), %rcx' '4b 8b 4c 25 00'
test_encoding 'movq (%r13, %r13), %rcx' '4b 8b 4c 2d 00'
test_encoding 'movq (%r13, %r15), %rcx' '4b 8b 4c 3d 00'

# movq (%r64), %r9
test_encoding 'movq (%rax), %r9' '4c 8b 08'
test_encoding 'movq (%rsp), %r9' '4c 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq (%rbp), %r9' '4c 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movq (%rdi), %r9' '4c 8b 0f'
test_encoding 'movq (%r8), %r9' '4d 8b 08'
test_encoding 'movq (%r12), %r9' '4d 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movq (%r13), %r9' '4d 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movq (%r15), %r9' '4d 8b 0f'

# movq (%rsp, %r64), %r9
test_encoding 'movq (%rsp, %rax), %r9' '4c 8b 0c 04'
test_encoding 'movq (%rsp, %rbp), %r9' '4c 8b 0c 2c'
test_encoding 'movq (%rsp, %rdi), %r9' '4c 8b 0c 3c'
test_encoding 'movq (%rsp, %r8), %r9' '4e 8b 0c 04'
test_encoding 'movq (%rsp, %r12), %r9' '4e 8b 0c 24'
test_encoding 'movq (%rsp, %r13), %r9' '4e 8b 0c 2c'
test_encoding 'movq (%rsp, %r15), %r9' '4e 8b 0c 3c'

# movq (%rbp, %r64), %r9
test_encoding 'movq (%rbp, %rax), %r9' '4c 8b 4c 05 00'
test_encoding 'movq (%rbp, %rbp), %r9' '4c 8b 4c 2d 00'
test_encoding 'movq (%rbp, %rdi), %r9' '4c 8b 4c 3d 00'
test_encoding 'movq (%rbp, %r8), %r9' '4e 8b 4c 05 00'
test_encoding 'movq (%rbp, %r12), %r9' '4e 8b 4c 25 00'
test_encoding 'movq (%rbp, %r13), %r9' '4e 8b 4c 2d 00'
test_encoding 'movq (%rbp, %r15), %r9' '4e 8b 4c 3d 00'

# movq (%r12, %r64), %r9
test_encoding 'movq (%r12, %rax), %r9' '4d 8b 0c 04'
test_encoding 'movq (%r12, %rbp), %r9' '4d 8b 0c 2c'
test_encoding 'movq (%r12, %rdi), %r9' '4d 8b 0c 3c'
test_encoding 'movq (%r12, %r8), %r9' '4f 8b 0c 04'
test_encoding 'movq (%r12, %r12), %r9' '4f 8b 0c 24'
test_encoding 'movq (%r12, %r13), %r9' '4f 8b 0c 2c'
test_encoding 'movq (%r12, %r15), %r9' '4f 8b 0c 3c'

# movq (%r13, %r64), %r9
test_encoding 'movq (%r13, %rax), %r9' '4d 8b 4c 05 00'
test_encoding 'movq (%r13, %rbp), %r9' '4d 8b 4c 2d 00'
test_encoding 'movq (%r13, %rdi), %r9' '4d 8b 4c 3d 00'
test_encoding 'movq (%r13, %r8), %r9' '4f 8b 4c 05 00'
test_encoding 'movq (%r13, %r12), %r9' '4f 8b 4c 25 00'
test_encoding 'movq (%r13, %r13), %r9' '4f 8b 4c 2d 00'
test_encoding 'movq (%r13, %r15), %r9' '4f 8b 4c 3d 00'

# movl $imm32, %r32
test_encoding 'movl $42, %eax' 'c7 c0 2a 00 00 00'
test_encoding 'movl $42, %esp' 'c7 c4 2a 00 00 00'
test_encoding 'movl $42, %ebp' 'c7 c5 2a 00 00 00'
test_encoding 'movl $42, %edi' 'c7 c7 2a 00 00 00'
test_encoding 'movl $42, %r8d' '41 c7 c0 2a 00 00 00'
test_encoding 'movl $42, %r12d' '41 c7 c4 2a 00 00 00'
test_encoding 'movl $42, %r13d' '41 c7 c5 2a 00 00 00'
test_encoding 'movl $42, %r15d' '41 c7 c7 2a 00 00 00'

# movl %ecx, %r32
test_encoding 'movl %ecx, %eax' '89 c8'
test_encoding 'movl %ecx, %esp' '89 cc'
test_encoding 'movl %ecx, %ebp' '89 cd'
test_encoding 'movl %ecx, %edi' '89 cf'
test_encoding 'movl %ecx, %r8d' '41 89 c8'
test_encoding 'movl %ecx, %r12d' '41 89 cc'
test_encoding 'movl %ecx, %r13d' '41 89 cd'
test_encoding 'movl %ecx, %r15d' '41 89 cf'

# movl %r9d, %r32
test_encoding 'movl %r9d, %eax' '44 89 c8'
test_encoding 'movl %r9d, %esp' '44 89 cc'
test_encoding 'movl %r9d, %ebp' '44 89 cd'
test_encoding 'movl %r9d, %edi' '44 89 cf'
test_encoding 'movl %r9d, %r8d' '45 89 c8'
test_encoding 'movl %r9d, %r12d' '45 89 cc'
test_encoding 'movl %r9d, %r13d' '45 89 cd'
test_encoding 'movl %r9d, %r15d' '45 89 cf'

# movl %r32, %ecx
test_encoding 'movl %eax, %ecx' '89 c1'
test_encoding 'movl %esp, %ecx' '89 e1'
test_encoding 'movl %ebp, %ecx' '89 e9'
test_encoding 'movl %edi, %ecx' '89 f9'
test_encoding 'movl %r8d, %ecx' '44 89 c1'
test_encoding 'movl %r12d, %ecx' '44 89 e1'
test_encoding 'movl %r13d, %ecx' '44 89 e9'
test_encoding 'movl %r15d, %ecx' '44 89 f9'

# movl %r32, %r9d
test_encoding 'movl %eax, %r9d' '41 89 c1'
test_encoding 'movl %esp, %r9d' '41 89 e1'
test_encoding 'movl %ebp, %r9d' '41 89 e9'
test_encoding 'movl %edi, %r9d' '41 89 f9'
test_encoding 'movl %r8d, %r9d' '45 89 c1'
test_encoding 'movl %r12d, %r9d' '45 89 e1'
test_encoding 'movl %r13d, %r9d' '45 89 e9'
test_encoding 'movl %r15d, %r9d' '45 89 f9'

# movl (%r64), %ecx
test_encoding 'movl (%rax), %ecx' '8b 08'
test_encoding 'movl (%rsp), %ecx' '8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movl (%rbp), %ecx' '8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movl (%rdi), %ecx' '8b 0f'
test_encoding 'movl (%r8), %ecx' '41 8b 08'
test_encoding 'movl (%r12), %ecx' '41 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movl (%r13), %ecx' '41 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movl (%r15), %ecx' '41 8b 0f'

# movl (%r64), %r9d
test_encoding 'movl (%rax), %r9d' '44 8b 08'
test_encoding 'movl (%rsp), %r9d' '44 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movl (%rbp), %r9d' '44 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movl (%rdi), %r9d' '44 8b 0f'
test_encoding 'movl (%r8), %r9d' '45 8b 08'
test_encoding 'movl (%r12), %r9d' '45 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movl (%r13), %r9d' '45 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movl (%r15), %r9d' '45 8b 0f'

# movl %ecx, (%r64)
test_encoding 'movl %ecx, (%rax)' '89 08'
test_encoding 'movl %ecx, (%rsp)' '89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movl %ecx, (%rbp)' '89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movl %ecx, (%rdi)' '89 0f'
test_encoding 'movl %ecx, (%r8)' '41 89 08'
test_encoding 'movl %ecx, (%r12)' '41 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movl %ecx, (%r13)' '41 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movl %ecx, (%r15)' '41 89 0f'

# movl %r9d, (%r64)
test_encoding 'movl %r9d, (%rax)' '44 89 08'
test_encoding 'movl %r9d, (%rsp)' '44 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movl %r9d, (%rbp)' '44 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movl %r9d, (%rdi)' '44 89 0f'
test_encoding 'movl %r9d, (%r8)' '45 89 08'
test_encoding 'movl %r9d, (%r12)' '45 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movl %r9d, (%r13)' '45 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movl %r9d, (%r15)' '45 89 0f'

# movw $imm16, %r16
test_encoding 'movw $42, %ax' '66 c7 c0 2a 00'
test_encoding 'movw $42, %sp' '66 c7 c4 2a 00'
test_encoding 'movw $42, %bp' '66 c7 c5 2a 00'
test_encoding 'movw $42, %di' '66 c7 c7 2a 00'
test_encoding 'movw $42, %r8w' '66 41 c7 c0 2a 00'
test_encoding 'movw $42, %r12w' '66 41 c7 c4 2a 00'
test_encoding 'movw $42, %r13w' '66 41 c7 c5 2a 00'
test_encoding 'movw $42, %r15w' '66 41 c7 c7 2a 00'

# movw %cx, %r16
test_encoding 'movw %cx, %ax' '66 89 c8'
test_encoding 'movw %cx, %sp' '66 89 cc'
test_encoding 'movw %cx, %bp' '66 89 cd'
test_encoding 'movw %cx, %di' '66 89 cf'
test_encoding 'movw %cx, %r8w' '66 41 89 c8'
test_encoding 'movw %cx, %r12w' '66 41 89 cc'
test_encoding 'movw %cx, %r13w' '66 41 89 cd'
test_encoding 'movw %cx, %r15w' '66 41 89 cf'

# movw %r9w, %r16
test_encoding 'movw %r9w, %ax' '66 44 89 c8'
test_encoding 'movw %r9w, %sp' '66 44 89 cc'
test_encoding 'movw %r9w, %bp' '66 44 89 cd'
test_encoding 'movw %r9w, %di' '66 44 89 cf'
test_encoding 'movw %r9w, %r8w' '66 45 89 c8'
test_encoding 'movw %r9w, %r12w' '66 45 89 cc'
test_encoding 'movw %r9w, %r13w' '66 45 89 cd'
test_encoding 'movw %r9w, %r15w' '66 45 89 cf'

# movw %r16, %cx
test_encoding 'movw %ax, %cx' '66 89 c1'
test_encoding 'movw %sp, %cx' '66 89 e1'
test_encoding 'movw %bp, %cx' '66 89 e9'
test_encoding 'movw %di, %cx' '66 89 f9'
test_encoding 'movw %r8w, %cx' '66 44 89 c1'
test_encoding 'movw %r12w, %cx' '66 44 89 e1'
test_encoding 'movw %r13w, %cx' '66 44 89 e9'
test_encoding 'movw %r15w, %cx' '66 44 89 f9'

# movw %r16, %r9w
test_encoding 'movw %ax, %r9w' '66 41 89 c1'
test_encoding 'movw %sp, %r9w' '66 41 89 e1'
test_encoding 'movw %bp, %r9w' '66 41 89 e9'
test_encoding 'movw %di, %r9w' '66 41 89 f9'
test_encoding 'movw %r8w, %r9w' '66 45 89 c1'
test_encoding 'movw %r12w, %r9w' '66 45 89 e1'
test_encoding 'movw %r13w, %r9w' '66 45 89 e9'
test_encoding 'movw %r15w, %r9w' '66 45 89 f9'

# movw (%r64), %cx
test_encoding 'movw (%rax), %cx' '66 8b 08'
test_encoding 'movw (%rsp), %cx' '66 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movw (%rbp), %cx' '66 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movw (%rdi), %cx' '66 8b 0f'
test_encoding 'movw (%r8), %cx' '66 41 8b 08'
test_encoding 'movw (%r12), %cx' '66 41 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movw (%r13), %cx' '66 41 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movw (%r15), %cx' '66 41 8b 0f'

# movw (%r64), %r9w
test_encoding 'movw (%rax), %r9w' '66 44 8b 08'
test_encoding 'movw (%rsp), %r9w' '66 44 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movw (%rbp), %r9w' '66 44 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movw (%rdi), %r9w' '66 44 8b 0f'
test_encoding 'movw (%r8), %r9w' '66 45 8b 08'
test_encoding 'movw (%r12), %r9w' '66 45 8b 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movw (%r13), %r9w' '66 45 8b 4d 00' # Mod: 1, disp8: 0
test_encoding 'movw (%r15), %r9w' '66 45 8b 0f'

# movw %cx, (%r64)
test_encoding 'movw %cx, (%rax)' '66 89 08'
test_encoding 'movw %cx, (%rsp)' '66 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movw %cx, (%rbp)' '66 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movw %cx, (%rdi)' '66 89 0f'
test_encoding 'movw %cx, (%r8)' '66 41 89 08'
test_encoding 'movw %cx, (%r12)' '66 41 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movw %cx, (%r13)' '66 41 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movw %cx, (%r15)' '66 41 89 0f'

# movw %r9w, (%r64)
test_encoding 'movw %r9w, (%rax)' '66 44 89 08'
test_encoding 'movw %r9w, (%rsp)' '66 44 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movw %r9w, (%rbp)' '66 44 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movw %r9w, (%rdi)' '66 44 89 0f'
test_encoding 'movw %r9w, (%r8)' '66 45 89 08'
test_encoding 'movw %r9w, (%r12)' '66 45 89 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movw %r9w, (%r13)' '66 45 89 4d 00' # Mod: 1, disp8: 0
test_encoding 'movw %r9w, (%r15)' '66 45 89 0f'

# movb $imm8, %r8
test_encoding 'movb $42, %al' 'c6 c0 2a'
test_encoding 'movb $42, %spl' '40 c6 c4 2a'
test_encoding 'movb $42, %bpl' '40 c6 c5 2a'
test_encoding 'movb $42, %dil' '40 c6 c7 2a'
test_encoding 'movb $42, %r8b' '41 c6 c0 2a'
test_encoding 'movb $42, %r12b' '41 c6 c4 2a'
test_encoding 'movb $42, %r13b' '41 c6 c5 2a'
test_encoding 'movb $42, %r15b' '41 c6 c7 2a'

# movb %cl, %r8
test_encoding 'movb %cl, %al' '88 c8'
test_encoding 'movb %cl, %spl' '40 88 cc'
test_encoding 'movb %cl, %bpl' '40 88 cd'
test_encoding 'movb %cl, %dil' '40 88 cf'
test_encoding 'movb %cl, %r8b' '41 88 c8'
test_encoding 'movb %cl, %r12b' '41 88 cc'
test_encoding 'movb %cl, %r13b' '41 88 cd'
test_encoding 'movb %cl, %r15b' '41 88 cf'

# movb %r9b, %r8
test_encoding 'movb %r9b, %al' '44 88 c8'
test_encoding 'movb %r9b, %spl' '44 88 cc'
test_encoding 'movb %r9b, %bpl' '44 88 cd'
test_encoding 'movb %r9b, %dil' '44 88 cf'
test_encoding 'movb %r9b, %r8b' '45 88 c8'
test_encoding 'movb %r9b, %r12b' '45 88 cc'
test_encoding 'movb %r9b, %r13b' '45 88 cd'
test_encoding 'movb %r9b, %r15b' '45 88 cf'

# movb %r8, %cl
test_encoding 'movb %al, %cl' '88 c1'
test_encoding 'movb %spl, %cl' '40 88 e1'
test_encoding 'movb %bpl, %cl' '40 88 e9'
test_encoding 'movb %dil, %cl' '40 88 f9'
test_encoding 'movb %r8b, %cl' '44 88 c1'
test_encoding 'movb %r12b, %cl' '44 88 e1'
test_encoding 'movb %r13b, %cl' '44 88 e9'
test_encoding 'movb %r15b, %cl' '44 88 f9'

# movb %r8, %r9b
test_encoding 'movb %al, %r9b' '41 88 c1'
test_encoding 'movb %spl, %r9b' '41 88 e1'
test_encoding 'movb %bpl, %r9b' '41 88 e9'
test_encoding 'movb %dil, %r9b' '41 88 f9'
test_encoding 'movb %r8b, %r9b' '45 88 c1'
test_encoding 'movb %r12b, %r9b' '45 88 e1'
test_encoding 'movb %r13b, %r9b' '45 88 e9'
test_encoding 'movb %r15b, %r9b' '45 88 f9'

# movb (%r64), %cb
test_encoding 'movb (%rax), %cl' '8a 08'
test_encoding 'movb (%rsp), %cl' '8a 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movb (%rbp), %cl' '8a 4d 00' # Mod: 1, displ8: 0
test_encoding 'movb (%rdi), %cl' '8a 0f'
test_encoding 'movb (%r8), %cl' '41 8a 08'
test_encoding 'movb (%r12), %cl' '41 8a 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movb (%r13), %cl' '41 8a 4d 00' # Mod: 1, displ8: 0
test_encoding 'movb (%r15), %cl' '41 8a 0f'

# movb (%r64), %r9b
test_encoding 'movb (%rax), %r9b' '44 8a 08'
test_encoding 'movb (%rsp), %r9b' '44 8a 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movb (%rbp), %r9b' '44 8a 4d 00' # Mod: 1, displ8: 0
test_encoding 'movb (%rdi), %r9b' '44 8a 0f'
test_encoding 'movb (%r8), %r9b' '45 8a 08'
test_encoding 'movb (%r12), %r9b' '45 8a 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movb (%r13), %r9b' '45 8a 4d 00' # Mod: 1, displ8: 0
test_encoding 'movb (%r15), %r9b' '45 8a 0f'

# movb %cl, (%r64)
test_encoding 'movb %cl, (%rax)' '88 08'
test_encoding 'movb %cl, (%rsp)' '88 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movb %cl, (%rbp)' '88 4d 00' # Mod: 1, displ8: 0
test_encoding 'movb %cl, (%rdi)' '88 0f'
test_encoding 'movb %cl, (%r8)' '41 88 08'
test_encoding 'movb %cl, (%r12)' '41 88 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movb %cl, (%r13)' '41 88 4d 00' # Mod: 1, displ8: 0
test_encoding 'movb %cl, (%r15)' '41 88 0f'

# movb %r9b, (%r64)
test_encoding 'movb %r9b, (%rax)' '44 88 08'
test_encoding 'movb %r9b, (%rsp)' '44 88 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movb %r9b, (%rbp)' '44 88 4d 00' # Mod: 1, displ8: 0
test_encoding 'movb %r9b, (%rdi)' '44 88 0f'
test_encoding 'movb %r9b, (%r8)' '45 88 08'
test_encoding 'movb %r9b, (%r12)' '45 88 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'movb %r9b, (%r13)' '45 88 4d 00' # Mod: 1, displ8: 0
test_encoding 'movb %r9b, (%r15)' '45 88 0f'

# leaq (%r64), %rcx
test_encoding 'leaq (%rax), %rcx' '48 8d 08'
test_encoding 'leaq (%rsp), %rcx' '48 8d 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq (%rbp), %rcx' '48 8d 4d 00' # Mod: 1, disp8: 0
test_encoding 'leaq (%rdi), %rcx' '48 8d 0f'
test_encoding 'leaq (%r8), %rcx' '49 8d 08'
test_encoding 'leaq (%r12), %rcx' '49 8d 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq (%r13), %rcx' '49 8d 4d 00' # Mod: 1, disp8: 0
test_encoding 'leaq (%r15), %rcx' '49 8d 0f'

# leaq disp8(%r64), %rcx
test_encoding 'leaq 8(%rax), %rcx' '48 8d 48 08'
test_encoding 'leaq 8(%rsp), %rcx' '48 8d 4c 24 08' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq 8(%rbp), %rcx' '48 8d 4d 08' # Mod: 1, disp8: 0
test_encoding 'leaq 8(%rdi), %rcx' '48 8d 4f 08'
test_encoding 'leaq 8(%r8), %rcx' '49 8d 48 08'
test_encoding 'leaq 8(%r12), %rcx' '49 8d 4c 24 08' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq 8(%r13), %rcx' '49 8d 4d 08' # Mod: 1, disp8: 0
test_encoding 'leaq 8(%r15), %rcx' '49 8d 4f 08'

# leaq disp32(%r64), %rcx
test_encoding 'leaq 144(%rax), %rcx' '48 8d 88 90 00 00 00'
test_encoding 'leaq 144(%rsp), %rcx' '48 8d 8c 24 90 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq 144(%rbp), %rcx' '48 8d 8d 90 00 00 00' # Mod: 1, disp8: 0
test_encoding 'leaq 144(%rdi), %rcx' '48 8d 8f 90 00 00 00'
test_encoding 'leaq 144(%r8), %rcx' '49 8d 88 90 00 00 00'
test_encoding 'leaq 144(%r12), %rcx' '49 8d 8c 24 90 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq 144(%r13), %rcx' '49 8d 8d 90 00 00 00' # Mod: 1, disp8: 0
test_encoding 'leaq 144(%r15), %rcx' '49 8d 8f 90 00 00 00'

# leaq (%r64), %r9
test_encoding 'leaq (%rax), %r9' '4c 8d 08'
test_encoding 'leaq (%rsp), %r9' '4c 8d 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq (%rbp), %r9' '4c 8d 4d 00' # Mod: 1, disp8: 0
test_encoding 'leaq (%rdi), %r9' '4c 8d 0f'
test_encoding 'leaq (%r8), %r9' '4d 8d 08'
test_encoding 'leaq (%r12), %r9' '4d 8d 0c 24' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq (%r13), %r9' '4d 8d 4d 00' # Mod: 1, disp8: 0
test_encoding 'leaq (%r15), %r9' '4d 8d 0f'

# leaq disp8(%r64), %r9
test_encoding 'leaq 8(%rax), %r9' '4c 8d 48 08'
test_encoding 'leaq 8(%rsp), %r9' '4c 8d 4c 24 08' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq 8(%rbp), %r9' '4c 8d 4d 08' # Mod: 1, disp8: 0
test_encoding 'leaq 8(%rdi), %r9' '4c 8d 4f 08'
test_encoding 'leaq 8(%r8), %r9' '4d 8d 48 08'
test_encoding 'leaq 8(%r12), %r9' '4d 8d 4c 24 08' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq 8(%r13), %r9' '4d 8d 4d 08' # Mod: 1, disp8: 0
test_encoding 'leaq 8(%r15), %r9' '4d 8d 4f 08'

# leaq disp32(%r64), %r9
test_encoding 'leaq 144(%rax), %r9' '4c 8d 88 90 00 00 00'
test_encoding 'leaq 144(%rsp), %r9' '4c 8d 8c 24 90 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq 144(%rbp), %r9' '4c 8d 8d 90 00 00 00' # Mod: 1, disp8: 0
test_encoding 'leaq 144(%rdi), %r9' '4c 8d 8f 90 00 00 00'
test_encoding 'leaq 144(%r8), %r9' '4d 8d 88 90 00 00 00'
test_encoding 'leaq 144(%r12), %r9' '4d 8d 8c 24 90 00 00 00' # Scale: 0, Index: 4, Base: 4
test_encoding 'leaq 144(%r13), %r9' '4d 8d 8d 90 00 00 00' # Mod: 1, disp8: 0
test_encoding 'leaq 144(%r15), %r9' '4d 8d 8f 90 00 00 00'

# addq
test_encoding 'addq $42, %rdx' '48 81 c2 2a 00 00 00'
test_encoding 'addq $42, (%rdx)' '48 81 02 2a 00 00 00'
test_encoding 'addq %rcx, %rdx' '48 01 ca'
test_encoding 'addq %rcx, (%rdx)' '48 01 0a'
test_encoding 'addq (%rdx), %rcx' '48 03 0a'

# addl
test_encoding 'addl $42, %edx' '81 c2 2a 00 00 00'
test_encoding 'addl $42, (%rdx)' '81 02 2a 00 00 00'
test_encoding 'addl %ecx, %edx' '01 ca'
test_encoding 'addl %ecx, (%rdx)' '01 0a'
test_encoding 'addl (%rdx), %ecx' '03 0a'

# subq
test_encoding 'subq $42, %rdx' '48 81 ea 2a 00 00 00'
test_encoding 'subq $42, (%rdx)' '48 81 2a 2a 00 00 00'
test_encoding 'subq %rcx, %rdx' '48 29 ca'
test_encoding 'subq %rcx, (%rdx)' '48 29 0a'
test_encoding 'subq (%rdx), %rcx' '48 2b 0a'

# subl
test_encoding 'subl $42, %edx' '81 ea 2a 00 00 00'
test_encoding 'subl $42, (%rdx)' '81 2a 2a 00 00 00'
test_encoding 'subl %ecx, %edx' '29 ca'
test_encoding 'subl %ecx, (%rdx)' '29 0a'
test_encoding 'subl (%rdx), %ecx' '2b 0a'

# mulq
test_encoding 'mulq %rdx' '48 f7 e2'
test_encoding 'mulq (%rdx)' '48 f7 22'

# mull
test_encoding 'mull %edx' 'f7 e2'
test_encoding 'mull (%rdx)' 'f7 22'

# imulq
test_encoding 'imulq %rdx' '48 f7 ea'
test_encoding 'imulq (%rdx)' '48 f7 2a'

# imull
test_encoding 'imull %edx' 'f7 ea'
test_encoding 'imull (%rdx)' 'f7 2a'

# divq
test_encoding 'divq %rdx' '48 f7 f2'
test_encoding 'divq (%rdx)' '48 f7 32'

# divl
test_encoding 'divl %edx' 'f7 f2'
test_encoding 'divl (%rdx)' 'f7 32'

# idivq
test_encoding 'idivq %rdx' '48 f7 fa'
test_encoding 'idivq (%rdx)' '48 f7 3a'

# idivl
test_encoding 'idivl %edx' 'f7 fa'
test_encoding 'idivl (%rdx)' 'f7 3a'

# cmpq
test_encoding 'cmpq $42, %rdx' '48 81 fa 2a 00 00 00'
test_encoding 'cmpq $42, (%rdx)' '48 81 3a 2a 00 00 00'
test_encoding 'cmpq %rcx, %rdx' '48 39 ca'
test_encoding 'cmpq %rcx, (%rdx)' '48 39 0a'
test_encoding 'cmpq (%rdx), %rcx' '48 3b 0a'

# cmpl
test_encoding 'cmpl $42, %edx' '81 fa 2a 00 00 00'
test_encoding 'cmpl $42, (%rdx)' '81 3a 2a 00 00 00'
test_encoding 'cmpl %ecx, %edx' '39 ca'
test_encoding 'cmpl %ecx, (%rdx)' '39 0a'
test_encoding 'cmpl (%rdx), %ecx' '3b 0a'

# cmpw
test_encoding 'cmpw $42, %dx' '66 81 fa 2a 00'
test_encoding 'cmpw $42, (%rdx)' '66 81 3a 2a 00'
test_encoding 'cmpw %cx, %dx' '66 39 ca'
test_encoding 'cmpw %cx, (%rdx)' '66 39 0a'
test_encoding 'cmpw (%rdx), %cx' '66 3b 0a'

# cmpb
test_encoding 'cmpb $42, %dl' '80 fa 2a'
test_encoding 'cmpb $42, (%rdx)' '80 3a 2a'
test_encoding 'cmpb %cl, %dl' '38 ca'
test_encoding 'cmpb %cl, (%rdx)' '38 0a'
test_encoding 'cmpb (%rdx), %cl' '3a 0a'

# sete
test_encoding 'sete %bl' '0f 94 c3'
test_encoding 'sete (%rbx)' '0f 94 03'

# setne
test_encoding 'setne %bl' '0f 95 c3'
test_encoding 'setne (%rbx)' '0f 95 03'

# setl
test_encoding 'setl %bl' '0f 9c c3'
test_encoding 'setl (%rbx)' '0f 9c 03'

# setg
test_encoding 'setg %bl' '0f 9f c3'
test_encoding 'setg (%rbx)' '0f 9f 03'

# setle
test_encoding 'setle %bl' '0f 9e c3'
test_encoding 'setle (%rbx)' '0f 9e 03'

# setge
test_encoding 'setge %bl' '0f 9d c3'
test_encoding 'setge (%rbx)' '0f 9d 03'

# negq
test_encoding 'negq %rsi' '48 f7 de'
test_encoding 'negq (%rsi)' '48 f7 1e'

# negl
test_encoding 'negl %esi' 'f7 de'
test_encoding 'negl (%rsi)' 'f7 1e'

# negq
test_encoding 'notq %rsi' '48 f7 d6'
test_encoding 'notq (%rsi)' '48 f7 16'

# negl
test_encoding 'notl %esi' 'f7 d6'
test_encoding 'notl (%rsi)' 'f7 16'

# andq
test_encoding 'andq $42, %rdx' '48 81 e2 2a 00 00 00'
test_encoding 'andq $42, (%rdx)' '48 81 22 2a 00 00 00'
test_encoding 'andq %rcx, %rdx' '48 21 ca'
test_encoding 'andq %rcx, (%rdx)' '48 21 0a'
test_encoding 'andq (%rdx), %rcx' '48 23 0a'

# andl
test_encoding 'andl $42, %edx' '81 e2 2a 00 00 00'
test_encoding 'andl $42, (%rdx)' '81 22 2a 00 00 00'
test_encoding 'andl %ecx, %edx' '21 ca'
test_encoding 'andl %ecx, (%rdx)' '21 0a'
test_encoding 'andl (%rdx), %ecx' '23 0a'

# xorq
test_encoding 'xorq $42, %rdx' '48 81 f2 2a 00 00 00'
test_encoding 'xorq $42, (%rdx)' '48 81 32 2a 00 00 00'
test_encoding 'xorq %rcx, %rdx' '48 31 ca'
test_encoding 'xorq %rcx, (%rdx)' '48 31 0a'
test_encoding 'xorq (%rdx), %rcx' '48 33 0a'

# xorl
test_encoding 'xorl $42, %edx' '81 f2 2a 00 00 00'
test_encoding 'xorl $42, (%rdx)' '81 32 2a 00 00 00'
test_encoding 'xorl %ecx, %edx' '31 ca'
test_encoding 'xorl %ecx, (%rdx)' '31 0a'
test_encoding 'xorl (%rdx), %ecx' '33 0a'

# orq
test_encoding 'orq $42, %rdx' '48 81 ca 2a 00 00 00'
test_encoding 'orq $42, (%rdx)' '48 81 0a 2a 00 00 00'
test_encoding 'orq %rcx, %rdx' '48 09 ca'
test_encoding 'orq %rcx, (%rdx)' '48 09 0a'
test_encoding 'orq (%rdx), %rcx' '48 0b 0a'

# orl
test_encoding 'orl $42, %edx' '81 ca 2a 00 00 00'
test_encoding 'orl $42, (%rdx)' '81 0a 2a 00 00 00'
test_encoding 'orl %ecx, %edx' '09 ca'
test_encoding 'orl %ecx, (%rdx)' '09 0a'
test_encoding 'orl (%rdx), %ecx' '0b 0a'

echo "[OK]"
exit 0
