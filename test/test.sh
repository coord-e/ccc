#!/bin/bash

set -u

readonly BASE_DIR="$(dirname "$BASH_SOURCE")/.."
readonly CCC="$BASE_DIR/build/ccc"

function try() {
    local expected="$1"
    local input="$2"

    local tmp_in="$(mktemp --suffix .c)"
    local tmp_asm="$(mktemp --suffix .s)"
    local tmp_exe="$(mktemp)"

    echo "$input" > "$tmp_in"
    "$CCC" "$tmp_in" -o "$tmp_asm"
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

function try_() {
    local expected="$1"
    local input="$(cat)"
    try "$expected" "$input"
}

function items() {
    local expected="$1"
    local input="$2"
    try "$expected" "int main(int argc, int argv) { $input }"
}

function expr() {
    local expected="$1"
    local input="$2"
    items "$expected" "return ($input);"
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
items 10 "int var; var = 10; return var;"
items 42 "int va; int vb; va = 11; vb = 31; int vc; vc = va + vb; return vc;"
items 50 "int v; v = 30; v = 50; return v;"

# if
items 5 "if (1) return 5; else return 20;"
items 10 "if (0) return 5; else if (0) return 20; else return 10;"
items 10 "int a; a = 0; int b; b = 0; if(a) b = 10; else if (0) return a; else if (a) return b; else return 10;"
items 27 "int a; a = 15; int b; b = 2; if(a-15) b = 10; else if (b) return (a + b + 10); else if (a) return b; else return 10;"

# compound
items 5 "{ return 5; }"
items 10 "{ int a; a = 5; { a = 5 + a; } return a; }"
items 20 "int a; a = 10; if (1) { a = 20; } else { a = 10; } return a;"
items 30 "int a; a = 10; if (a) { if (a - 10) { a = a + 1; } else { a = a + 20; } a = a - 10; } else { a = a + 5; } return a + 10;"

# loop
items 55 "int acc; int p; acc = 0; p = 10; while (p) { acc = acc + p; p = p - 1; } return acc;"
items 60 "int acc; acc = 15; do { acc = acc * -2; } while (acc < 0); return acc;"
items 45 "int i; int acc; acc = 0; for (i = 0; i < 10; i = i + 1) { acc = acc + i; } return acc;"
items 45 "int i; int j; i=0; j=0; while (i<10) { j=j+i; i=i+1; } return j;"
items 1 "int x; x=0; do {x = x + 1; break;} while (1); return x;"
items 1 "int x; x=0; do {x = x + 1; continue;} while (0); return x;"
items 7 "int i; i=0; int j; for (j = 0; j < 10; j = j + 1) { if (j < 3) continue; i = i + 1; } return i;"
items 10 "while(0); return 10;"
items 10 "while(1) break; return 10;"
items 10 "for(;;) break; return 10;"
items 0 "int x; for(x = 10; x > 0; x = x - 1); return x;"
items 30 "int i; int acc; i = 0; acc = 0; do { i = i + 1; if (i - 1 < 5) continue; acc = acc + i; if (i == 9) break; } while (i < 10); return acc;"
items 26 "int acc; acc = 0; int i; for (i = 0; i < 100; i=i+1) { if (i < 5) continue; if (i == 9) break; acc = acc + i; } return acc;"

# functions
try_ 55 << EOF
int sum(int m, int n) {
  int acc;
  acc = 0;
  int i;
  for (i = m; i <= n; i = i + 1)
    acc = acc + i;
  return acc;
}

int main() {
  return sum(1, 10);
}
EOF

try_ 120 << EOF
int fact(int x) {
  if (x == 0) {
    return 1;
  } else {
    return x * fact(x - 1);
  }
}

int main() {
  return fact(5);
}
EOF

try_ 1 << EOF
int is_even(int x) {
  if (x == 0) {
    return 1;
  } else {
    return is_odd(x - 1);
  }
}

int is_odd(int x) {
  if (x == 0) {
    return 0;
  } else {
    return is_even(x - 1);

  }
}

int main() {
  return is_even(20);
}
EOF

try_ 253 << EOF
int ack(int m, int n) {
  if (m == 0) {
    return n + 1;
  } else if (n == 0) {
    return ack(m - 1, 1);
  } else {
    int a;
    a = ack(m, n - 1);
    return ack(m - 1, a);
  }
}

int main() {
  return ack(3, 5);
}
EOF

echo OK
