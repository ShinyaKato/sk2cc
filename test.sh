#!/bin/bash

echo 2 | ./cc > test1.s
gcc test1.s -o test1
./test1
[ ! $? -eq 2 ] && echo test1 failed
rm test1.s test1

echo 71 | ./cc > test2.s
gcc test2.s -o test2
./test2
[ ! $? -eq 71 ] && echo test2 failed
rm test2.s test2

echo 234 | ./cc > test3.s
gcc test3.s -o test3
./test3
[ ! $? -eq 234 ] && echo test3 failed
rm test3.s test3


echo 2+3 | ./cc > test4.s
gcc test4.s -o test4
./test4
[ ! $? -eq 5 ] && echo test4 failed
rm test4.s test4

echo 123+43+1+21 | ./cc > test5.s
gcc test5.s -o test5
./test5
[ ! $? -eq 188 ] && echo test5 failed
rm test5.s test5
