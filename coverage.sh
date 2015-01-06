#!/bin/bash
# require gcovr tool to be installed; http://gcovr.com; just `pip install gcovr`
set -e
make clean
GCOV=1 make test || true
gcovr -r . --html --html-details -o gcov.html -e '.*test.c'
echo "open gcov.html"
open gcov.html

