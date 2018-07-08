cc: main.c
	gcc main.c -o cc

.PHONY: test
test: cc
	./test.sh

.PHONY: clean
clean:
	rm cc
