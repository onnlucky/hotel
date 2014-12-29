#!/bin/bash

make

PASS=0
FAIL=0

for i in *.tl; do
    REF=${i/.tl/.ref}
    LOG=${i/.tl/.log}
    ERR=${i/.tl/.err}
    #TL_MODULE_PATH=../../modules ../../tl ../../tlcompiler $i
    #../../tlcompiler $i
    ../../tl --boot ${i}b 2>$ERR | diff $REF - >$LOG

    if (( $? == 0 )); then
        PASS=$((PASS+1))
        echo "PASS: $i"
    else
        FAIL=$((FAIL+1))
        echo "FAIL: $i"
    fi
done
for i in *.tl-skip; do
    echo "SKIPPING: $i"
done

if (( FAIL > 0)); then
    echo "pass: $PASS"
    echo "fail: $FAIL"
    exit 1
fi
echo "pass: $PASS"

