#!/bin/bash
# Script is used to build Coverity scan

rm -rf cov-int/
rm -f sim-cov-build.tar.gz
make clean
cov-build --dir cov-int make -j 4 DEBUG=1 CXX=g++ && tar caf sim-cov-build.xz cov-int
