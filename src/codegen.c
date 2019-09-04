#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "arch.h"
#include "codegen.h"
#include "error.h"

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

static const char* reg_of(Reg* r) {
  return reg_name(r->real, r->size);
}

static const char* nth_reg_of(unsigned i, RegVec* rs) {
  return reg_of(get_RegVec(rs, i));
}

static void id_label_name(FILE* p, unsigned id) {
  fprintf(p, "_ccc_%d", id);
}

static void emit_id_label(FILE* p, unsigned id) {
  id_label_name(p, id);
  fprintf(p, ":\n");
}

static void emit_save_regs(FILE* p, BitSet* bs, bool scratch_filter_switch) {
  for (unsigned i = 0; i < length_BitSet(bs); i++) {
    if (!get_BitSet(bs, i)) {
      continue;
    }
    if (scratch_filter_switch == is_scratch[i]) {
      emit(p, "push %s", regs64[i]);
    }
  }
}

static void emit_restore_regs(FILE* p, BitSet* bs, bool scratch_filter_switch) {
  for (unsigned ti = length_BitSet(bs); ti > 0; ti--) {
    unsigned i = ti - 1;
    if (!get_BitSet(bs, i)) {
      continue;
    }
    if (scratch_filter_switch == is_scratch[i]) {
      emit(p, "pop %s", regs64[i]);
    }
  }
}

static void emit_prologue(FILE* p, Function* f) {
  emit(p, "push rbp");
  emit(p, "mov rbp, rsp");
  emit(p, "sub rsp, %d", f->stack_count);
  emit_save_regs(p, f->used_regs, false);
  emit_(p, "jmp ");
  id_label_name(p, f->entry->global_id);
  fprintf(p, "\n");
}

static void emit_epilogue(FILE* p, Function* f) {
  emit_restore_regs(p, f->used_regs, false);
  emit(p, "mov rsp, rbp");
  emit(p, "pop rbp");
}

static void codegen_binop(FILE* p, IRInst* inst);
static void codegen_unaop(FILE* p, IRInst* inst);

static void codegen_insts(FILE* p, Function* f, BasicBlock* bb, IRInstList* insts) {
  if (is_nil_IRInstList(insts)) {
    return;
  }

  IRInst* h = head_IRInstList(insts);
  switch (h->kind) {
    case IR_NOP:
      break;
    case IR_IMM:
      emit(p, "mov %s, %d", reg_of(h->rd), h->imm);
      break;
    case IR_ARG:
      assert(false && "IR_ARG can't be last by codegen");
      break;
    case IR_MOV:
      emit(p, "mov %s, %s", reg_of(h->rd), nth_reg_of(0, h->ras));
      break;
    case IR_TRUNC: {
      Reg* opr = get_RegVec(h->ras, 0);
      emit(p, "mov %s, %s", reg_of(h->rd), reg_name(opr->real, h->rd->size));
      break;
    }
    case IR_SEXT:
      emit(p, "movsx %s, %s", reg_of(h->rd), nth_reg_of(0, h->ras));
      break;
    case IR_RET:
      assert(!bb->is_call_bb);
      if (length_RegVec(h->ras) != 0) {
        assert(length_RegVec(h->ras) == 1);
        assert(get_RegVec(h->ras, 0)->real == rax_reg_id);
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
      if (bb->is_call_bb) {
        emit_save_regs(p, bb->should_preserve, true);
      }
      break;
    case IR_JUMP:
      if (bb->is_call_bb) {
        emit_restore_regs(p, bb->should_preserve, true);
      }
      emit_(p, "jmp ");
      id_label_name(p, h->jump->global_id);
      fprintf(p, "\n");
      break;
    case IR_BR:
      assert(!bb->is_call_bb);
      emit(p, "cmp %s, 0", nth_reg_of(0, h->ras));
      emit_(p, "je ");
      id_label_name(p, h->else_->global_id);
      fprintf(p, "\n");
      emit_(p, "jmp ");
      id_label_name(p, h->then_->global_id);
      fprintf(p, "\n");
      break;
    case IR_GLOBAL_ADDR:
      switch (h->global_kind) {
        case GN_FUNCTION:
          emit(p, "lea %s, %s@PLT[rip]", reg_of(h->rd), h->global_name);
          break;
        case GN_DATA:
          emit(p, "mov %s, [rip + %s@GOTPCREL]", reg_of(h->rd), h->global_name);
          break;
      }
      break;
    case IR_CALL:
      for (unsigned i = 1; i < length_RegVec(h->ras); i++) {
        assert(get_RegVec(h->ras, i)->real == nth_arg_id(i - 1));
      }
      if (h->is_vararg) {
        emit(p, "mov rax, 0");
      }
      emit(p, "call %s", nth_reg_of(0, h->ras));
      if (h->rd != NULL) {
        assert(h->rd->real == rax_reg_id);
      }
      break;
    default:
      CCC_UNREACHABLE;
  }

  codegen_insts(p, f, bb, tail_IRInstList(insts));
}

static void codegen_cmp(FILE* p, const char* s, Reg* rd, Reg* rhs) {
  emit(p, "cmp %s, %s", reg_of(rd), reg_of(rhs));
  emit(p, "set%s %s", s, regs8[rd->real]);
  emit(p, "movzb %s, %s", reg_of(rd), regs8[rd->real]);
}

static void codegen_unaop(FILE* p, IRInst* inst) {
  Reg* rd  = inst->rd;
  Reg* opr = get_RegVec(inst->ras, 0);

  assert(rd->real == opr->real);

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
  Reg* rd  = inst->rd;
  Reg* lhs = get_RegVec(inst->ras, 0);
  Reg* rhs = get_RegVec(inst->ras, 1);

  // A = B op A instruction can't be emitted
  assert(rd->real != rhs->real);

  // rem operator is exceptionally avoided
  // rdx = rax % reg
  if (inst->binop != BINOP_REM) {
    assert(rd->real == lhs->real);
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
      assert(lhs->real == rax_reg_id);
      assert(rd->real == rax_reg_id);
      emit(p, "cqo");
      emit(p, "idiv %s", reg_of(rhs));
      return;
    case BINOP_REM:
      assert(lhs->real == rax_reg_id);
      assert(rd->real == rdx_reg_id);
      emit(p, "cqo");
      emit(p, "idiv %s", reg_of(rhs));
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
      assert(rhs->real == rcx_reg_id);
      // TODO: Consider signedness
      emit(p, "sar %s, cl", reg_of(rd));
      return;
    case BINOP_SHIFT_LEFT:
      assert(rhs->real == rcx_reg_id);
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

static void codegen_blocks(FILE* p, Function* f, BBListIterator* it) {
  if (is_nil_BBListIterator(it)) {
    return;
  }

  BasicBlock* b = data_BBListIterator(it);

  codegen_insts(p, f, b, b->insts);

  codegen_blocks(p, f, next_BBListIterator(it));
}

static void codegen_functions(FILE* p, FunctionList* l) {
  if (is_nil_FunctionList(l)) {
    return;
  }

  Function* f = head_FunctionList(l);
  emit(p, ".global %s", f->name);
  emit_label(p, f->name);
  emit_prologue(p, f);
  codegen_blocks(p, f, front_BBList(f->blocks));

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
    case GE_NAME:
      emit(p, ".quad %s", expr->name);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static void codegen_global_init(FILE* p, GlobalInitializer* l) {
  if (is_nil_GlobalInitializer(l)) {
    return;
  }
  codegen_global_expr(p, head_GlobalInitializer(l));
  codegen_global_init(p, tail_GlobalInitializer(l));
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
  emit(p, ".data");
  codegen_globals(p, ir->globals);
  emit(p, ".text");
  codegen_functions(p, ir->functions);
}
