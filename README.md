# The Sunder Programming Language
Sunder is a C-like systems programming language and compiler for x86-64 Linux,
ARM64 Linux, and WebAssembly.

## Dependencies
Sunder tooling should build and run on any x86-64 or ARM64 Linux machine
satisfying the following dependencies:

+ POSIX-compatible `make`
+ Supported toolchain containing:
  + C99 compiler (POSIX `c99`, `clang`, `gcc`, etc.)
  + `ld`
+ [`clang`](https://clang.llvm.org/) or [`gcc`](https://gcc.gnu.org/) (C backend only)
+ [`emcc`](https://emscripten.org/) (C→WebAssembly backend only)
+ [`nasm`](https://www.nasm.us/) or [`yasm`](https://yasm.tortall.net/) (x86-64 NASM backend only)
+ `clang-format` (development only)

The top-level Dockerfile defines a Debian image with all development
dependencies pre-installed. The Docker image can be built and run with:

```sh
$ docker buildx build --platform=linux/amd64 --tag sunder .             # Build the image (do this once)
$ docker run --rm --interactive --tty --volume "$(pwd)":/sunder sunder  # Run the image (do this every time)
```

## Building
### Quick Version
Run `make` to build the compiler.

Run `make check examples` to run the test suite and compile the example
programs under the `examples` directory.

### Long Version
The top-level `Makefile` contains the following important targets:

+ `build` => Build the compiler (default `make` target).
+ `check` => Run the test suite for the language and standard library.
+ `examples` => Compile the example programs under the `examples` directory.
+ `format` => Run `clang-format` over the compiler sources.
+ `clean` => Remove build artifacts.

Targets will execute with `CC=c99` using release mode `CFLAGS` by default.

Specific `CC`/`CFLAGS` combinations include:

```sh
$ make <targets> CFLAGS='$(C99_DBG)'  # POSIX c99 (debug)
$ make <targets> CFLAGS='$(C99_REL)'  # POSIX c99 (release)

$ # Use CC=clang for Clang or CC=gcc for GCC
$ make <targets> CC=clang CFLAGS='$(GNU_DBG)'              # clang/gcc (debug)
$ make <targets> CC=clang CFLAGS='$(GNU_DBG) $(SANITIZE)'  # clang/gcc (debug with Address Sanitizer)
$ make <targets> CC=clang CFLAGS='$(GNU_REL)'              # clang/gcc (release)
```

The compiler is built with `SUNDER_DEFAULT_BACKEND=C` and
`SUNDER_DEFAULT_CC=cc` by default, indicating that the C backend should be used
with `cc` if `SUNDER_BACKEND` and/or `SUNDER_CC` (explained below) are not set
when the compiler is invoked.

To use `nasm` as the default compiler backend, override
`SUNDER_DEFAULT_BACKEND` with `nasm` when executing targets:

```sh
$ make <targets> SUNDER_DEFAULT_BACKEND=nasm
```

To use `yasm` as the default compiler backend, override
`SUNDER_DEFAULT_BACKEND` with `yasm` when executing targets:

```sh
$ make <targets> SUNDER_DEFAULT_BACKEND=yasm
```

## Installing
The `install` target will install the Sunder toolchain into the directory
specified by `SUNDER_HOME` (default `$HOME/.sunder`). Run `make install` with
`SUNDER_HOME` specified as the directory of your choice:

```sh
$ make install                          # Install to the default $HOME/.sunder
$ make install SUNDER_HOME=/opt/sunder  # Install to /opt/sunder
```

Then, add the following snippet to your `.profile`, replacing `$HOME/.sunder`
with your chosen `SUNDER_HOME` directory if installing to a non-default
`SUNDER_HOME` location:

```sh
export SUNDER_HOME="$HOME/.sunder"
if [ -e "$SUNDER_HOME/env" ]; then
    . "$SUNDER_HOME/env"
fi
```

Verify that the compiler has been successfully installed by running
`sunder-compile -h`. You may have to source your `.profile` in new shells until
the start of your next login session.

## Using the Sunder Compiler
Sunder programs are compiled with `sunder-compile`.

```sunder
import "std";

func main() void {
    std::print_line(std::out(), "Hello, world!");
}
```

```sh
$ sunder-compile -o hello examples/hello.sunder
$ ./hello
Hello, world!
```

The `-o OUT` option may be used to specify the name of the output file. If this
option is not provided then the output file will default to the name `a.out`.

Intermediate files of the form `OUT.*`, for output program `OUT` are generated
during compilation and subsequently removed after the output file has been
created. The `-k` flag will instruct the compiler *not* to remove these files.

```sh
$ SUNDER_BACKEND=C sunder-compile -k -o hello examples/hello.sunder
$ ls hello*
hello  hello.tmp.c

$ SUNDER_BACKEND=nasm sunder-compile -k -o hello examples/hello.sunder
$ ls hello*
hello  hello.tmp.asm  hello.tmp.o

$ SUNDER_BACKEND=C SUNDER_CC=emcc SUNDER_ARCH=wasm32 SUNDER_HOST=emscripten sunder-compile -k -o hello.html examples/hello.sunder
$ ls hello*
hello.html  hello.html.tmp.c  hello.js  hello.wasm
```

The `-g` flag will instruct the compiler to generate debug information. Use of
`-g` in combination with `-k` facilitates debugging with GDB.

```sh
$ SUNDER_BACKEND=nasm sunder-compile -g -k -o hello examples/hello.sunder
$ gdb -q hello
libthread-db debugging is 0.
Reading symbols from hello...
(gdb) break std.print_line
Breakpoint 1 at 0x4105c3
(gdb) run
Starting program: /home/ashn/sources/sunder/hello

Breakpoint 1, 0x00000000004105c3 in std.print_line ()
(gdb) where
#0  0x00000000004105c3 in std.print_line ()
#1  0x000000000045a548 in main ()
```

The following environment variables affect compiler behavior:

**`SUNDER_BACKEND`** selects the backend to be used for compilation. Currently,
`SUNDER_BACKEND=C`, `SUNDER_BACKEND=nasm`, and `SUNDER_BACKEND=yasm`, are
supported. If `SUNDER_BACKEND` is not set, then the default backend is used.

**`SUNDER_CC`** selects the C compiler to be used when compiling with the C
backend. Currently, `SUNDER_CC=clang`, `SUNDER_CC=gcc`, and `SUNDER_CC=emcc`
are supported. If `SUNDER_CC` is not set, then the default C compiler is used.

**`SUNDER_CFLAGS`** is a space-separated list of additional flags passed to the
C compiler when compiling with the C backend.

**`SUNDER_SEARCH_PATH`** is a colon-separated list of directories specifying
the module search path for `import` and `embed` statements.

**`SUNDER_SYSASM_PATH`** specifies the location of the platform specific
`sys.asm` file that defines the program entry point as well as low-level
operating system and hardware abstractions when using the NASM backend. If
`SUNDER_SYSASM_PATH` is not set, then the default path is used.

**`SUNDER_ARCH`** specifies the target architecture to build for. Currently,
`SUNDER_ARCH=amd64`, `SUNDER_ARCH=arm64`, and `SUNDER_ARCH=wasm32` are
supported. If `SUNDER_ARCH` is not set, then the default architecture specified
by `sunder-platform arch` is used.

**`SUNDER_HOST`** specifies the target operating system to build for.
Currently, `SUNDER_HOST=freestanding`, `SUNDER_HOST=emscripten`, and
`SUNDER_HOST=linux` are supported. If `SUNDER_HOST` is not set, then the
default host specified by `sunder-platform host` is used.

## Compiling to WebAssembly
Sunder supports compiling to WebAssembly via the C backend using
[Emscripten](https://emscripten.org/). When compiling to WebAssembly, specify
`emcc`, `wasm32`, and `emscripten` as the C compiler, target architecture, and
target host, respectively.

```sh
$ SUNDER_BACKEND=C \
    SUNDER_CC=emcc \
    SUNDER_CFLAGS="-sSINGLE_FILE --shell-file ${SUNDER_HOME}/lib/sys/sys.wasm32-emscripten.html" \
    SUNDER_ARCH=wasm32 \
    SUNDER_HOST=emscripten \
    sunder-compile -o hello.html examples/hello.sunder
$ firefox hello.html
```

## Using Sunder as a Scripting Language
Sunder can be used for scripting by adding `#!/usr/bin/env sunder-run` as the
first line of a Sunder source file.

```sunder
#!/usr/bin/env sunder-run
import "std";

func main() void {
    std::print(std::out(), "What is your name?\n> ");

    var result = std::read_line(std::input());
    if result.is_error() {
        std::print_line(std::err(), result.error().*.data);
        std::exit(std::EXIT_FAILURE);
    }

    var optional = result.value();
    if optional.is_empty() or countof(optional.value()) == 0 {
        std::print_line(std::err(), "unexpected empty input");
        std::exit(std::EXIT_FAILURE);
    }

    var line = optional.value();
    defer std::slice[[byte]]::delete(line);

    var name = std::ascii::view_trimmed(optional.value());
    std::print_format_line(std::out(), "Nice to meet you {}!", (:[]std::formatter)[std::formatter::init[[typeof(name)]](&name)]);
}
```

```sh
$ ./examples/greet.sunder
What is your name?
> Alice
Nice to meet you Alice!
```

## Using Sunder Without Installing
Executing the following commands will create an environment sufficient for
Sunder development and experimentation without requiring the Sunder toolchain
to be installed.

```sh
$ cd /your/path/to/sunder
$ make
$ SUNDER_HOME="$(pwd)"
$ . ./env
$ sunder-run examples/hello.sunder
Hello, world!
```

## License
Sunder is distributed under the terms of the Apache License (Version 2.0).

See LICENSE for more information.
