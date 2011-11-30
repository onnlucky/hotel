#!/bin/bash
set -e

DEBUG="--enable-gc-debug"
#NOTHREADS="--disable-threads"
THREADS="--enable-threads=posix --enable-thread-local-alloc --enable-parallel-mark"
if ! cd libgc; then
    git clone git://github.com/ivmai/bdwgc.git libgc
    cd libgc
    git checkout origin/release
fi
if ! cd libatomic_ops; then
    git clone git://github.com/ivmai/libatomic_ops.git
    cd libatomic_ops
    git checkout origin/release
    cd ..
else
    cd ..
fi
./configure $DEBUG $NOTHREADS $THREADS
make
rm -rf objs
mkdir objs
cd objs
cp ../.libs/libgc.a .
ar -x libgc.a
