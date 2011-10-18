#!/bin/bash
set -e

DEBUG="--enable-gc-debug"
#THREADS="--enable-threads=posix --enable-thread-local-alloc --enable-parallel-mark"
if ! cd bdwgc; then
    git clone git://github.com/ivmai/bdwgc.git -b release
    cd bdwgc
fi
if ! cd libatomic_ops; then
    git clone git://github.com/ivmai/libatomic_ops.git -b release
else
    cd ..
fi
./configure $DEBUG $THREADS
make
cp .libs/libgc.a ..

