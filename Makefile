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
	-Werror=format \
	-Werror=implicit-function-declaration \
	-Werror=incompatible-pointer-types \
	-Werror=vla
GNU_DBG = $(GNU_BASE) -O0 -g
GNU_REL = $(GNU_BASE) -Os -DNDEBUG
SANITIZE = -fsanitize=address -fsanitize=leak -fsanitize=undefined

CC = c99
CFLAGS = $(C99_REL)

all: build

build: bin/sunder-compile lib/sys/sys.sunder

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
	codegen.o \
	codegen-c.o \
	codegen-nasm.o
bin/sunder-compile: $(SUNDER_COMPILE_OBJS)
	$(CC) -o $@ $(CFLAGS) $(SUNDER_COMPILE_OBJS)

lib/sys/sys.sunder:
	@if [ ! -L lib/sys/sys.sunder -a $$(uname -m) = "x86_64"  ]; then (cd lib/sys && ln -s sys.sunder.amd64 sys.sunder); fi
	@if [ ! -L lib/sys/sys.sunder -a $$(uname -m) = "aarch64" ]; then (cd lib/sys && ln -s sys.sunder.arm64 sys.sunder); fi

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
	cp env "$(SUNDER_HOME)"

format:
	clang-format -i *.h *.c lib/sys/sys.h

clean:
	unlink lib/sys/sys.sunder 2>/dev/null || true
	rm -f bin/sunder-compile
	rm -f $$(find . -type f -name 'a.out*')
	rm -f $$(find . -type f -name '*.out')
	rm -f $$(find . -type f -name '*.o')
	rm -f $$(find . -type f -name '*.tmp')
	(cd examples/ && sh examples.clean.sh)

.SUFFIXES: .c .o
.c.o:
	$(CC) -o $@ -c $(CFLAGS) $<
