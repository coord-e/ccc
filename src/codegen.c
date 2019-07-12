#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "codegen.h"
#include "error.h"

static const char* regs[] = {"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rbx"};

// declared as an extern variable in codegen.h
size_t num_regs = sizeof(regs) / sizeof(*regs);

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

const char* reg_of(Reg r) {
  return regs[r.real];
}

const char* nth_reg_of(unsigned i, RegVec* rs) {
  return reg_of(get_RegVec(rs, i));
}

void codegen_binop(FILE* p, IRInst* inst);

void codegen_insts(FILE* p, IRInstList* insts) {
  if (is_nil_IRInstList(insts)) {
    return;
  }

  IRInst* h = head_IRInstList(insts);
  switch(h->kind) {
    case IR_IMM:
      emit(p, "mov %s, %d", reg_of(h->rd), h->imm);
      break;
    case IR_MOV:
      emit(p, "mov %s, %s", reg_of(h->rd), nth_reg_of(0, h->ras));
      break;
    case IR_RET:
      emit(p, "mov rax, %s", nth_reg_of(0, h->ras));
      emit(p, "mov rsp, rbp");
      emit(p, "pop rbp");
      emit(p, "ret");
      break;
    case IR_BIN:
      codegen_binop(p, h);
      break;
    case IR_LOAD:
      emit(p, "mov %s, [rbp - %d]", reg_of(h->rd), 8 * h->stack_idx + 8);
      break;
    case IR_STORE:
      emit(p, "mov [rbp - %d], %s", 8 * h->stack_idx + 8, nth_reg_of(0, h->ras));
      break;
    case IR_SUBS:
      emit(p, "sub rsp, %d", 8 * h->stack_idx + 8);
      break;
    default:
      CCC_UNREACHABLE;
  }

  codegen_insts(p, tail_IRInstList(insts));
}

void codegen_binop(FILE* p, IRInst* inst) {
  const char* rd = reg_of(inst->rd);
  const char* lhs = nth_reg_of(0, inst->ras);
  const char* rhs = nth_reg_of(1, inst->ras);
  emit(p, "mov %s, %s", rd, lhs);
  switch(inst->binop) {
    case BINOP_ADD:
      emit(p, "add %s, %s", rd, rhs);
      return;
    case BINOP_SUB:
      emit(p, "sub %s, %s", rd, rhs);
      return;
    case BINOP_MUL:
      emit(p, "imul %s, %s", rd, rhs);
      return;
    case BINOP_DIV:
      emit(p, "mov rax, %s", rd);
      emit(p, "cqo");
      emit(p, "idiv %s", rhs);
      emit(p, "mov %s, rax", rd);
      return;
    default:
      CCC_UNREACHABLE;
  }
}

void codegen(FILE* p, IR* ir) {
  emit(p, ".intel_syntax noprefix");
  emit(p, ".global main");
  emit_label(p, "main");
  emit(p, "push rbp");
  emit(p, "mov rbp, rsp");
  codegen_insts(p, ir->insts);
}
