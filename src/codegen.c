#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "error.h"

// clang-format off
static const char* regs[]  = {"r12",  "r13",  "r14",  "r15",  "rbx"};
static const char* regs8[] = {"r12b", "r13b", "r14b", "r15b", "bl"};

static const char* nth_arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
// clang-format on

// declared as an extern variable in codegen.h
size_t num_regs = sizeof(regs) / sizeof(*regs);

static void emit_label(FILE* p, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(p, fmt, ap);
  fprintf(p, ":\n");
}

static void emit_(FILE* p, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(p, "  ");
  vfprintf(p, fmt, ap);
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

const size_t max_args = sizeof(nth_arg_regs) / sizeof(*nth_arg_regs);
static const char* nth_arg(unsigned idx) {
  if (idx >= max_args) {
    error("unsupported number of arguments/parameters. (%d)", idx);
  }

  return nth_arg_regs[idx];
}

static void id_label_name(FILE* p, unsigned id) {
  fprintf(p, "_ccc_%d", id);
}

static void emit_id_label(FILE* p, unsigned id) {
  id_label_name(p, id);
  fprintf(p, ":\n");
}

static void emit_prologue(FILE* p, Function* f) {
  emit(p, "push rbp");
  emit(p, "mov rbp, rsp");
  emit(p, "sub rsp, %d", f->stack_count + 8);
  for (unsigned i = 0; i < length_BitSet(f->used_regs); i++) {
    if (get_BitSet(f->used_regs, i)) {
      emit(p, "push %s", regs[i]);
    }
  }
  emit_(p, "jmp ");
  id_label_name(p, f->entry->global_id);
  fprintf(p, "\n");
}

static void emit_epilogue(FILE* p, Function* f) {
  for (unsigned i = length_BitSet(f->used_regs); i > 0; i--) {
    if (get_BitSet(f->used_regs, i - 1)) {
      emit(p, "pop %s", regs[i - 1]);
    }
  }
  emit(p, "mov rsp, rbp");
  emit(p, "pop rbp");
}

static void codegen_binop(FILE* p, IRInst* inst);

static void codegen_insts(FILE* p, Function* f, IRInstList* insts) {
  if (is_nil_IRInstList(insts)) {
    return;
  }

  IRInst* h = head_IRInstList(insts);
  switch (h->kind) {
    case IR_IMM:
      emit(p, "mov %s, %d", reg_of(h->rd), h->imm);
      break;
    case IR_ARG:
      emit(p, "mov %s, %s", reg_of(h->rd), nth_arg(h->argument_idx));
      break;
    case IR_MOV:
      emit(p, "mov %s, %s", reg_of(h->rd), nth_reg_of(0, h->ras));
      break;
    case IR_RET:
      if (length_RegVec(h->ras) != 0) {
        assert(length_RegVec(h->ras) == 1);
        emit(p, "mov rax, %s", nth_reg_of(0, h->ras));
      }
      emit_epilogue(p, f);
      emit(p, "ret");
      break;
    case IR_BIN:
      codegen_binop(p, h);
      break;
    case IR_STACK_ADDR:
      emit(p, "lea %s, [rbp - %d]", reg_of(h->rd), h->stack_idx + 8);
      break;
    case IR_STACK_LOAD:
      emit(p, "mov %s, [rbp - %d]", reg_of(h->rd), h->stack_idx + 8);
      break;
    case IR_STACK_STORE:
      emit(p, "mov [rbp - %d], %s", h->stack_idx + 8, nth_reg_of(0, h->ras));
      break;
    case IR_LOAD:
      emit(p, "mov %s, [%s]", reg_of(h->rd), nth_reg_of(0, h->ras));
      break;
    case IR_STORE:
      emit(p, "mov [%s], %s", nth_reg_of(0, h->ras), nth_reg_of(1, h->ras));
      break;
    case IR_LABEL:
      emit_id_label(p, h->label->global_id);
      break;
    case IR_JUMP:
      emit_(p, "jmp ");
      id_label_name(p, h->jump->global_id);
      fprintf(p, "\n");
      break;
    case IR_BR:
      emit(p, "cmp %s, 0", nth_reg_of(0, h->ras));
      emit_(p, "je ");
      id_label_name(p, h->else_->global_id);
      fprintf(p, "\n");
      emit_(p, "jmp ");
      id_label_name(p, h->then_->global_id);
      fprintf(p, "\n");
      break;
    case IR_GLOBAL_ADDR:
      emit(p, "lea %s, %s@PLT[rip]", reg_of(h->rd), h->global_name);
      break;
    case IR_CALL:
      for (unsigned i = 1; i < length_RegVec(h->ras); i++) {
        Reg r = get_RegVec(h->ras, i);
        emit(p, "mov %s, %s", nth_arg(i - 1), reg_of(r));
      }
      emit(p, "call %s", nth_reg_of(0, h->ras));
      emit(p, "mov %s, rax", reg_of(h->rd));
      break;
    default:
      CCC_UNREACHABLE;
  }

  codegen_insts(p, f, tail_IRInstList(insts));
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

static void codegen_blocks(FILE* p, Function* f, BBList* l) {
  if (is_nil_BBList(l)) {
    return;
  }

  BasicBlock* b = head_BBList(l);
  if (!b->dead) {
    codegen_insts(p, f, b->insts);
  }

  codegen_blocks(p, f, tail_BBList(l));
}

static void codegen_functions(FILE* p, FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  Function* f = head_FunctionList(l);
  emit(p, ".global %s", f->name);
  emit_label(p, f->name);
  emit_prologue(p, f);
  codegen_blocks(p, f, f->blocks);

  codegen_functions(p, tail_FunctionList(l));
}

void codegen(FILE* p, IR* ir) {
  emit(p, ".intel_syntax noprefix");
  codegen_functions(p, ir->functions);
}
