# sip [![Build Status](https://travis-ci.org/varqox/sip.svg)](https://travis-ci.org/varqox/sip)

Sip is a tool for preparing and managing Sim problem packages

## How to build

You will need `meson` build system to be installed (on most platforms it is in the _meson_ package).

### Dependencies
You need a C++ compiler with C++17 support. Meson will point out missing dependencies.

Here are instructions how to satisfy dependencies on some systems:
#### Debian / Ubuntu

```sh
sudo apt-get install meson g++-multilib libseccomp-dev libzip-dev
```

#### Arch Linux

```sh
sudo pacman -S meson gcc libseccomp libzip
```

### Tutorial: release build step by step
First of all clone the repository and all its submodules:
```sh
git clone --recursive https://github.com/varqox/sip && cd sip
```
Then setup build directory:
```sh
meson setup release-build/ -Dbuildtype=release
```
And finally build:
```
ninja -C release-build/ base
```

The above command creates a dynamic executable `release-build/sip`.

Installing is done via:
```sh
sudo meson install -C release-build
```

Now you can use sip command:
```sh
sip
```

For more information about how to use it and what it can do type:
```sh
sip help
```
#### Upgrading
Just type:

```sh
git pull --rebase --autostash
git submodule update --init --recursive
meson -C release-build base
sudo meson install -C release-build
```

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
cd release-build/ # or other build directory
# if you want to install to default prefix
meson install
# specifying other install directory
DESTDIR=other/install/dir/ meson install
```

## Running tests
Run after building:
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
./tidy.sh
```

#### Specified sources
To lint specified sources:
```sh
./tidy.sh path/to/source.cc other/source.h
```

### Static analysis
```sh
ninja -C build scan-build
```
