CC = gcc
CFLAGS = -std=c11 --pedantic-errors -Wall -g

HEADERS = sk2cc.h
SRCS = vector.c string.c map.c error.c token.c lex.c scan.c cpp.c node.c symbol.c type.c parse.c sema.c gen.c main.c

DIR = tmp

SK2CC = ./sk2cc
SELF = ./self
SELF2 = ./self2

.PHONY: all
all:
	make sk2cc

# working directory
$(DIR):
	mkdir $@

# compile with gcc
$(SK2CC): $(HEADERS) $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

# self host
ASMS_SELF = $(patsubst %.c,$(DIR)/%.s,$(SRCS))

$(ASMS_SELF): $(DIR)/%.s:%.c $(HEADERS) $(DIR) $(SK2CC)
	$(SK2CC) $< > $@

$(SELF): $(ASMS_SELF)
	$(CC) $(CFLAGS) -o $@ $^

# self host by self-hosted executable
ASMS_SELF2 = $(patsubst %.c,$(DIR)/%2.s,$(SRCS))

$(ASMS_SELF2): $(DIR)/%2.s:%.c $(HEADERS) $(DIR) $(SELF)
	$(SELF) $< > $@

$(SELF2): $(ASMS_SELF2)
	$(CC) $(CFLAGS) -o $@ $^

# test commands
.PHONY: test_unit
test_unit: $(DIR)
	# string
	gcc -std=c11 -Wall string.c tests/string_driver.c -o tmp/string_test
	./tmp/string_test
	# vector
	gcc -std=c11 -Wall vector.c tests/vector_driver.c -o tmp/vector_test
	./tmp/vector_test
	# map
	gcc -std=c11 -Wall map.c tests/map_driver.c -o tmp/map_test
	./tmp/map_test
	# token
	gcc -std=c11 -Wall error.c token.c tests/token_driver.c -o tmp/token_test
	./tmp/token_test

TEST_SCRIPT = ./tests/test.sh

.PHONY: test_check
test_check: $(DIR)
	$(TEST_SCRIPT) "gcc -std=c11 --pedantic-errors -S -o -"

.PHONY: test_sk2cc
test_sk2cc: $(DIR) $(SK2CC)
	$(TEST_SCRIPT) $(SK2CC)

.PHONY: test_self
test_self: $(DIR) $(SELF)
	$(TEST_SCRIPT) $(SELF)

.PHONY: test_self2
test_self2: $(DIR) $(SELF2)
	$(TEST_SCRIPT) $(SELF2)

.PHONY: test_diff
test_diff: $(DIR) $(ASMS_SELF) $(ASMS_SELF2)
	for src in $(SRCS); do \
		diff tmp/`echo $$src | sed -e "s/\.c$$/.s/g"` tmp/`echo $$src | sed -e "s/\.c$$/2.s/g"`; \
	done

.PHONY: test
test:
	make test_unit
	make test_check
	make test_sk2cc
	make test_self
	make test_self2
	make test_diff

# assembler
as: as.h as_string.c as_vector.c as_map.c as_binary.c as_error.c as_scan.c as_lex.c as_parse.c as_encode.c as_gen.c as.c
	$(CC) $(CFLAGS) as_string.c as_vector.c as_map.c as_binary.c as_error.c as_scan.c as_lex.c as_parse.c as_encode.c as_gen.c as.c -o as

.PHONY: test_as
test_as: tmp as
	./tests/as_test.sh

# clean
.PHONY: clean
clean:
	rm -rf $(DIR) $(SK2CC) $(SELF) $(SELF2) as
