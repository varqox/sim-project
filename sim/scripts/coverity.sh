#!/bin/bash
# Script is used to build Coverity scan

rm -rf cov-int/
rm -f sim-cov-build.tar.gz
make clean
cp src/include/simlib/meta.h meta.h~
echo '#define static_assert(...)' >> src/include/simlib/meta.h
cov-build --dir cov-int make -j $(grep -c ^processor /proc/cpuinfo) DEBUG=5 CC=gcc CXX=g++ test && tar caf sim-cov-build.xz cov-int
rm src/include/simlib/meta.h
mv meta.h~ src/include/simlib/meta.h
