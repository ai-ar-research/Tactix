# Installing Tactix

## The solver binary

Each [release](https://github.com/ai-ar-research/Tactix/releases) has a
statically linked `tactix` for Linux on x86-64. It needs no libraries and runs
on any x86-64 Linux distribution; `gzip` and `xz` on the `PATH` are needed only
for compressed input.

```sh
tar -xzf tactix-VERSION-linux-x86_64.tar.gz
tactix-VERSION-linux-x86_64/bin/tactix formula.cnf proof
```

`SHA256SUMS` in the release lists the checksum of every file; check a
download with `sha256sum -c SHA256SUMS --ignore-missing`.

Three releases matter for the paper and current development:

| Release | Commit | Use |
|---|---|---|
| `v0.1.0-0f7c10174` | `0f7c10174`, the version evaluated in the paper | reproducing the paper's SAT Competition results |
| `v0.2.1` (current source) | whole-run PBP by default, without the retired automatic proof selector | everything else |
| `v0.1.0` | first public installable release | compatibility and historical comparison |

The current source uses whole-run PBP by default. The paper release and its
original proof-mode behavior remain reproducible from its versioned artifact.
See the repository changelog for the full version history.

## The library

The library is a static archive, `libtactix.a`, with the public header
`tactix.h`. Strategies written in C or C++ link against it (see
[c-api.md](c-api.md)).

### From a release

The release package contains

```
bin/tactix
include/tactix.h
lib/libtactix.a
lib/cmake/Tactix/        CMake package
lib/pkgconfig/tactix.pc  pkg-config file
```

Unpack it anywhere, for example to `$HOME/.local` (a build from source may
use `lib64` or `lib/x86_64-linux-gnu` instead of `lib`).
The library was built on Ubuntu 24.04 with GCC 13: link it with GCC 13 or newer
(or a Clang that uses libstdc++ 13 or newer) on a system with glibc 2.38 or
newer (Ubuntu 24.04, Debian 13, Fedora 39 and later). On older systems, build
it from source as below. The static `tactix` binary has no such requirement.

### From source

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-O3 -DTACTIX_BUILD_TESTS=OFF
cmake --build build --parallel --target tactix tactix_archive
cmake --install build --prefix $HOME/.local
```

Requirements are those of [getting-started.md](getting-started.md): a C++20
compiler, CMake 3.25 or newer, and zlib. `-DTACTIX_STATIC_BINARY=ON` links the
`tactix` executable statically, as the release binaries are.

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
([getting-started.md](getting-started.md#use-tactix-as-a-library)).
