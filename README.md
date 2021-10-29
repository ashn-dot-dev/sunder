# The Sunder Programming Language
Sunder is a work-in-progress C-like systems programming language and compiler
for x86-64 Linux.

See [this blog post](https://www.ashn.dev/blog/2021-10-05-i-wrote-a-compiler-for-my-own-programming-language.html)
for a quick overview of the features supported within the language.

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
The script `misc/build-debug.sh` will perform a full build/test cycle using
appropriate debug flags. This is likely the script you want to be using as your
build driver when working on the compiler:
```sh
$ sh misc/build-debug.sh
```
By default `build-debug.sh` will run the `clean` and `all` make targets, but
this can be overridden. See `sh misc/build-debug.sh --help` for details.

Specific compiler/compiler-flag combinations include:
```sh
$ make <targets> CFLAGS='$(C99_DBG)'  # POSIX c99 (debug)
$ make <targets>                      # POSIX c99 (release)

$ # Use CC=clang for Clang or CC=gcc for GCC
$ make <targets> CC=clang CFLAGS='$(GNU_DBG)'              # clang/gcc (debug)
$ make <targets> CC=clang CFLAGS='$(GNU_DBG) $(SANITIZE)'  # clang/gcc (debug with Address Sanitizer)
$ make <targets> CC=clang CFLAGS='$(GNU_REL)'              # clang/gcc (release)
```

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
import "std/io.sunder";

func main() void {
    std::println("Hello, world!");
}
```
```sh
$ sunder-compile -o hello hello.sunder
$ ./hello
Hello, world!
```

The `-o FILE` option determines the name of the output executable. If this
option is not provided then the output executable will default to the name
`a.out`.

The intermediate files `FILE.asm` and `FILE.o` (output executable name plus
`.asm` and `.o` extensions) are generated during compilation and then
subsequently removed after the output executable has been created. The `-k` or
`--keep` flags will instruct the compiler *not* to remove these files (useful
for debugging).

```sh
$ sunder-compile -o hello hello.sunder -k
$ ls hello*
hello  hello.asm  hello.o  hello.sunder

```

## License
Sunder is distributed under the terms of the Apache License (Version 2.0).

See LICENSE for more information.
