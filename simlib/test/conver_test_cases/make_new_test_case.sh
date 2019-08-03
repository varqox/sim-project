#!/bin/sh
set -e

if [ "$#" -ne 2 ]; then
	echo "Usage: ./make_new_test_case.sh <path_to_sip_package> <path_to_conver_options>"
	exit 1
fi

NUM=$(($(ls -v *package.zip | tail -n 1 | sed s/package.zip//) + 1))

echo "Preparing test case $NUM"
cp "$2" "${NUM}conver.options"
sip -C "$1" zip
mv "${1%/}.zip" "${NUM}package.zip"

touch "${NUM}pre_simfile.out"
touch "${NUM}post_simfile.out"
touch "${NUM}conver_log.out"
touch "${NUM}judge_log.out"

echo Done.
