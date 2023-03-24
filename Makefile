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
SUNDER_DEFAULT_ARCH = $$(sh bin/sunder-platform arch)
SUNDER_DEFAULT_HOST = $$(sh bin/sunder-platform host)
SUNDER_DEFAULT_BACKEND = C
SUNDER_DEFAULT_CC = clang

C99_BASE = \
	-DSUNDER_DEFAULT_ARCH=$(SUNDER_DEFAULT_ARCH) \
	-DSUNDER_DEFAULT_HOST=$(SUNDER_DEFAULT_HOST) \
	-DSUNDER_DEFAULT_BACKEND=$(SUNDER_DEFAULT_BACKEND) \
	-DSUNDER_DEFAULT_CC=$(SUNDER_DEFAULT_CC)
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

build: bin/sunder-compile

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
	rm -f bin/sunder-compile
	rm -f $$(find . -type f -name 'a.out*')
	rm -f $$(find . -type f -name '*.out')
	rm -f $$(find . -type f -name '*.o')
	rm -f $$(find . -type f -name '*.tmp')
	(cd examples/ && sh examples.clean.sh)

.SUFFIXES: .c .o
.c.o:
	$(CC) -o $@ -c $(CFLAGS) $<
