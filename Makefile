cc: cc.h string.h string.c vector.h vector.c map.h map.c error.c scan.c lex.c cpp.c type.c parse.c analyze.c gen.c main.c
	gcc -std=c11 -Wall -ggdb string.c vector.c map.c error.c scan.c lex.c cpp.c type.c parse.c analyze.c gen.c main.c -o cc

self: tmp cc cc.h string.c vector.c map.c error.c scan.c lex.c cpp.c type.c parse.c analyze.c gen.c main.c
	./cc string.c > tmp/string.s
	./cc vector.c > tmp/vector.s
	./cc map.c > tmp/map.s
	./cc error.c > tmp/error.s
	./cc scan.c > tmp/scan.s
	./cc lex.c > tmp/lex.s
	./cc cpp.c > tmp/cpp.s
	./cc type.c > tmp/type.s
	./cc parse.c > tmp/parse.s
	./cc analyze.c > tmp/analyze.s
	./cc gen.c > tmp/gen.s
	./cc main.c > tmp/main.s
	gcc -no-pie tmp/string.s tmp/vector.s tmp/map.s tmp/error.s tmp/scan.s tmp/lex.s tmp/cpp.s tmp/type.s tmp/parse.s tmp/analyze.s tmp/gen.s tmp/main.s -o self

self_test: self
	./tests/test.sh ./self && echo "self compiled successfully."

tmp:
	mkdir tmp

.PHONY: test
test: tmp cc
	./tests/test.sh ./cc

.PHONY: clean
clean:
	rm -rf cc self tmp
