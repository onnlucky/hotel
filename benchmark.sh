#!/bin/bash
set -e
make clean
BUILD=release make
TL_MODULE_PATH=./modules:./cmodules ./tl rect_bench.tl --publish

