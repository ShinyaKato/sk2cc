cat << EOS | ./as /dev/stdin tmp/as_test.o
  movq \$42, %rax
  ret
EOS
gcc tmp/as_test.o -o tmp/as_test
./tmp/as_test
[ $? -ne 42 ] && exit 1

cat << EOS | ./as /dev/stdin tmp/as_test.o
  movq \$123, %rax
  ret
EOS
gcc tmp/as_test.o -o tmp/as_test
./tmp/as_test
[ $? -ne 123 ] && exit 1

cat << EOS | ./as /dev/stdin tmp/as_test.o
  movq \$42, %rax
  movq \$123, %rax
  movq \$34, %rax
  ret
EOS
gcc tmp/as_test.o -o tmp/as_test
./tmp/as_test
[ $? -ne 34 ] && exit 1

exit 0
