#!/bin/bash
set -e

DEBUG="--enable-gc-debug"
NOTHREADS="--disable-threads"
#THREADS="--enable-threads=posix --enable-thread-local-alloc --enable-parallel-mark"
if ! cd libgc; then
    git clone git://github.com/ivmai/bdwgc.git -b release libgc
    cd libgc
fi
if ! cd libatomic_ops; then
    git clone git://github.com/ivmai/libatomic_ops.git -b release
else
    cd ..
fi
./configure $DEBUG $NOTHREADS $THREADS
make
cp .libs/libgc.a ..

