#!/bin/sh
TL_MODULE_PATH=./modules:./cmodules $TOOL ./tl runspec.tl "$@"
