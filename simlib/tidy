#!/bin/sh

if [ -z ${MESON_SOURCE_ROOT+x} ]; then
	BUILD_DIR="build/"
else
	cd "$MESON_SOURCE_ROOT"
	BUILD_DIR="$MESON_BUILD_ROOT"
fi

if [ "$#" = 0 ]; then
	(cd "$BUILD_DIR" && nice run-clang-tidy.py -format -fix)
else
	nice clang-tidy -p "$BUILD_DIR" --fix --fix-errors --format-style=file "$@"
fi
