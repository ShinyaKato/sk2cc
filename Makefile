cc: cc.h string.h string.c vector.h vector.c map.h map.c error.c scan.c lex.c cpp.c type.c symbol.c node.c parse.c gen.c main.c
	gcc -std=c11 -Wall -ggdb string.c vector.c map.c error.c scan.c lex.c cpp.c type.c symbol.c node.c parse.c gen.c main.c -o cc

self: tmp cc cc.h string.h string.c vector.h vector.c map.h map.c error.c scan.c lex.c cpp.c type.c symbol.c node.c parse.c gen.c main.c
	./cc string.c > tmp/string.s
	./cc vector.c > tmp/vector.s
	./cc map.c > tmp/map.s
	./cc error.c > tmp/error.s
	./cc scan.c > tmp/scan.s
	./cc lex.c > tmp/lex.s
	./cc cpp.c > tmp/cpp.s
	./cc type.c > tmp/type.s
	./cc symbol.c > tmp/symbol.s
	./cc node.c > tmp/node.s
	./cc parse.c > tmp/parse.s
	./cc gen.c > tmp/gen.s
	./cc main.c > tmp/main.s
	gcc -no-pie tmp/string.s tmp/vector.s tmp/map.s tmp/error.s tmp/scan.s tmp/lex.s tmp/cpp.s tmp/type.s tmp/symbol.s tmp/node.s tmp/parse.s tmp/gen.s tmp/main.s -o self

tmp:
	mkdir tmp

.PHONY: test
test: tmp cc
	./tests/test.sh ./cc

.PHONY: self_test
self_test: self
	./tests/test.sh ./self

.PHONY: clean
clean:
	rm -rf cc self tmp
