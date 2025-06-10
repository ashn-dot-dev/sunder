# The Sunder Programming Language
Sunder is a modest systems programming language for Unix-like platforms.

## Dependencies
Sunder tooling should build and run on any supported machine satisfying the
following dependencies:

+ POSIX-compatible `make`
+ [GNU Core Utilities](https://www.gnu.org/software/coreutils/)
+ GNU C compiler ([Clang](https://clang.llvm.org/), [GCC](https://gcc.gnu.org/), and [TCC](https://bellard.org/tcc/) are currently supported)
+ [Emscripten](https://emscripten.org/) (WebAssembly code-generation only)
+ [Clang Format](https://clang.llvm.org/docs/ClangFormat.html) (development only)

Supported platforms include:
+ x86-64 Linux
+ ARM64 Linux
+ ARM64 macOS
+ WebAssembly (via Emscripten)

The top-level Dockerfile defines a Debian image with dependencies for native
x86-64 Linux development pre-installed. You can build and run the Docker image
with:

```sh
$ docker buildx build --platform=linux/amd64 --tag sunder .             # Build the image (do this once)
$ docker run --rm --interactive --tty --volume "$(pwd)":/sunder sunder  # Run the image (do this every time)
```

## Building
### Quick Version
To build the compiler, run `make`.

To execute the test suite and compile the example programs under the `examples`
directory, run `make check examples`.

### Long Version
The top-level `Makefile` contains the following important targets:

+ `build` → Build the compiler (default `make` target).
+ `check` → Run the test suite for the language and standard library.
+ `examples` → Compile the example programs under the `examples` directory.
+ `format` → Run `clang-format` over the compiler sources.
+ `clean` → Remove build artifacts.

Targets will execute with `CC=c99` using release mode `CFLAGS` by default.

Specific `CC`/`CFLAGS` combinations include:

```sh
$ make <targets> CFLAGS='$(C99_DBG)'  # POSIX c99 (debug)
$ make <targets> CFLAGS='$(C99_REL)'  # POSIX c99 (release)

$ # Use CC=clang for Clang or CC=gcc for GCC
$ make <targets> CC=clang CFLAGS='$(GNU_DBG)'  # clang/gcc (debug)
$ make <targets> CC=clang CFLAGS='$(GNU_REL)'  # clang/gcc (release)
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
`sunder-compile -h`. You may need to source your `.profile` in new shells until
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
option is not provided, the output file will default to the name `a.out`.

Intermediate files of the form `OUT.tmp.*` for output program `OUT` are
generated during compilation and subsequently removed after the output file has
been created. The `-k` flag will instruct the compiler *not* to remove these
files.

```sh
$ sunder-compile -k -o hello examples/hello.sunder
$ ls hello*
hello  hello.tmp.c

$ SUNDER_CC=emcc SUNDER_ARCH=wasm32 SUNDER_HOST=emscripten sunder-compile -k -o hello.html examples/hello.sunder
$ ls hello*
hello.html  hello.html.tmp.c  hello.js  hello.wasm
```

The `-g` flag will instruct the compiler to generate debug information. The use
of `-g` in combination with `-k` facilitates debugging with GDB and LLDB.

```sh
$ sunder-compile -g -k -o hello examples/hello.sunder
$ lldb hello
(lldb) target create "hello"
Current executable set to '/home/ashn/sources/sunder/hello' (x86_64).
(lldb) b std_print_line
Breakpoint 1: where = hello`std_print_line + 58 at hello.tmp.c:13191:9, address = 0x0000000000020b06
(lldb) run
Process 133770 launched: '/home/ashn/sources/sunder/hello' (x86_64)
Process 133770 stopped
* thread #1, name = 'hello', stop reason = breakpoint 1.1
    frame #0: 0x0000555555574b06 hello`std_print_line(__sunder_argument_0_writer=std_writer @ 0x00007fffffffe4d0, __sunder_argument_1_str=(start = "Hello, world!", count = 13)) at hello.tmp.c:13191:9
   13188        std_print_line(std_writer __sunder_argument_0_writer, __sunder_slice_of_byte __sunder_argument_1_str)
   13189        {
   13190            // var return: void
-> 13191            int __sunder_return = /* zero-sized local */0;
   13192            // var result: std::result[[void, *std::error_info]]
   13193            std_result_TEMPLATE_BGN_void_COMMA_pointer_to_std_error_info_TEMPLATE_END __sunder_local_0_result = {/* uninit */0};
   13194            /// [std/std.sunder:1602] STATEMENT STMT_ASSIGN
(lldb)
```

The following environment variables affect compiler behavior:

**`SUNDER_CC`** selects the C compiler to be used when compiling generated C.
Currently, `SUNDER_CC=clang`, `SUNDER_CC=gcc`, and `SUNDER_CC=emcc` are
supported. If `SUNDER_CC` is not set, then the default C compiler is used.

**`SUNDER_CFLAGS`** is a space-separated list of additional flags passed to the
C compiler.

**`SUNDER_SEARCH_PATH`** is a colon-separated list of directories specifying
the module search path for `import` and `embed` statements.

**`SUNDER_ARCH`** specifies the target architecture to build for. Currently,
`SUNDER_ARCH=amd64`, `SUNDER_ARCH=arm64`, and `SUNDER_ARCH=wasm32` are
supported. If `SUNDER_ARCH` is not set, then the default architecture specified
by `sunder-platform arch` is used.

**`SUNDER_HOST`** specifies the target operating system to build for.
Currently, `SUNDER_HOST=emscripten`, `SUNDER_HOST=linux`, and
`SUNDER_HOST=macos` are supported. If `SUNDER_HOST` is not set, then the
default host specified by `sunder-platform host` is used.

## Compiling to WebAssembly
Sunder supports compiling to WebAssembly via
[Emscripten](https://emscripten.org/). When compiling to WebAssembly, specify
`emcc`, `wasm32`, and `emscripten` as the C compiler, target architecture, and
target host, respectively.

```sh
$ SUNDER_CC=emcc \
    SUNDER_CFLAGS="-sSINGLE_FILE --shell-file ${SUNDER_HOME}/lib/sys/sys.wasm32-emscripten.html" \
    SUNDER_ARCH=wasm32 \
    SUNDER_HOST=emscripten \
    sunder-compile -o hello.html examples/hello.sunder
$ firefox --new-window "file://$(realpath hello.html)"
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

    var name = std::ascii::view_trimmed(line);
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
