#!/bin/bash

test_expression() {
  exp=$1
  val=$2

  echo $exp | ./cc > out.s
  if [ $? -ne 0 ]; then
    echo failed to generate assembly from $exp
    rm out.s
    exit 1
  fi

  gcc out.s -o out
  if [ $? -ne 0 ]; then
    echo failed to compile $exp
    cat out.s
    rm out.s
    exit 1
  fi

  ./out
  ret=$?

  if [ $ret -ne $val ]; then
    echo $exp should be $val, but got $ret.
    cat out.s
    rm out.s out
    exit 1
  fi

  rm out.s out
}

test_error() {
  exp=$1
  msg=$2

  echo $exp | ./cc > /dev/null 2> stderr.txt
  if [ $? -ne 1 ]; then
    echo $exp should cause exit status 1, but got 0.
    rm stderr.txt
    exit 1
  fi

  echo $msg | diff - stderr.txt > /dev/null
  ret=$?

  if [ $ret -ne 0 ]; then
    echo $exp should raise \"$msg\", but got \"$(cat stderr.txt)\".
    rm stderr.txt
    exit 1
  fi

  rm stderr.txt
}

test_expression "2" 2
test_expression "71" 71
test_expression "234" 234

test_expression "2+3" 5
test_expression "123+43+1+21" 188

test_expression "11-7" 4
test_expression "123-20-3" 100
test_expression "123+20-3+50-81" 109

test_expression "3*5" 15
test_expression "3*5+7*8-3*4" 59

test_expression "(3-5)*3+7" 1
test_expression "(((123)))" 123

test_expression "6/2" 3
test_expression "6/(2-3)+7" 1
test_expression "123%31" 30
test_expression "32/4+5*(8-5)+5%2" 24

test_expression "3*5==7+8" 1
test_expression "3*5==123" 0
test_expression "3*5!=7+8" 0
test_expression "3*5!=123" 1

test_error "abc" "error: unexpected character."
test_error "2*(3+4" "error: tRPAREN is expected."
test_error "5+*" "error: unexpected primary expression."
test_error "5(4)" "error: invalid expression."
