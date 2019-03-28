CC = gcc
CFLAGS = -std=c11 --pedantic-errors -Wall -g

HEADERS = \
	string.h vector.h map.h binary.h \
	sk2cc.h as.h

SRCS = \
	vector.c string.c map.c binary.c \
	error.c lex.c cpp.c node.c symbol.c type.c parse.c sema.c gen.c cc.c \
	as_error.c as_scan.c as_lex.c as_parse.c as_encode.c as_gen.c as.c \
	main.c

DIR = tmp

SK2CC = ./sk2cc
SELF = ./self
SELF2 = ./self2

.PHONY: all
all:
	make sk2cc

# compile with gcc
$(SK2CC): $(HEADERS) $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

# self host
ASMS_SELF = $(patsubst %.c,$(DIR)/%.s,$(SRCS))

$(ASMS_SELF): $(DIR)/%.s:%.c $(HEADERS) $(SK2CC)
	@mkdir -p $(DIR)
	$(SK2CC) $< > $@

$(SELF): $(ASMS_SELF)
	$(CC) $(CFLAGS) -o $@ $^

# self host by self-hosted executable
ASMS_SELF2 = $(patsubst %.c,$(DIR)/%2.s,$(SRCS))

$(ASMS_SELF2): $(DIR)/%2.s:%.c $(HEADERS) $(SELF)
	@mkdir -p $(DIR)
	$(SELF) $< > $@

$(SELF2): $(ASMS_SELF2)
	$(CC) $(CFLAGS) -o $@ $^

# test commands
.PHONY: test_unit
test_unit:
	@mkdir -p $(DIR)
	gcc -std=c11 -Wall string.c tests/string_driver.c -o tmp/string_test && ./tmp/string_test
	gcc -std=c11 -Wall vector.c tests/vector_driver.c -o tmp/vector_test && ./tmp/vector_test
	gcc -std=c11 -Wall map.c tests/map_driver.c -o tmp/map_test && ./tmp/map_test

.PHONY: test_check
test_check:
	./tests/test.sh 'gcc -std=c11 --pedantic-errors -S -o -'

.PHONY: test_sk2cc
test_sk2cc: $(SK2CC)
	./tests/test.sh '$(SK2CC)'
	./tests/as_test.sh '$(SK2CC) --as'

.PHONY: test_self
test_self: $(SELF)
	./tests/test.sh '$(SELF)'
	./tests/as_test.sh '$(SELF) --as'

.PHONY: test_self2
test_self2: $(SELF2)
	./tests/test.sh '$(SELF2)'
	./tests/as_test.sh '$(SELF2) --as'

.PHONY: test_diff
test_diff: $(ASMS_SELF) $(ASMS_SELF2)
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

# clean
.PHONY: clean
clean:
	rm -rf $(DIR) $(SK2CC) $(SELF) $(SELF2)
