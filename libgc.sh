#!/bin/bash
set -e

git submodule init
git submodule update

CFLAGS="$CFLAGS -g -O3"
DEBUG="--enable-gc-debug --enable-gc-assertions --enable-debug"
#NOTHREADS="--disable-threads"
THREADS="--enable-threads=posix"
#THREADS+=" --enable-thread-local-alloc --enable-parallel-mark"
OPS="--disable-dependency-tracking"
cd libgc
if ! cd libatomic_ops; then
    cp -r ../libatomic_ops .
else
    cd ..
fi
CFLAGS="$CFLAGS" ./configure $DEBUG $NOTHREADS $THREADS $OPS
make
mkdir -p .libs/objs
rm -rf .libs/objs/*
cd .libs/objs
cp ../libgc.a .
ar -x libgc.a
