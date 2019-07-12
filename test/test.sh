#!/bin/bash

set -u

readonly BASE_DIR="$(dirname "$BASH_SOURCE")/.."
readonly CCC="$BASE_DIR/build/ccc"

function try() {
    local expected="$1"
    local input="$2"

    local tmp_asm="$(mktemp --suffix .s)"
    local tmp_exe="$(mktemp)"

    "$CCC" "$input" > "$tmp_asm"
    gcc -o "$tmp_exe" "$tmp_asm"
    "$tmp_exe"
    local actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "$input => $actual"
    else
        echo "$input => $expected expected, but got $actual"
        exit 1
    fi
}

# just a number
try 0 0
try 42 42

# arithmetic
try 42 "24 + 18"
try 30 "58 - 28"
try 10 "5 * 2"
try 4 "16 / 4"

try 20 "8 + 3 * 4"
try 54 "(11 - 2) * 6"
try 10 "9 / 3 + 7"
try 8 "8 / (4 - 3)"
try 35 "8 + 3 * 5 + 2 * 6"

try 55 "1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10"
try 55 "((((((((1 + 2) + 3) + 4) + 5) + 6) + 7) + 8) + 9) + 10"
try 55 "1 + (2 + (3 + (4 + (5 + (6 + (7 + (8 + (9 + 10))))))))"

echo OK
