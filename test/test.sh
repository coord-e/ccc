#!/bin/bash

set -u

readonly BASE_DIR="$(dirname "$BASH_SOURCE")/.."
readonly CCC="$BASE_DIR/build/$1"

function try() {
    local expected="$1"
    local input="$2"

    local tmp_in="$(mktemp --suffix .c)"
    local tmp_asm="$(mktemp --suffix .s)"
    local tmp_exe="$(mktemp)"

    local tmp_tks="$(mktemp)"
    local tmp_ast1="$(mktemp)"
    local tmp_ast2="$(mktemp)"
    local tmp_ir1="$(mktemp --suffix .gv)"
    local tmp_ir2="$(mktemp --suffix .gv)"

    echo "$input" > "$tmp_in"
    "$CCC" "$tmp_in" \
      -o "$tmp_asm" \
      --emit-tokens "$tmp_tks" \
      --emit-ast1 "$tmp_ast1" \
      --emit-ast2 "$tmp_ast2" \
      --emit-ir1 "$tmp_ir1" \
      --emit-ir2 "$tmp_ir2"
    gcc -o "$tmp_exe" "$tmp_asm"
    "$tmp_exe"
    local actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "$input => $actual"
    else
        echo "$input => $expected expected, but got $actual"
        echo "input: $tmp_in"
        echo "tokens: $tmp_tks"
        echo "ast1: $tmp_ast1"
        echo "ast2: $tmp_ast2"
        echo "ir1: $tmp_ir1"
        echo "ir2: $tmp_ir2"
        echo "output: $tmp_asm"
        echo "executable: $tmp_exe"
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
expr 210 "1 + (2 + (3 + (4 + (5 + (6 + (7 + (8 + (9 + (10 + (11 + (12 + (13 + (14 + (15 + (16 + (17 + (18 + (19 + 20))))))))))))))))))"

expr 21 "+1+20"
expr 10 "-15+(+35-10)"

expr 2 "5 % 3"
expr 6 "111 % 7"

expr 1 "10 > 5"
expr 1 "3+3 > 5"
expr 0 "30 == 20"
expr 0 "5 >= 10"
expr 1 "5 >= 5"
expr 1 "30 != 20"

expr 0 "!237"
expr 18 "~237"

expr 0 "0 || 0"
expr 1 "1 || 0"
expr 1 "1 || 1"
expr 0 "0 && 0"
expr 0 "1 && 0"
expr 1 "1 && 1"

expr 16 "2 << 3"
expr 32 "256 >> 3"
expr 239 "237 | 106"
expr 135 "237 ^ 106"
expr 104 "237 & 106"

try_ 0 <<EOF
int main() {
  unsigned char c = 255;
  int i = c; // zext is expected, not sext
  return i == -1; // false
}
EOF

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
items 45 "int i; int acc; acc = 0; for (i = 0; i < 10; ++i) { acc = acc + i; } return acc;"
items 45 "int i; int j; i=0; j=0; while (i<10) { j=j+i; i=i+1; } return j;"
items 1 "int x; x=0; do {x = x + 1; break;} while (1); return x;"
items 1 "int x; x=0; do {x = x + 1; continue;} while (0); return x;"
items 7 "int i; i=0; int j; for (j = 0; j < 10; j++) { if (j < 3) continue; i = i + 1; } return i;"
items 10 "while(0); return 10;"
items 10 "while(1) break; return 10;"
items 10 "for(;;) break; return 10;"
items 0 "int x; for(x = 10; x > 0; x--); return x;"
items 30 "int i; int acc; i = 0; acc = 0; do { i = i + 1; if (i - 1 < 5) continue; acc = acc + i; if (i == 9) break; } while (i < 10); return acc;"
items 26 "int acc; acc = 0; int i; for (i = 0; i < 100; ++i) { if (i < 5) continue; if (i == 9) break; acc = acc + i; } return acc;"

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
int is_odd(int);

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
    return ack(m - 1, ack(m, n - 1));
  }
}

int main() {
  return ack(3, 5);
}
EOF

# pointers
items 3 "int x; x = 3; int* y; y = &x; return *y;"
items 5 "int b; b = 10; int* a; a = &b; *a = 5; return b;"
try_ 80 <<EOF
int main() {
  int i = 10;
  int j = i + 20;
  int *p = &j;
  int k = j + i + 10;
  int v = *p + k;
  return v;
}
EOF
try_ 10 << EOF
int change_it(int* p) {
  if (*p == 0) {
    *p = 10;
  } else {
    *p = *p - 1;
  }
}

int main() {
  int v;
  v = 2;
  change_it(&v);
  change_it(&v);
  change_it(&v);
  return v;
}
EOF

# arrays
items 8 "int a[2]; a[0] = 3; a[1] = 5; return a[0] + a[1];"
items 3 "int a[2]; a[0] = 1; a[1] = 2; int *p; p = a; return *p + *(p + 1);"
items 8 "int a[2][5]; a[0][2] = 3; a[1][4] = 5; return a[0][2] + a[1][4];"
items 16 "int a[2]; int b[2]; int c; c = 10; b[1] = 5; a[1] = 1; return c + b[1] + a[1];"
try_ 12 << EOF
int nth_of(int* a, int i) {
  return a[i];
}

int main() {
  int ary[5];
  int i;

  for (i = 0; i < 5; i++) {
    ary[i] = i * 2;
  }

  int v0;
  int v1;
  int v2;
  v0 = nth_of(ary, 0);
  v1 = nth_of(ary, 2);
  v2 = nth_of(ary, 4);
  return v0 + v1 + v2;
}
EOF

# cast
items 10 "char a; a = (char)10; return (int)a;"
items 5 "int* p; int a[3]; unsigned long addr; addr = (unsigned long)a + (unsigned long)4; a[1] = 5; p = (int*)addr; return *p;"

# conditional operator
expr 10 "1 ? 10 : 5"
expr 25 "0 ? 10 : 25"

# compound assignemnt
items 5 "int a; a = 2; a += 3; return a;"
items 5 "int a; a = 10; a -= 5; return a;"
items 237 "int a; a = 237; a |= 106; return a-=2;"
items 20 "int a[3]; a[0] = 10; a[1] = 20; a[2] = 30; int* p; p = a; p+=1; return *p;"
items 10 "int a[3]; a[0] = 10; a[1] = 20; a[2] = 30; int* p; p = &a[2]; return *(p-=2);"
items 4 "int a; a = 1; int b; b = a++; return b++ + ++a;";
items 8 "int a; a = 5; int b; b = a--; return --b + a--;";
items 20 "int a[3]; a[0] = 5; a[1] = 10; a[2] = 15; int* p; p = a; int* b; b = p++; return *b + *(++p);"

# comma
expr 3 "(1, 2, 3)"
items 1 "int a; a = 0; return (a+=1, a);"

# sizeof
items 4 "int a; a = 10; return (int)sizeof(a);"
items 1 "char a; return (int)sizeof(a);"
items 2 "short a; return (int)sizeof(a);"
items 40 "int a[10]; return (int)sizeof(a);"
items 80 "long a[2][5]; return (int)sizeof(a);"
expr 4 "(int)sizeof(int)";
expr 1 "(int)sizeof(char)";
expr 8 "(int)sizeof(char*)";
expr 8 "(int)sizeof(int*)";
expr 40 "(int)sizeof(int[10])";

# labels
items 0 "int a; a = 0; goto x; a = 5; x: ; return a;"
items 55 "int acc; int p; acc = 0; p = 10; loop: if(!p) goto end; acc = acc + p; p = p - 1; goto loop; end: return acc;"
items 60 "int acc; acc = 15; loop: acc = acc * -2; if (acc < 0) goto loop; return acc;"

# switch-case
items 10 "int a; a = 0; switch (3) { case 0: return 2; case 3: a = 10; break; case 1: return 0; } return a;"
items 10 "int a; a = 0; switch (3) { case 0: return 2; default: a = 10; break; } return a;"
items 5 "int a; a = 5; switch (3) { ++a; } return a;"
try_ 42 <<EOF
int main(void) {
  int x;
  x = 0;
  while (1) {
    switch (x) {
    case 0:
    case 2:
    case 4:
      x += 3;
      continue;
    case 1:
    case 3:
    case 5:
      x -= 1;
      continue;
    case 28:
      x = 42;
      break;
    default:
      x *= 2;
      continue;
    }
    break;
  }
  return x;
}
EOF
try_ 0 <<EOF
int main() {
  while(1) {
    while(1) {
      while(1) {
        break;
      }
      break;
    }
    break;
  }
  return 0;
}
EOF

# globals
try_ 42 <<EOF
int var;
void modify() {
  var++;
}
void modify2(int* p) {
  *p+=40;
}
int main() {
  var = 0;
  modify();
  modify();
  modify2(&var);
  return var;
}
EOF
try_ 30 <<EOF
long vs[5];
void modify() {
  vs[3] = (long)10;
}
long get() {
  return vs[0] + vs[3] + vs[4];
}
int main() {
  vs[0] = (long)5;
  vs[4] = (long)15;
  modify();
  return (int)get();
}
EOF
expr 5 "(int)sizeof(\"abcd\")"
expr 97 "(int)\"gyaa\"[2]"
expr 0 "(int)\"hello\"[5]"

# initializers
items 10 "int a = 10; return a;"
items 10  "int i = 10; int a[2] = {i, i}; return a[1];"
items 30 "int a[3] = {10, 20, 30}; return a[2];"
items 60 "int a[3] = {10, 20, 30}; int i = 0; int acc = 0; for (; i < 3; i++) acc += a[i]; return acc;"
items 60 "int a[2][3] = {{10, 20, 30}, {40, 50, 60}}; return a[1][2];"
items 3 "int a[4][1] = {{1}, {2}, {3}, {4}}; return a[2][0];"
items 24 "int a[4][3][2] = {{{1, 2}, {3, 4}, {5, 6}}, {{7, 8}, {9, 10}, {11, 12}}, {{13, 14}, {15, 16}, {17, 18}}, {{19, 20}, {21, 22}, {23, 24}}}; return a[3][2][1];"
items 21 "int a[3][2] = {{1, 2}, {3, 4}, {5, 6}}; int j = 0; int acc = 0; for (; j < 3; j++) { int i = 0; for (; i < 2; i++) acc += a[j][i]; } return acc;"
items 108 "char s[6] = \"hello\"; return (int)s[3];"
items 108 "char s[6] = {\"hello\"}; return (int)s[3];"
items 108 "char *s = {\"hello\"}; return (int)s[3];"
items 100 "char s[2][6] = {\"hello\", \"world\"}; return (int)s[1][4];"
items 0 "char s[6] = {\"hello\"}; return (int)s[5];"
items 0 "char s[10] = \"omg\"; return (int)s[3];"
items 0 "int a[10] = {}; return a[5];"
items 0 "int a[2][4] = {}; return a[1][2];"
items 0 "int a[2][4] = {{1,2}, {2}}; return a[1][2];"
try_ 21 <<EOF
int a[2][3] = {{1, 2, 3}, {4, 5, 6}};

int main(int argc, char** argv) {
  int acc = 0;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 3; j++) {
      acc += a[i][j];
    }
  }
  return acc;
}
EOF
try_ 0 <<EOF
long a[2][4];

int main(int argc, char** argv) {
  return (int)a[1][3];
}
EOF
try_ 114 <<EOF
char *strings[4] = {"str1", "str2", "str3", "str4"};

int main(int argc, char** argv) {
  return (int)strings[3][2];
}
EOF
try_ 10 <<EOF
long v;
long* ptr = &v;

int main() {
  v = (long)10;
  return (int)*ptr;
}
EOF
items 3 "int a[] = {1, 2, 3}; return a[2];"
try_ 137 <<EOF
char *strs[] = {"hello", "world", "my", "initializers"};

int main(int argc, char** argv) {
  return (int)sizeof(strs) + (int)strs[3][2];
}
EOF
items 10 "int a = 10, *b = &a; return *b;"
items 10 "int a = 10, b[3] = {1, 2, a}; return b[2];"

# implicit conversions
items 10 "long a = 10; return a;"
items 10 "int a = (char)10; return a;"
expr 4 "sizeof 4"
try_ 10 <<EOF
void* malloc(int);
int main() {
  char* b = malloc(2);
  b[1] = 10;
  return b[1];
}
EOF
items 1 "_Bool b = 10; return b;"
items 0 "_Bool b = 0; return b;"
items 1 "_Bool b = -1; return b;"
items 0 "_Bool b = 1; return !b;"
items 1 "_Bool b = 0; return !b;"
items 1 "int a, *p = &a; _Bool b = p; return b;"
items 0 "int *a = 0; _Bool b = a; return b; "
try_ 1 <<EOF
_Bool is_zero_or_seven(int num) {
  return num == 0 || num == 7;
}

int main() {
  return is_zero_or_seven(1) || (is_zero_or_seven(0) && is_zero_or_seven(7));
}
EOF

# comments
items 10 "int /* I am a comment */ a = /*hello!*/ 10; return /*wowo*/ a;"
try_ 42 <<EOF
// OMG I AM A COMMENT

// weeeeeeeeee
int main (/* no parameters. */) {
  // line comment is cool, huh?
  // /* super-cool c compiler */ yeah
  return /* this is a magic number -> */ 42;
}

// this is also a comment
EOF

# struct
items 4 "struct {int a;} x; return sizeof(x);"
items 12 "struct {char a; int b;} x; x.a=10; x.b = 2; return x.a + x.b;"
items 22 "struct {long a; int b;} x; struct {long a; int b;} *p = &x; x.a = 10; p->b = 12; return p->a + p->b;"
items 22 "struct tag {long a; int b;} x; struct tag *p = &x; x.a = 10; p->b = 12; return p->a + p->b;"
items 8 "struct { struct { int b; int c[5];} a[2]; } x; x.a[0].b = 3; x.a[0].c[1] = 5; return x.a[0].b + x.a[0].c[1];"
items 48 "struct { long a[2]; int b[8]; } x; return sizeof(x);"
items 96 "struct { long a[2], b[5], *c; struct { int d; int e; } f[4]; } x; return sizeof(x);"
try_ 10 <<EOF
struct A {
  struct A* ptr;
  int i;
};

void* calloc(int, int);

int main(int argc, char** argv) {
  struct A *a = calloc(1, sizeof(struct A));
  a->i = 10;
  a->ptr = a;
  return a->ptr->ptr->ptr->ptr->ptr->i;
}
EOF
try_ 25 <<EOF
struct S {
  int a;
  struct {
    long field1;
    char field2;
  } b;
};

int main() {
  struct S s1;
  s1.a = 10;
  s1.b.field1 = 5;
  s1.b.field2 = 20;
  struct S s2;
  s2 = s1;
  return s2.b.field1 + s2.b.field2;
}
EOF
try_ 1 <<EOF
void* malloc(int);

struct S {
  int a;
  long b;
};

struct S* new_S(int i, long l) {
  struct S* s = malloc(sizeof(struct S));
  s->a = i;
  s->b = l;
  return s;
}

int main() {
  struct S* s1 = new_S(1, 2);
  struct S* s2 = new_S(3, 4);
  *s2 = *s1;
  s1->a = 10;
  return s2->a;
}
EOF

# typedef
items 4 "typedef int yeah; yeah x = 4; return x;"
items 0 "typedef struct data data; data* p = 0; return (int)p;"
try_ 42 <<EOF
typedef struct S S;

int get_S(S*);
void incr_S(S*);
S* new_S(int);
void release_S(S*);

int main() {
  S* s = new_S(40);
  incr_S(s);
  incr_S(s);
  int i = get_S(s);
  release_S(s);
  return i;
}

void free(void*);
void* calloc(int, int);

struct S {
  int value;
};

int get_S(S* s) {
  return s->value;
}

void incr_S(S* s) {
  s->value++;
}

S* new_S(int i) {
  S* s = calloc(1, sizeof(S));
  s->value = i;
  return s;
}

void release_S(S* s) {
 free(s);
}
EOF
try_ 10 <<EOF
typedef int A;

typedef struct {
  A yeah;
} T;

typedef T *Tptr;

int main() {
  T t;
  Tptr x = &t;
  x->yeah = 10;
  return t.yeah;
}
EOF

# enum
items 0 "enum {E1, E2, E3} e = E1; return e;"
items 2 "enum {E1, E2, E3} e = E3; return e;"
items 0 "enum { E }; return E;"
items 9 "enum tag { E1 = 1, E2 = 8 }; enum tag v = E2; return v + E1;"
try_ 11 <<EOF
typedef enum {
  SUPER_MY_ENUM,
  AWESOME_MY_ENUM = 10,
  FOO,
} E;

int main() {
    E e = FOO;
    return e;
}
EOF

# constant expressions
items 24 "int a[sizeof(int) + 2]; return sizeof(a);"
try_ 5 <<EOF
char *strings[] = {"str1", "str2", "str3", "str4", "str5"};
unsigned long length = sizeof(strings) / sizeof(*strings);

int main(int argc, char** argv) {
  return length;
}
EOF
try_ 3 <<EOF
enum E {
  E1 = 0,
  E2 = 2,
  E3 = 4
};

int a[E3 - 1];

int main(int argc, char** argv) {
  return sizeof(a) / sizeof(int);
}
EOF

echo OK
