cc: cc.h string.c main.c
	gcc -std=c11 -Wall string.c main.c -o cc

string_test: cc.h string.c tests/string_driver.c
	gcc -std=c11 -Wall string.c tests/string_driver.c -o string_test

.PHONY: test
test: cc string_test
	./test.sh

.PHONY: clean
clean:
	rm cc string_test
