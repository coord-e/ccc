#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "error.h"

// clang-format off
static const char* regs8[]  = {"r12b", "r13b", "r14b", "r15b", "bl"};
static const char* regs16[] = {"r12w", "r13w", "r14w", "r15w", "bxj"};
static const char* regs32[] = {"r12d", "r13d", "r14d", "r15d", "ebx"};
static const char* regs64[] = {"r12",  "r13",  "r14",  "r15",  "rbx"};

static const char* arg_regs8[]  = {"dil", "sil", "dl",  "cl",  "r8b", "r9b"};
static const char* arg_regs16[] = {"di",  "si",  "dx",  "cx",  "r8w", "r9w"};
static const char* arg_regs32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static const char* arg_regs64[] = {"rdi", "rsi", "rdx", "rcx", "r8",  "r9"};
// clang-format on

// declared as an extern variable in codegen.h
size_t num_regs = sizeof(regs64) / sizeof(*regs64);

static const char* size_spec(DataSize s) {
  switch (s) {
    case SIZE_BYTE:
      return "BYTE PTR";
    case SIZE_WORD:
      return "WORD PTR";
    case SIZE_DWORD:
      return "DWORD PTR";
    case SIZE_QWORD:
      return "QWORD PTR";
    default:
      CCC_UNREACHABLE;
  }
}

static const char* rax_of_size(DataSize s) {
  switch (s) {
    case SIZE_BYTE:
      return "al";
    case SIZE_WORD:
      return "ax";
    case SIZE_DWORD:
      return "eax";
    case SIZE_QWORD:
      return "rax";
    default:
      CCC_UNREACHABLE;
  }
}

static const char* rcx_of_size(DataSize s) {
  switch (s) {
    case SIZE_BYTE:
      return "cl";
    case SIZE_WORD:
      return "cx";
    case SIZE_DWORD:
      return "ecx";
    case SIZE_QWORD:
      return "rcx";
    default:
      CCC_UNREACHABLE;
  }
}

static const char* rdx_of_size(DataSize s) {
  switch (s) {
    case SIZE_BYTE:
      return "dl";
    case SIZE_WORD:
      return "dx";
    case SIZE_DWORD:
      return "edx";
    case SIZE_QWORD:
      return "rdx";
    default:
      CCC_UNREACHABLE;
  }
}

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

static const char* reg_name(unsigned id, DataSize size) {
  switch (size) {
    case SIZE_BYTE:
      return regs8[id];
    case SIZE_WORD:
      return regs16[id];
    case SIZE_DWORD:
      return regs32[id];
    case SIZE_QWORD:
      return regs64[id];
    default:
      CCC_UNREACHABLE;
  }
}

static const char* reg_of(Reg r) {
  return reg_name(r.real, r.size);
}

static const char* nth_reg_of(unsigned i, RegVec* rs) {
  return reg_of(get_RegVec(rs, i));
}

const size_t max_args = sizeof(arg_regs64) / sizeof(*arg_regs64);
static const char* nth_arg(unsigned idx, DataSize s) {
  if (idx >= max_args) {
    error("unsupported number of arguments/parameters. (%d)", idx);
  }

  switch (s) {
    case SIZE_BYTE:
      return arg_regs8[idx];
    case SIZE_WORD:
      return arg_regs16[idx];
    case SIZE_DWORD:
      return arg_regs32[idx];
    case SIZE_QWORD:
      return arg_regs64[idx];
    default:
      CCC_UNREACHABLE;
  }
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
  emit(p, "sub rsp, %d", f->stack_count);
  for (unsigned i = 0; i < length_BitSet(f->used_regs); i++) {
    if (get_BitSet(f->used_regs, i)) {
      emit(p, "push %s", regs64[i]);
    }
  }
  emit_(p, "jmp ");
  id_label_name(p, f->entry->global_id);
  fprintf(p, "\n");
}

static void emit_epilogue(FILE* p, Function* f) {
  for (unsigned i = length_BitSet(f->used_regs); i > 0; i--) {
    if (get_BitSet(f->used_regs, i - 1)) {
      emit(p, "pop %s", regs64[i - 1]);
    }
  }
  emit(p, "mov rsp, rbp");
  emit(p, "pop rbp");
}

static void codegen_binop(FILE* p, IRInst* inst);
static void codegen_unaop(FILE* p, IRInst* inst);

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
      emit(p, "mov %s, %s", reg_of(h->rd), nth_arg(h->argument_idx, h->rd.size));
      break;
    case IR_MOV:
      emit(p, "mov %s, %s", reg_of(h->rd), nth_reg_of(0, h->ras));
      break;
    case IR_TRUNC: {
      Reg opr = get_RegVec(h->ras, 0);
      emit(p, "mov %s, %s", reg_of(h->rd), reg_name(opr.real, h->rd.size));
      break;
    }
    case IR_SEXT:
      emit(p, "movsx %s, %s", reg_of(h->rd), nth_reg_of(0, h->ras));
      break;
    case IR_RET:
      if (length_RegVec(h->ras) != 0) {
        assert(length_RegVec(h->ras) == 1);
        Reg r = get_RegVec(h->ras, 0);
        emit(p, "mov %s, %s", rax_of_size(r.size), reg_of(r));
      }
      emit_epilogue(p, f);
      emit(p, "ret");
      break;
    case IR_BIN:
      codegen_binop(p, h);
      break;
    case IR_UNA:
      codegen_unaop(p, h);
      break;
    case IR_STACK_ADDR:
      emit(p, "lea %s, [rbp - %d]", reg_of(h->rd), h->stack_idx);
      break;
    case IR_STACK_LOAD:
      emit(p, "mov %s, %s [rbp - %d]", reg_of(h->rd), size_spec(h->data_size), h->stack_idx);
      break;
    case IR_STACK_STORE:
      emit(p, "mov %s [rbp - %d], %s", size_spec(h->data_size), h->stack_idx,
           nth_reg_of(0, h->ras));
      break;
    case IR_LOAD:
      emit(p, "mov %s, %s [%s]", reg_of(h->rd), size_spec(h->data_size), nth_reg_of(0, h->ras));
      break;
    case IR_STORE:
      emit(p, "mov %s [%s], %s", size_spec(h->data_size), nth_reg_of(0, h->ras),
           nth_reg_of(1, h->ras));
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
        emit(p, "mov %s, %s", nth_arg(i - 1, r.size), reg_of(r));
      }
      emit(p, "call %s", nth_reg_of(0, h->ras));
      emit(p, "mov %s, %s", reg_of(h->rd), rax_of_size(h->rd.size));
      break;
    default:
      CCC_UNREACHABLE;
  }

  codegen_insts(p, f, tail_IRInstList(insts));
}

static void codegen_cmp(FILE* p, const char* s, Reg rd, Reg rhs) {
  emit(p, "cmp %s, %s", reg_of(rd), reg_of(rhs));
  emit(p, "set%s %s", s, regs8[rd.real]);
  emit(p, "movzb %s, %s", reg_of(rd), regs8[rd.real]);
}

static void codegen_unaop(FILE* p, IRInst* inst) {
  Reg rd  = inst->rd;
  Reg opr = get_RegVec(inst->ras, 0);

  if (rd.real != opr.real) {
    emit(p, "mov %s, %s", reg_of(rd), reg_of(opr));
  }

  switch (inst->unaop) {
    case UNAOP_POSITIVE:
      return;
    case UNAOP_INTEGER_NEG:
      emit(p, "neg %s", reg_of(rd));
      return;
    case UNAOP_BITWISE_NEG:
      emit(p, "not %s", reg_of(rd));
      return;
    default:
      CCC_UNREACHABLE;
  }
}

static void codegen_binop(FILE* p, IRInst* inst) {
  Reg rd  = inst->rd;
  Reg lhs = get_RegVec(inst->ras, 0);
  Reg rhs = get_RegVec(inst->ras, 1);

  // A = B op A instruction can't be emitted
  assert(rd.real != rhs.real);

  if (rd.real != lhs.real) {
    emit(p, "mov %s, %s", reg_of(rd), reg_of(lhs));
  }

  switch (inst->binop) {
    case BINOP_ADD:
      emit(p, "add %s, %s", reg_of(rd), reg_of(rhs));
      return;
    case BINOP_SUB:
      emit(p, "sub %s, %s", reg_of(rd), reg_of(rhs));
      return;
    case BINOP_MUL:
      emit(p, "imul %s, %s", reg_of(rd), reg_of(rhs));
      return;
    case BINOP_DIV:
      emit(p, "mov %s, %s", rax_of_size(rd.size), reg_of(rd));
      emit(p, "cqo");
      emit(p, "idiv %s", reg_of(rhs));
      emit(p, "mov %s, %s", reg_of(rd), rax_of_size(rd.size));
      return;
    case BINOP_REM:
      emit(p, "mov %s, %s", rax_of_size(rd.size), reg_of(rd));
      emit(p, "cqo");
      emit(p, "idiv %s", reg_of(rhs));
      emit(p, "mov %s, %s", reg_of(rd), rdx_of_size(rd.size));
      return;
    case BINOP_EQ:
      codegen_cmp(p, "e", rd, rhs);
      return;
    case BINOP_NE:
      codegen_cmp(p, "ne", rd, rhs);
      return;
    case BINOP_GT:
      codegen_cmp(p, "g", rd, rhs);
      return;
    case BINOP_GE:
      codegen_cmp(p, "ge", rd, rhs);
      return;
    case BINOP_LT:
      codegen_cmp(p, "l", rd, rhs);
      return;
    case BINOP_LE:
      codegen_cmp(p, "le", rd, rhs);
      return;
    case BINOP_SHIFT_RIGHT:
      emit(p, "mov %s, %s", rcx_of_size(rhs.size), reg_of(rhs));
      // TODO: Consider signedness
      emit(p, "sar %s, cl", reg_of(rd));
      return;
    case BINOP_SHIFT_LEFT:
      emit(p, "mov %s, %s", rcx_of_size(rhs.size), reg_of(rhs));
      emit(p, "shl %s, cl", reg_of(rd));
      return;
    case BINOP_AND:
      emit(p, "and %s, %s", reg_of(rd), reg_of(rhs));
      return;
    case BINOP_XOR:
      emit(p, "xor %s, %s", reg_of(rd), reg_of(rhs));
      return;
    case BINOP_OR:
      emit(p, "or %s, %s", reg_of(rd), reg_of(rhs));
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

static void codegen_global_expr(FILE* p, GlobalExpr* expr) {
  switch (expr->kind) {
    case GE_ADD:
      emit(p, ".quad %s + %ld", expr->lhs, expr->rhs);
      break;
    case GE_SUB:
      emit(p, ".quad %s - %ld", expr->lhs, expr->rhs);
      break;
    case GE_NUM:
      switch (expr->size) {
        case SIZE_BYTE:
          emit(p, ".byte %ld", expr->num);
          break;
        case SIZE_WORD:
          emit(p, ".word %ld", expr->num);
          break;
        case SIZE_DWORD:
          emit(p, ".int %ld", expr->num);
          break;
        case SIZE_QWORD:
          emit(p, ".quad %ld", expr->num);
          break;
        default:
          CCC_UNREACHABLE;
      }
      break;
    case GE_STRING:
      emit(p, ".string \"%s\"", expr->string);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static void codegen_global_init(FILE* p, GlobalInitializer* init) {
  for (unsigned i = 0; i < length_GlobalInitializer(init); i++) {
    codegen_global_expr(p, get_GlobalInitializer(init, i));
  }
}

static void codegen_globals(FILE* p, GlobalVarVec* vs) {
  for (unsigned i = 0; i < length_GlobalVarVec(vs); i++) {
    GlobalVar* v = get_GlobalVarVec(vs, i);
    emit_label(p, v->name);
    codegen_global_init(p, v->init);
  }
}

void codegen(FILE* p, IR* ir) {
  emit(p, ".intel_syntax noprefix");
  emit(p, ".text");
  codegen_globals(p, ir->globals);
  codegen_functions(p, ir->functions);
}
