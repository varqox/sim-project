#!/bin/bash
set -e

script_dir=$(cd -P -- "$(dirname -- "$0")" && pwd -P)
orig_pkg="$script_dir/package"
tmp_dir=$(mktemp -d)
trap 'rm -r "$tmp_dir"' EXIT
cp -rL "$orig_pkg" "$tmp_dir"
pkg="$tmp_dir/package"
sip=$1
[ -z "$sip" ] && echo "Usage: $0 <sip_command>" && false

prepare() {
	"$sip" -C "$pkg" clean
}

echo "pattern: main"
prepare
"$sip" -C "$pkg" prog main |& grep -P 'Compiling' -c | diff <(echo 3) -
prepare
"$sip" -C "$pkg" prog main |& grep -P 'main\.(c|cc|cpp)\b' -c | diff <(echo 3) -

echo "pattern: .c"
prepare
"$sip" -C "$pkg" prog .c |& grep -P 'Compiling' -c | diff <(echo 3) -
prepare
"$sip" -C "$pkg" prog .c |& grep -P 'main\.(c|cc|cpp)\b' -c | diff <(echo 3) -

echo "pattern: ."
prepare
"$sip" -C "$pkg" prog . |& grep -P 'Compiling' -c | diff <(echo 3) -
prepare
"$sip" -C "$pkg" prog . |& grep -P 'main\.(c|cc|cpp)\b' -c | diff <(echo 3) -

echo "pattern: .cc"
prepare
"$sip" -C "$pkg" prog .cc |& grep -P 'Compiling' -c | diff <(echo 1) -
prepare
"$sip" -C "$pkg" prog .cc |& grep -P 'main\.cc\b' -c | diff <(echo 1) -

echo "pattern: .cpp"
prepare
"$sip" -C "$pkg" prog .cpp |& grep -P 'Compiling' -c | diff <(echo 1) -
prepare
"$sip" -C "$pkg" prog .cpp |& grep -P 'main\.cpp\b' -c | diff <(echo 1) -

echo "pattern: .p"
prepare
"$sip" -C "$pkg" prog .p |& grep -P 'Compiling' -c | diff <(echo 1) -
prepare
"$sip" -C "$pkg" prog .p |& grep -P 'main\.cpp\b' -c | diff <(echo 1) -

echo "pattern: main.c"
prepare
"$sip" -C "$pkg" prog main.c |& grep -P 'Compiling' -c | diff <(echo 1) -
prepare
"$sip" -C "$pkg" prog main.c |& grep -P 'main\.c\b' -c | diff <(echo 1) -

echo "pattern: main.cc"
prepare
"$sip" -C "$pkg" prog main.cc |& grep -P 'Compiling' -c | diff <(echo 1) -
prepare
"$sip" -C "$pkg" prog main.cc |& grep -P 'main\.cc\b' -c | diff <(echo 1) -

echo "pattern: main.cpp"
prepare
"$sip" -C "$pkg" prog main.cpp |& grep -P 'Compiling' -c | diff <(echo 1) -
prepare
"$sip" -C "$pkg" prog main.cpp |& grep -P 'main\.cpp\b' -c | diff <(echo 1) -
