# Altair Compiler v1.8 - Makefile
CC      = gcc
CFLAGS  = -O3 -flto -fomit-frame-pointer -Wall -Wextra -std=c11 -Wno-unused-parameter -Wno-stringop-truncation -DENABLE_FNUMLIST -DENABLE_SB \
           -DALTAIR_RT_H="$(ALTAIR_RT_H)" \
           -DALTAIR_RT_C="$(ALTAIR_RT_C)"
ALTAIR_RT_H = $(abspath runtime/altair_rt.h)
ALTAIR_RT_C = $(abspath runtime/altair_rt.c)
SRCS = src/main.c src/lexer.c src/ast.c src/parser.c src/sema.c src/codegen.c
OBJS = $(SRCS:.c=.o)
.PHONY: all clean test install
all: altairc
altairc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm -flto
	@echo ""
	@echo "  altairc 1.8 built. Usage: ./altairc <program.at> -o <output>"
	@echo "  Guide: ./altairc guide"
	@echo ""
%.o: %.c
	$(CC) $(CFLAGS) -Isrc -c -o $@ $<
clean:
	rm -f $(OBJS) altairc
test: altairc
	./altairc examples/hello.at -o /tmp/altair_hello && /tmp/altair_hello
install: altairc
	@mkdir -p ~/.local/bin
	cp altairc ~/.local/bin/altairc
	@echo "Installed to ~/.local/bin/altairc"