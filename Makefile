cc: cc.h string.c map.c error.c sc.c lex.c parse.c gen.c main.c
	gcc -std=c11 -Wall string.c map.c error.c sc.c lex.c parse.c gen.c main.c -o cc

.PHONY: test
test: cc
	./test.sh

.PHONY: clean
clean:
	rm cc
