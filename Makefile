cc: cc.h string.c vector.c map.c error.c scan.c lex.c type.c parse.c analyze.c gen.c main.c
	gcc -std=c11 -Wall string.c vector.c map.c error.c scan.c lex.c type.c parse.c analyze.c gen.c main.c -o cc

.PHONY: test
test: cc
	./test.sh

.PHONY: clean
clean:
	rm cc
