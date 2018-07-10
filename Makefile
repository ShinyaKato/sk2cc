cc: main.c
	gcc -std=c11 -Wall main.c -o cc

.PHONY: test
test: cc
	./test.sh

.PHONY: clean
clean:
	rm cc
