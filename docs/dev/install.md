# Installing Tactix

## The solver binary

Build it from source as in [cli.md](../cli.md). With
`-DTACTIX_STATIC_BINARY=ON` the `tactix` executable is linked statically: it
then needs no libraries and runs on any x86-64 Linux distribution (`gzip` and
`xz` on the `PATH` are needed only for compressed input).

## The library

The library is a static archive, `libtactix.a`, with the public header
`tactix.h`. Strategies written in C or C++ link against it (see
[c-api-reference.md](c-api-reference.md)).

### From source

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-O3 -DTACTIX_BUILD_TESTS=OFF
cmake --build build --parallel --target tactix tactix_archive
cmake --install build --prefix $HOME/.local
```

Requirements are those of [cli.md](../cli.md): a C++20
compiler, CMake 3.25 or newer, and zlib.

### Using it

With CMake:

```cmake
find_package(Tactix REQUIRED)          # add the prefix to CMAKE_PREFIX_PATH
add_executable(my_strategy my_strategy.c)
target_link_libraries(my_strategy PRIVATE Tactix::tactix)
```

With pkg-config:

```sh
export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig
cc -o my_strategy my_strategy.c $(pkg-config --cflags --libs tactix)
```

Both link the whole archive (`--whole-archive`). Tactix registers its rules
in static initializers; a plain `-ltactix` lets the linker drop rules that the
program never names, and those rules would then be missing at run time.

Alternatively, add the source tree to a CMake project with
`add_subdirectory(path/to/tactix)` and link `tactix_lib`
([c-api.md](../c-api.md)).
