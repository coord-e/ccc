# ccc

[![Build Status](https://travis-ci.com/coord-e/ccc.svg?branch=develop)](https://travis-ci.com/coord-e/ccc)

a work-in-progress compiler for a tiny subset of C language.

ccc is intended to be an optimizing compiler that produces code which is faster than gcc -O1 does, and compiles faster than that.

# Roadmap

- [ ] support C11 features
  - [-] arithmetic operations
  - [-] pointer arithmetic
  - [-] arrays
  - [-] type checker
    - [ ] support type qualifiers
  - [-] implicit type conversions
  - [ ] initializers
    - [-] scalar
    - [-] array
    - [ ] struct
    - [ ] designated initializers
  - [ ] a copy of struct
    - [-] local assignment
    - [ ] argument
    - [ ] return value
  - [ ] preprocessor
  - [ ] self-hosting
  - [ ] complex declarators
  - [ ] alignment specifiers
  - [ ] float-point values
- [ ] optimizations
  - [-] linear scan register allocation
  - [-] naive mem2reg
  - [ ] constant folding
  - [ ] copy propagation
  - [ ] dead code elimination
  - [ ] tail call optimization
  - [ ] loop unwinding
- [ ] misc
  - [ ] improved error messages
  - [ ] better support of debuggers
  - [ ] support targets other than x86_64
