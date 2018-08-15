cc: cc.h string.c vector.c map.c error.c scan.c lex.c type.c parse.c analyze.c gen.c main.c
	gcc -std=c11 -Wall -ggdb string.c vector.c map.c error.c scan.c lex.c type.c parse.c analyze.c gen.c main.c -o cc

tmp:
	mkdir tmp

.PHONY: test
test: tmp cc
	./tests/test.sh ./cc

.PHONY: clean
clean:
	rm -rf cc tmp
