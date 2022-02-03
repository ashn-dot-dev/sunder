.POSIX:
.SUFFIXES:
.PHONY: \
	all \
	build \
	test \
	examples \
	install \
	format \
	clean
.SILENT: clean

C99_DBG = -O0 -g
C99_REL = -DNDEBUG

GNU_BASE = \
	-std=c99 -pedantic-errors \
	-Wall -Wextra \
	-Werror=conversion \
	-Werror=double-promotion \
	-Werror=float-equal \
	-Werror=implicit-function-declaration \
	-Werror=incompatible-pointer-types \
	-Werror=vla
GNU_DBG = $(GNU_BASE) -O0 -g
GNU_REL = $(GNU_BASE) -Os -DNDEBUG
SANITIZE = -fsanitize=address -fsanitize=leak -fsanitize=undefined

CC = c99
CFLAGS = $(C99_DBG)
INCS = -Ideps

SUNDER_HOME="$$HOME/.sunder"

all: build test examples

SUNDER_COMPILE_OBJS = \
	sunder-compile.o \
	sunder.o \
	lex.o \
	cst.o \
	parse.o \
	order.o \
	tir.o \
	resolve.o \
	eval.o \
	codegen.o
bin/sunder-compile: $(SUNDER_COMPILE_OBJS)
	$(CC) -o $@ $(INCS) $(CFLAGS) $(SUNDER_COMPILE_OBJS)

build: bin/sunder-compile

test: build
	(cd tests/ && sh test-all.sh)

examples: build
	(cd examples/ && sh examples.build.sh)

install: build
	mkdir -p $(SUNDER_HOME)
	cp -r bin/ $(SUNDER_HOME)
	cp -r lib/ $(SUNDER_HOME)

format:
	clang-format -i *.h *.c

clean:
	rm -f bin/sunder-compile
	rm -f $$(find . -type f -name '*.out')
	rm -f $$(find . -type f -name '*.o')
	(cd examples/ && sh examples.clean.sh)

.SUFFIXES: .c .o
.c.o:
	$(CC) -o $@ -c $(INCS) $(CFLAGS) $<
