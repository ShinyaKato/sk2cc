cc: cc.h string.c map.c error.c sc.c lex.c parse.c gen.c main.c
	gcc -std=c11 -Wall string.c map.c error.c sc.c lex.c parse.c gen.c main.c -o cc

string_test: cc.h string.c tests/string_driver.c
	gcc -std=c11 -Wall string.c tests/string_driver.c -o string_test

map_test: cc.h map.c tests/map_driver.c
	gcc -std=c11 -Wall map.c tests/map_driver.c -o map_test

.PHONY: test
test: cc string_test map_test
	./test.sh

.PHONY: clean
clean:
	rm cc string_test
