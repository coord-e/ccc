# Features

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
  - conditional directives such as `#if`

# Refactoring

- refine data structures
  - use doubly-linked list
  - distinguish between deep and shallow copy
  - place shallow copies
  - place UI* structures as common defs
  - release_Type: ignore NULL
  - new_Type: zero initialized and only take common fields
  - copy_Type: deep copy
  - shallow_copy_Type: shallow copy
- separate gen_ir from ir.c
  - IRBuilder
- use finish_Env instead of release_Env as much as possible
- naming convention
  - name_Type
  - name_target ?
  - new_?
  - build_?
  - is_ ?
  - to_ ?
  - as_ ? 
- abort on error in DEBUG mode

// perform some allocation or creation
in:consume out:new   new_
in:view    out:new   make_
in:consume out:view  create_
in:view    out:view  _
complex:             build_
