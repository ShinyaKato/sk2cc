CC = gcc
CFLAGS = -std=c11 --pedantic-errors -Wall -g

HEADERS = sk2cc.h
SRCS = $(wildcard *.c)

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
TEST_SCRIPT = ./tests/test.sh

.PHONY: test_check
test_check: $(DIR)
	$(TEST_SCRIPT) "gcc -std=c11 --pedantic-errors -S -o -"

.PHONY: test
test: $(DIR) $(SK2CC)
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

.PHONY: test_full
test_full:
	make test_check
	make test
	make test_self
	make test_self2
	make test_diff

# clean
.PHONY: clean
clean:
	rm -rf $(DIR) $(SK2CC) $(SELF) $(SELF2)
