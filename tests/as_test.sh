./as 42 tmp/as_test.o
gcc tmp/as_test.o -o tmp/as_test
./tmp/as_test
[ $? -ne 42 ] && exit 1

./as 123 tmp/as_test.o
gcc tmp/as_test.o -o tmp/as_test
./tmp/as_test
[ $? -ne 123 ] && exit 1

exit 0
