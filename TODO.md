# Features

- copies of struct objects
- type check of constness and other type qualifiers
- varadic functions
  - declaration
  - definition
  - stdarg functions ?
- exact support of `static` / `extern`
- function specifiers (`inline`, `_Noreturn`)
- preprocessor
  - `#include`
  - `#define`

# Refactoring

- refine data structures
  - use doubly-linked list
  - distinguish between deep and shallow copy
  - place shallow copies
  - place dummy funcs in util
  - place UI* structures as common defs
- refine reg_alloc
  - add indexed linked list
  - abstract the alloc/free in reg_alloc
- use pointers
  - Reg
  - Token
  - remove unneeded zero init with calloc
- naming convention
  - name_Type
  - name_target ?
  - is_ ?
  - to_ ?
  - as_ ? 
- abort on error in DEBUG mode
