#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "codegen.h"
#include "error.h"

void emit_label(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  fprintf(stdout, ":\n");
}

void emit(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stdout, "  ");
  vfprintf(stdout, fmt, ap);
  fprintf(stdout, "\n");
}

void codegen_expr(Node* node);

void codegen_binop(Node* node) {
  codegen_expr(node->lhs);
  codegen_expr(node->rhs);
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
      CCC_UNREACHABLE;
  }
  emit("push rax");
}

void codegen_expr(Node* node) {
  switch(node->kind) {
    case ND_NUM:
      emit("push %d", node->num);
      return;
    case ND_BINOP:
      codegen_binop(node);
      return;
    default:
      CCC_UNREACHABLE;
  }
}

void codegen(Node* node) {
  emit(".intel_syntax noprefix");
  emit(".global main");
  emit_label("main");
  codegen_expr(node);
  emit("pop rax");
  emit("ret");
}
