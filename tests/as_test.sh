cat << EOS | ./as /dev/stdin tmp/as_test.o
  movq \$42, %rax
  ret
EOS
gcc tmp/as_test.o -o tmp/as_test
./tmp/as_test
[ $? -ne 42 ] && exit 1

cat << EOS | ./as /dev/stdin tmp/as_test.o
  movq \$12, %rax
  movq \$23, %rcx
  movq \$34, %rdx
  movq \$45, %rbx
  movq \$56, %rsi
  movq \$67, %rdi
  ret
EOS
gcc tmp/as_test.o -o tmp/as_test
./tmp/as_test
[ $? -ne 12 ] && exit 1

exit 0
