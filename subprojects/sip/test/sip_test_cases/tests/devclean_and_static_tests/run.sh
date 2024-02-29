#!/bin/sh
set -e

script_dir=$(cd -P -- "$(dirname -- "$0")" && pwd -P)
orig_pkg="$script_dir/package"
tmp_dir=$(mktemp -d)
trap 'rm -r "$tmp_dir"' EXIT
cp -rL "$orig_pkg" "$tmp_dir"
pkg="$tmp_dir/package"
sip=$1
[ -z "$sip" ] && echo "Usage: $0 <sip_command>" && false

"$sip" -C "$pkg" gen

test -f "$pkg/in/0a.in"
test -f "$pkg/in/0b.in"
test -f "$pkg/in/0c.in"
test -f "$pkg/in/1.in"
test -f "$pkg/in/2.in"
test -f "$pkg/out/0a.out"
test -f "$pkg/out/0b.out"
test -f "$pkg/out/0c.out"
test -f "$pkg/out/1.out"
test -f "$pkg/out/2.out"

"$sip" -C "$pkg" devclean

test -f "$pkg/in/0a.in"
test -f "$pkg/in/0b.in"
test -f "$pkg/in/0c.in"
test -f "$pkg/out/0a.out"
test -f "$pkg/out/0b.out"
test -f "$pkg/out/0c.out"
test ! -f "$pkg/in/1.in"
test ! -f "$pkg/in/2.in"
test ! -f "$pkg/out/1.out"
test ! -f "$pkg/out/2.out"
