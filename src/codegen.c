#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "error.h"

static const char* regs[]  = {"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rbx"};
static const char* regs8[] = {"r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b", "bl"};

// declared as an extern variable in codegen.h
size_t num_regs = sizeof(regs) / sizeof(*regs);

static void emit_label(FILE* p, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(p, fmt, ap);
  fprintf(p, ":\n");
}

static void emit(FILE* p, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(p, "  ");
  vfprintf(p, fmt, ap);
  fprintf(p, "\n");
}

static const char* reg_of(Reg r) {
  return regs[r.real];
}

static const char* nth_reg_of(unsigned i, RegVec* rs) {
  return reg_of(get_RegVec(rs, i));
}

static void codegen_binop(FILE* p, IRInst* inst);

static void codegen_insts(FILE* p, IRInstList* insts) {
  if (is_nil_IRInstList(insts)) {
    return;
  }

  IRInst* h = head_IRInstList(insts);
  switch (h->kind) {
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
      emit(p, "sub rsp, %d", 8 * h->stack_idx);
      break;
    default:
      CCC_UNREACHABLE;
  }

  codegen_insts(p, tail_IRInstList(insts));
}

static void codegen_cmp(FILE* p, const char* s, unsigned rd_id, unsigned rhs_id) {
  emit(p, "cmp %s, %s", regs[rd_id], regs[rhs_id]);
  emit(p, "set%s %s", s, regs8[rd_id]);
  emit(p, "movzb %s, %s", regs[rd_id], regs8[rd_id]);
}

static void codegen_binop(FILE* p, IRInst* inst) {
  // extract ids for comparison
  unsigned rd_id  = inst->rd.real;
  unsigned lhs_id = get_RegVec(inst->ras, 0).real;
  unsigned rhs_id = get_RegVec(inst->ras, 1).real;
  // A = B op A instruction can't be emitted
  assert(rd_id != rhs_id);

  const char* rd  = reg_of(inst->rd);
  const char* lhs = nth_reg_of(0, inst->ras);
  const char* rhs = nth_reg_of(1, inst->ras);

  if (rd_id != lhs_id) {
    emit(p, "mov %s, %s", rd, lhs);
  }

  switch (inst->binop) {
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
    case BINOP_EQ:
      codegen_cmp(p, "e", rd_id, rhs_id);
      return;
    case BINOP_NE:
      codegen_cmp(p, "ne", rd_id, rhs_id);
      return;
    case BINOP_GT:
      codegen_cmp(p, "g", rd_id, rhs_id);
      return;
    case BINOP_GE:
      codegen_cmp(p, "ge", rd_id, rhs_id);
      return;
    case BINOP_LT:
      codegen_cmp(p, "l", rd_id, rhs_id);
      return;
    case BINOP_LE:
      codegen_cmp(p, "le", rd_id, rhs_id);
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
