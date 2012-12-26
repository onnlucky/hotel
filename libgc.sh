#!/bin/bash
set -e

CFLAGS="-g -O3"
DEBUG="--enable-gc-debug --enable-gc-assertions --enable-debug"
#NOTHREADS="--disable-threads"
THREADS="--enable-threads=posix"
#THREADS+=" --enable-thread-local-alloc --enable-parallel-mark"
OPS="--disable-dependency-tracking --enable-cplusplus"
cd libgc
if ! cd libatomic_ops; then
    ln -sf ../libatomic_ops .
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
