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
    case IR_RET:
      emit(p, "mov %%rax, %s", reg_of(h->ra));
      emit(p, "ret");
      break;
    case IR_BIN:
      codegen_binop(p, h);
      break;
    case IR_LOAD:
      emit(p, "mov %s, %d(%%rbp)", reg_of(h->rd), -8 * h->stack_idx);
      break;
    case IR_STORE:
      emit(p, "mov %d(%%rbp), %s", -8 * h->stack_idx, reg_of(h->ra));
      break;
    case IR_SUBS:
      emit(p, "sub %%rsp, %d", h->stack_idx);
      break;
    default:
      CCC_UNREACHABLE;
  }

  codegen_insts(p, tail_IRInstList(insts));
}

void codegen_binop(FILE* p, IRInst* inst) {
  const char* rd = reg_of(inst->rd);
  const char* ra = reg_of(inst->ra);
  switch(inst->binop) {
    case BINOP_ADD:
      emit(p, "add %s, %s", rd, ra);
      return;
    case BINOP_SUB:
      emit(p, "sub %s, %s", rd, ra);
      return;
    case BINOP_MUL:
      emit(p, "imul %s, %s", rd, ra);
      return;
    case BINOP_DIV:
      emit(p, "mov %%rax, %s", rd);
      emit(p, "cqo");
      emit(p, "idiv %s", ra);
      emit(p, "mov %s, %%rax", rd);
      return;
    default:
      CCC_UNREACHABLE;
  }
}

void codegen(FILE* p, IR* ir) {
  emit(p, ".intel_syntax noprefix");
  emit(p, ".global main");
  emit_label(p, "main");
  codegen_insts(p, ir->insts);
}
