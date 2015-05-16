#!/bin/bash
# require gcovr tool to be installed; http://gcovr.com; just `pip install gcovr`
set -e
make clean
GCOV=1 make -j4 test || true
cd vm
gcovr -r . --html --html-details -o gcov.html -e '.*test.c'
echo "create html report in vm/gcov.html"

