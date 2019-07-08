#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "codegen.h"
#include "error.h"

void emit_label(FILE* p, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(p, fmt, ap);
  fprintf(p, ":\n");
}

void emit(FILE* p, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(p, "  ");
  vfprintf(p, fmt, ap);
  fprintf(p, "\n");
}

void codegen_expr(FILE*, Node* node);

void codegen_binop(FILE* p, Node* node) {
  codegen_expr(p, node->lhs);
  codegen_expr(p, node->rhs);
  emit(p, "pop rdi");
  emit(p, "pop rax");
  switch(node->binop) {
    case BINOP_ADD:
      emit(p, "add rax, rdi");
      break;
    case BINOP_SUB:
      emit(p, "sub rax, rdi");
      break;
    case BINOP_MUL:
      emit(p, "imul rax, rdi");
      break;
    case BINOP_DIV:
      emit(p, "cqo");
      emit(p, "idiv rdi");
      break;
    default:
      CCC_UNREACHABLE;
  }
  emit(p, "push rax");
}

void codegen_expr(FILE* p, Node* node) {
  switch(node->kind) {
    case ND_NUM:
      emit(p, "push %d", node->num);
      return;
    case ND_BINOP:
      codegen_binop(p, node);
      return;
    default:
      CCC_UNREACHABLE;
  }
}

void codegen(FILE* p, Node* node) {
  emit(p, ".intel_syntax noprefix");
  emit(p, ".global main");
  emit_label(p, "main");
  codegen_expr(p, node);
  emit(p, "pop rax");
  emit(p, "ret");
}
