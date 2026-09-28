CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -O2
LDLIBS := -lm

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
BIN := guanaco

.PHONY: all test clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Smoke test: no input file runs the two built-in demos through the
# full tokenize/parse/eval/G-code pipeline (see main.c) and exits 0
# only if neither crashes or hits a parse/runtime error.
test: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) $(OBJS)
