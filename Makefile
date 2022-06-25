.POSIX:
.SUFFIXES:
.PHONY: \
	all \
	build \
	check \
	examples \
	install \
	format \
	clean
.SILENT: clean

SUNDER_HOME = $$HOME/.sunder
SUNDER_DEFAULT_BACKEND = yasm

C99_BASE = -DSUNDER_DEFAULT_BACKEND=$(SUNDER_DEFAULT_BACKEND)
C99_DBG = $(C99_BASE) -O0 -g
C99_REL = $(C99_BASE) -DNDEBUG

GNU_BASE = \
	$(C99_BASE) \
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

all: build

SUNDER_COMPILE_OBJS = \
	sunder-compile.o \
	util.o \
	sunder.o \
	lex.o \
	cst.o \
	parse.o \
	order.o \
	ast.o \
	resolve.o \
	eval.o \
	codegen.o
bin/sunder-compile: $(SUNDER_COMPILE_OBJS)
	$(CC) -o $@ $(CFLAGS) $(SUNDER_COMPILE_OBJS)

build: bin/sunder-compile

check: build
	SUNDER_HOME="$(realpath .)" \
	SUNDER_IMPORT_PATH="$(realpath .)/lib" \
	sh bin/sunder-test

examples: build
	(cd examples/ && sh examples.build.sh)

install: build
	mkdir -p "$(SUNDER_HOME)"
	cp -r bin/ "$(SUNDER_HOME)"
	cp -r lib/ "$(SUNDER_HOME)"

format:
	clang-format -i *.h *.c

clean:
	rm -f bin/sunder-compile
	rm -f $$(find . -type f -name '*.out')
	rm -f $$(find . -type f -name '*.o')
	(cd examples/ && sh examples.clean.sh)

.SUFFIXES: .c .o
.c.o:
	$(CC) -o $@ -c $(CFLAGS) $<
