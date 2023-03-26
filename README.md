# The Sunder Programming Language
Sunder is a C-like systems programming language and compiler for x86-64 Linux.

## Dependencies
Sunder tooling should build and run on any x86-64 Linux machine satisfying the
following dependencies:

+ POSIX-compatible `make`
+ Supported toolchain containing:
  + C99 compiler (POSIX `c99`, `clang`, `gcc`, etc.)
  + `ld`
+ [`clang`](https://clang.llvm.org/) or [`gcc`](https://gcc.gnu.org/) (C backend only)
+ [`nasm`](https://www.nasm.us/) or [`yasm`](https://yasm.tortall.net/) (NASM backend only)
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

Specific compiler/compiler-flag combinations include:

```sh
$ make <targets> CFLAGS='$(C99_DBG)'  # POSIX c99 (debug)
$ make <targets> CFLAGS='$(C99_REL)'  # POSIX c99 (release)

$ # Use CC=clang for Clang or CC=gcc for GCC
$ make <targets> CC=clang CFLAGS='$(GNU_DBG)'              # clang/gcc (debug)
$ make <targets> CC=clang CFLAGS='$(GNU_DBG) $(SANITIZE)'  # clang/gcc (debug with Address Sanitizer)
$ make <targets> CC=clang CFLAGS='$(GNU_REL)'              # clang/gcc (release)
```

The compiler is built with `SUNDER_DEFAULT_BACKEND=C` and
`SUNDER_DEFAULT_CC=clang` by default, indicating that the C backend should be
used with `clang` if `SUNDER_BACKEND` and/or `SUNDER_CC` (explained below) are
not set when the compiler is invoked.

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
Sunder programs are compiled into executables with `sunder-compile`.

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

The `-o OUT` option may be used to specify the name of the output executable.
If this option is not provided then the output executable will default to the
name `a.out`.

The intermediate files `OUT.tmp.asm`, `OUT.tmp.c`, and `OUT.tmp.o` (output
executable name plus `tmp.asm`, `tmp.c`, and `tmp.o` extensions) are generated
during compilation (depending on the chosen `SUNDER_BACKEND`) and subsequently
removed after the output executable has been created. The `-k` flag will
instruct the compiler *not* to remove these files (useful for debugging).

```sh
$ SUNDER_BACKEND=C sunder-compile -k -o hello examples/hello.sunder
$ ls hello*
hello  hello.tmp.c

$ SUNDER_BACKEND=nasm sunder-compile -k -o hello examples/hello.sunder
$ ls hello*
hello  hello.tmp.asm  hello.tmp.o
```

The following environment variables affect compiler behavior:

+ `SUNDER_BACKEND` => Selects the backend to be used for object file
  generation. Currently, `SUNDER_BACKEND=C`, `SUNDER_BACKEND=nasm`, and
  `SUNDER_BACKEND=yasm`, are supported. If this environment variable is not
  set, then the default backend is used.
+ `SUNDER_IMPORT_PATH` => Colon-separated list of directories specifying the
  module search path for `import` statements.
+ `SUNDER_SYSASM_PATH` => Location of the platform specific `sys.asm` file that
  defines the program entry point as well as low-level operating system and
  hardware abstractions when using the NASM backend. If this environment
  variable is not set, then the default path is used.
+ `SUNDER_CC` => Selects the C compiler to be used when compiling with the C
  backend. Currently, `SUNDER_CC=clang` and `SUNDER_CC=gcc` are supported. If
  this environment variable is not set, then the default C compiler is used.
+ `SUNDER_CFLAGS` => Space-separated list of additional flags passed to the C
  compiler when compiling with the C backend.

## Using Sunder as a Scripting Language
Sunder can be used for scripting by adding `#!/usr/bin/env sunder-run` (or
equivalent) as the first line of a Sunder source file. If this environment
  variable is not set, then the default path, `$SUNDER_HOME/lib/sys/sys.asm`,
  is used.

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
