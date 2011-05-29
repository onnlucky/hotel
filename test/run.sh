#!/bin/bash

PASS=0
FAIL=0

for i in *.tl; do
    REF=${i/.tl/.ref}
    LOG=${i/.tl/.log}
    ../tl $i 2>/dev/null | diff $REF - >$LOG

    if (( $? == 0 )); then
        PASS=$((PASS+1))
        echo "PASS: $i"
    else
        FAIL=$((FAIL+1))
        echo "FAIL: $i"
    fi
done

if (( FAIL > 0)); then
    echo "pass: $PASS"
    echo "fail: $FAIL"
    exit 1
fi
echo "pass: $PASS"

