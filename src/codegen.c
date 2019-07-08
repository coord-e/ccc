#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "codegen.h"
#include "error.h"

void emit(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "  ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void codegen_binop(Node* node) {
  codegen(node->lhs);
  codegen(node->rhs);
  emit("pop rdi");
  emit("pop rax");
  switch(node->binop) {
    case BINOP_ADD:
      emit("add rax, rdi");
      break;
    case BINOP_SUB:
      emit("sub rax, rdi");
      break;
    case BINOP_MUL:
      emit("imul rax, rdi");
      break;
    case BINOP_DIV:
      emit("cqo");
      emit("idiv rdi");
      break;
    default:
      error("unreachable (codegen_binop)");
  }
  emit("push rax");
}

void codegen(Node* node) {
  switch(node->kind) {
    case ND_NUM:
      emit("push %d", node->num);
      return;
    case ND_BINOP:
      codegen_binop(node);
      return;
    default:
      error("unreachable (codegen)");
  }
  emit("pop rax");
}
