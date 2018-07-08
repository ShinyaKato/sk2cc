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


echo 11-7 | ./cc > test6.s
gcc test6.s -o test6
./test6
[ ! $? -eq 4 ] && echo test6 failed
rm test6.s test6

echo 123-20-3 | ./cc > test7.s
gcc test7.s -o test7
./test7
[ ! $? -eq 100 ] && echo test7 failed
rm test7.s test7

echo 123+20-3+50-81 | ./cc > test8.s
gcc test8.s -o test8
./test8
[ ! $? -eq 109 ] && echo test8 failed
rm test8.s test8


echo 3*5 | ./cc > test9.s
gcc test9.s -o test9
./test9
[ ! $? -eq 15 ] && echo test9 failed
rm test9.s test9

echo 3*5+7*8-3*4 | ./cc > test10.s
gcc test10.s -o test10
./test10
[ ! $? -eq 59 ] && echo test10 failed
rm test10.s test10
