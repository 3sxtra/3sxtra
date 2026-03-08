#!/bin/bash
cd build_tests || exit 1
FAILED=0
for f in tests/unit/*.exe; do
    echo "Running $f"
    ./$f
    if [ $? -ne 0 ]; then
        echo "$f FAILED!"
        FAILED=1
    fi
done
exit $FAILED
