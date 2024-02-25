# simlib [![Build Status](https://travis-ci.org/varqox/simlib.svg?branch=master)](https://travis-ci.org/varqox/simlib)

simlib is a C++ utility library for Linux originally created for the [sim project](https://github.com/varqox/sim).

[Some functionality documentation](doc/README.md)

## How to build
You will need `meson` build system to be installed (on most platforms it is in the _meson_ package).

### Dependencies
You need a C++ compiler with C++17 support. Meson will point out missing dependencies.

### Release build
```sh
meson setup release-build/ -Dbuildtype=release -Ddefault_library=both
ninja -C release-build/ base
```

### Development build
```sh
meson setup build/ -Dc_args=-DDEBUG -Dcpp_args='-DDEBUG -D_GLIBCXX_DEBUG' -Db_sanitize=undefined -Db_lundef=false
ninja -C build/ base
```

### Static build
Static build is useful if you want to build executables linked statically.
```sh
meson setup static-build/ -Ddefault_library=static -Dstatic=true
ninja -C static-build/ base
```

## Installing
Run after building:
```sh
# we will use release-build build directory, if you use other just change all release-build below
# if you want to install to default prefix
meson install -C release-build
# specifying other install directory
DESTDIR=other/install/dir/ meson install -C release-build
```

## Running tests
```sh
ninja -C build/ test # or other build directory
```

## Development build targets

### Formating C/C++ sources
To format all sources (clang-format is required):
```sh
ninja -C build format
```

### Linting C/C++ sources

#### All sources
To lint all sources:
```sh
ninja -C build tidy
```
or
```sh
./tidy
```

#### Specified sources
To lint specified sources:
```sh
./tidy path/to/source.cc other/source.h
```

### Static analysis
```sh
ninja -C build scan-build
```
