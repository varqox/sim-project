# Sim project

Sim is an open source platform for carrying out algorithmic contests and programming classes.

This repository contains all Sim projects as a monorepo. You can explore individual repositories here:

## Subprojects

### [subprojects/sim](subprojects/sim/)
Web server and utilities around that.

### [subprojects/sip](subprojects/sip/)
Tool for creating and managing problem packages.

### [subprojects/simlib](subprojects/simlib/)
The library used internally by the subprojects. It contains common functionality e.g. the Sim Sandbox.

## Development

### Setting up build directory

First of all, you need to set up the build directory. The simplest way to do it is like this:
```sh
meson setup build
```
This will setup build directory `build/` in which all subproject will be build but no install targets are made. To build only subprojects you can use `-Dbuild=suproject1,subproject2,...` option. E.g.
```sh
meson setup build --wipe -Dbuild=sip
```

For detailed option description you can inspect [meson_options.txt](meson_options.txt) file.

By default no subproject is marked to be installed on `meson install -C build`. Marking subprojects to be installed is done with '-Dinstall=subproject1,subproject2,...' option. E.g.
```sh
meson setup build --wipe -Dbuild=sim,sip -Dinstall=sip
```
This command will build `sim` and `sip` subprojects, but will mark only `sip` to be installed.

Specifying subpojects to install is also adds them to the list of subprojects to build.

For detailed option description you can inspect [meson_options.txt](meson_options.txt) file.

### Building
Just run
```sh
meson compile -C build
```
or
```sh
ninja -C build
```

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
