CC = gcc
CFLAGS = -std=c11 -Wall -Wno-builtin-declaration-mismatch -ggdb # TODO removing -Wno-builtin-declaration-mismatch

.PHONY: all
all:
	make sk2cc

sk2cc: sk2cc.h string.h string.c vector.h vector.c map.h map.c error.c token.c lex.c cpp.c type.c node.c parse.c gen.c main.c
	$(CC) $(CFLAGS) string.c vector.c map.c error.c token.c lex.c cpp.c type.c node.c parse.c gen.c main.c -o sk2cc

sk2cc_self: tmp sk2cc sk2cc.h string.h string.c vector.h vector.c map.h map.c error.c token.c lex.c cpp.c type.c node.c parse.c gen.c main.c
	./sk2cc string.c > tmp/string.s
	./sk2cc vector.c > tmp/vector.s
	./sk2cc map.c > tmp/map.s
	./sk2cc error.c > tmp/error.s
	./sk2cc token.c > tmp/token.s
	./sk2cc lex.c > tmp/lex.s
	./sk2cc cpp.c > tmp/cpp.s
	./sk2cc type.c > tmp/type.s
	./sk2cc node.c > tmp/node.s
	./sk2cc parse.c > tmp/parse.s
	./sk2cc gen.c > tmp/gen.s
	./sk2cc main.c > tmp/main.s
	gcc tmp/string.s tmp/vector.s tmp/map.s tmp/error.s tmp/token.s tmp/lex.s tmp/cpp.s tmp/type.s tmp/node.s tmp/parse.s tmp/gen.s tmp/main.s -o sk2cc_self

sk2cc_self2: tmp sk2cc_self sk2cc.h string.h string.c vector.h vector.c map.h map.c error.c token.c lex.c cpp.c type.c node.c parse.c gen.c main.c
	./sk2cc_self string.c > tmp/string2.s
	./sk2cc_self vector.c > tmp/vector2.s
	./sk2cc_self map.c > tmp/map2.s
	./sk2cc_self error.c > tmp/error2.s
	./sk2cc_self token.c > tmp/token2.s
	./sk2cc_self lex.c > tmp/lex2.s
	./sk2cc_self cpp.c > tmp/cpp2.s
	./sk2cc_self type.c > tmp/type2.s
	./sk2cc_self node.c > tmp/node2.s
	./sk2cc_self parse.c > tmp/parse2.s
	./sk2cc_self gen.c > tmp/gen2.s
	./sk2cc_self main.c > tmp/main2.s
	gcc tmp/string2.s tmp/vector2.s tmp/map2.s tmp/error2.s tmp/token2.s tmp/lex2.s tmp/cpp2.s tmp/type2.s tmp/node2.s tmp/parse2.s tmp/gen2.s tmp/main2.s -o sk2cc_self2

tmp:
	mkdir tmp

.PHONY: test
test: tmp sk2cc
	./tests/test.sh ./sk2cc

.PHONY: self_test
self_test: sk2cc_self
	./tests/test.sh ./sk2cc_self

.PHONY: self_self_test
self2_test: sk2cc_self2
	./tests/test.sh ./sk2cc_self2

.PHONY: full_test
full_test:
	make test
	make self_test
	make self2_test

.PHONY: clean
clean:
	rm -rf sk2cc sk2cc_self sk2cc_self2 tmp
