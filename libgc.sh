#!/bin/bash
set -e

CFLAGS="-g -O3"
DEBUG="--enable-gc-debug --enable-gc-assertions --enable-debug"
#NOTHREADS="--disable-threads"
THREADS="--enable-threads=posix"
#THREADS+=" --enable-thread-local-alloc --enable-parallel-mark"
OPS="--disable-dependency-tracking --enable-cplusplus"
if ! cd libgc; then
    git clone git://github.com/ivmai/bdwgc.git libgc
    cd libgc
    git checkout origin/release-7_2
fi
if ! cd libatomic_ops; then
    git clone git://github.com/ivmai/libatomic_ops.git
    cd libatomic_ops
    git checkout origin/release-7_2
    cd ..
else
    cd ..
fi
CFLAGS="$CFLAGS" ./configure $DEBUG $NOTHREADS $THREADS $OPS
make
rm -rf objs
mkdir objs
cd objs
cp ../.libs/libgc.a .
ar -x libgc.a
