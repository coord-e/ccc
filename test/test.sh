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

function items() {
    try "$@"
}

function expr() {
    local expected="$1"
    local input="$2"
    try "$expected" "return ($input);"
}

# just a number
expr 0 0
expr 42 42

# arithmetic
expr 42 "24 + 18"
expr 30 "58 - 28"
expr 10 "5 * 2"
expr 4 "16 / 4"

expr 20 "8 + 3 * 4"
expr 54 "(11 - 2) * 6"
expr 10 "9 / 3 + 7"
expr 8 "8 / (4 - 3)"
expr 35 "8 + 3 * 5 + 2 * 6"

expr 55 "1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10"
expr 55 "((((((((1 + 2) + 3) + 4) + 5) + 6) + 7) + 8) + 9) + 10"
expr 55 "1 + (2 + (3 + (4 + (5 + (6 + (7 + (8 + (9 + 10))))))))"

expr 21 "+1+20"
expr 10 "-15+(+35-10)"

expr 1 "10 > 5"
expr 1 "3+3 > 5"
expr 0 "30 == 20"
expr 0 "5 >= 10"
expr 1 "5 >= 5"
expr 1 "30 != 20"

# return statements
items 1 "return 1;";
items 42 "return 2*21;";

# variables
items 10 "decl var; var = 10; return var;"
items 42 "decl va; decl vb; va = 11; vb = 31; decl vc; vc = va + vb; return vc;"
items 50 "decl v; v = 30; v = 50; return v;"

# if
items 5 "if (1) return 5; else return 20;"
items 10 "if (0) return 5; else if (0) return 20; else return 10;"
items 10 "decl a; a = 0; decl b; b = 0; if(a) b = 10; else if (0) return a; else if (a) return b; else return 10;"
items 27 "decl a; a = 15; decl b; b = 2; if(a-15) b = 10; else if (b) return (a + b + 10); else if (a) return b; else return 10;"

# compound
items 5 "{ return 5; }"
items 10 "{ decl a; a = 5; { a = 5 + a; } return a; }"
items 20 "decl a; a = 10; if (1) { a = 20; } else { a = 10; } return a;"
items 30 "decl a; a = 10; if (a) { if (a - 10) { a = a + 1; } else { a = a + 20; } a = a - 10; } else { a = a + 5; } return a + 10;"

# loop
items 55 "decl acc; decl p; acc = 0; p = 10; while (p) { acc = acc + p; p = p - 1; } return acc;"
items 60 "decl acc; acc = 15; do { acc = acc * -2; } while (acc < 0); return acc;"
items 45 "decl j; decl acc; acc = 0; for (j = 0; j < 10; j = j + 1) { acc = acc + j; } return acc;"

echo OK
