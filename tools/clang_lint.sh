#!/usr/bin/env bash

# clang_lint.sh
# Runs clang-format and clang-tidy on the codebase

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

CLANG_FORMAT_ISSUES=0
CLANG_TIDY_ISSUES=0

echo "[CLANG-FORMAT] C/C++ Code Formatting Check"
# Run clang-format to enforce style
find src tools -type d -name 'zlib' -prune -o -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' \) -exec clang-format -i {} + || CLANG_FORMAT_ISSUES=1

if [ $CLANG_FORMAT_ISSUES -ne 0 ]; then
    echo "clang-format encountered errors."
    exit 1
fi

echo "[CLANG-TIDY] Static Analysis"
if [ -f build/compile_commands.json ]; then
    find src -type d -name 'zlib' -prune -o -type f \( -name '*.c' -o -name '*.cpp' \) -exec clang-tidy -p build {} + || CLANG_TIDY_ISSUES=1
else
    echo "No compile_commands.json, skipping advanced clang-tidy! Build first."
fi

if [ $CLANG_TIDY_ISSUES -ne 0 ]; then
    echo "clang-tidy found issues."
    exit 2
fi

exit 0
