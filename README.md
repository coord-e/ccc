# ccc

[![Build Status](https://travis-ci.com/coord-e/ccc.svg?branch=develop)](https://travis-ci.com/coord-e/ccc)

a work-in-progress compiler for a tiny subset of C language.

ccc is intended to be an optimizing compiler that produces code which is faster than gcc -O1 does, and compiles faster than that.

## Roadmap

- [ ] support C11 features
  - [x] arithmetic operations
  - [x] pointer arithmetic
  - [x] arrays
  - [x] type checker
    - [ ] support type qualifiers
  - [x] implicit type conversions
  - [ ] initializers
    - [x] scalar
    - [x] array
    - [ ] struct
    - [ ] designated initializers
  - [ ] a copy of struct
    - [x] local assignment
    - [ ] argument
    - [ ] return value
  - [ ] preprocessor
  - [ ] self-hosting
  - [ ] complex declarators
  - [ ] alignment specifiers
  - [ ] float-point values
- [ ] optimizations
  - [x] linear scan register allocation
  - [x] naive mem2reg
  - [ ] constant folding
  - [ ] copy propagation
  - [ ] dead code elimination
  - [ ] tail call optimization
  - [ ] loop unwinding
- [ ] misc
  - [ ] improved error messages
  - [ ] better support of debuggers
  - [ ] support targets other than x86_64

## Development

To build ccc, run:

```shell
make
```

You'll see a compiled binary under `./build/`.

You can add `DEBUG=0` to get an optimized build of `ccc`.

To run tests, use `test` target:

```shell
make test
```
