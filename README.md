# The Sunder Programming Language
Sunder is a work-in-progress C-like systems programming language and compiler
for x86-64 Linux.

## Dependencies
+ POSIX-compatible `make`
+ Supported toolchain containing:
  + C99 compiler (POSIX `c99`, `clang`, `gcc`, etc.)
  + `ld`
+ [`nasm`](https://www.nasm.us/)
+ `clang-format` (development only)

Dependencies can be installed on Debian-based distros (amd64) with:

```sh
$ apt-get install build-essential clang clang-format nasm
```

## Building
### Quick Version
Run `make all` to build the compiler and run the test suite.
To rebuild from scratch run `make clean all`.

### Long Version
The top-level `Makefile` contains the following important targets:

+ `build` => Build the compiler.
+ `test` => Run the test suite for the language and standard library.
+ `examples` => Build the example programs under the `examples` directory.
+ `format` => Run `clang-format` over the compiler sources.
+ `clean` => Remove build artifacts.

Targets will execute with `CC=c99` and `CFLAGS='-O0 -g'` by default. Additional
compiler/compiler-flag combinations are listed below, but the default `CC` and
`CFLAGS` selection should be fine for most cases.

Specific compiler/compiler-flag combinations include:

```sh
$ make <targets> CFLAGS='$(C99_DBG)'  # POSIX c99 (debug)
$ make <targets> CFLAGS='$(C99_REL)'  # POSIX c99 (release)

$ # Use CC=clang for Clang or CC=gcc for GCC
$ make <targets> CC=clang CFLAGS='$(GNU_DBG)'              # clang/gcc (debug)
$ make <targets> CC=clang CFLAGS='$(GNU_DBG) $(SANITIZE)'  # clang/gcc (debug with Address Sanitizer)
$ make <targets> CC=clang CFLAGS='$(GNU_REL)'              # clang/gcc (release)
```

The script `build.sh` will perform the full build/test cycle with stricter
checks and with address santizer enabled. Compiling the test suite using a
compiler built with address sanitizer enabled is *much slower* than the default
`make test`, but will catch a lot of memory safety and undefined behavior bugs.
When working on the compiler it is generally most efficient to use `make all`
while iterating and `sh build.sh` just before committing a changeset.

```sh
$ sh build.sh
```

By default `build.sh` will run the `clean` and `all` make targets, but this can
be overridden. See `sh build.sh --help` for details.

## Installing
The `install` target will install the Sunder toolchain into the directory
specified by `$SUNDER_HOME` (default `$HOME/.sunder`). Run `make install` with
`$SUNDER_HOME` specified as the directory of your choice, add `$SUNDER_HOME` to
your `.profile` (or equivalent), and then finally add `$SUNDER_HOME/bin` to your
`$PATH`.

```sh
$ make install
```

```sh
# Add this to your .profile
if [ -d "${HOME}/.sunder" ]; then
    export SUNDER_HOME="${HOME}/.sunder"
    export SUNDER_IMPORT_PATH="${SUNDER_HOME}/lib"
    PATH="${SUNDER_HOME}/bin:$PATH"
fi
```

Verify that the compiler has been successfully installed by running
`sunder-compile --help`. You may have to source your `.profile` in new shells
until the start of your next login session.

## Using the Sunder Compiler
Compiling the source file `hello.sunder` into the executable `hello`.

```sunder
import "std";

func main() void {
    std::print_line(std::out(), "Hello, world!");
}
```

```sh
$ sunder-compile -o hello hello.sunder
$ ./hello
Hello, world!
```

The `-o OUT` option determines the name of the output executable. If this
option is not provided then the output executable will default to the name
`a.out`.

The intermediate files `OUT.asm` and `OUT.o` (output executable name plus
`.asm` and `.o` extensions) are generated during compilation and then
subsequently removed after the output executable has been created. The `-k`
flag will instruct the compiler *not* to remove these files (useful for
debugging).

```sh
$ sunder-compile -k -o hello hello.sunder
$ ls hello*
hello  hello.asm  hello.o  hello.sunder

```

## License
Sunder is distributed under the terms of the Apache License (Version 2.0).

See LICENSE for more information.
