#include "ast.h"
#include "util.h"

// release functions

static void release_expr(Expr* e);

static void release_DeclarationSpecifiers(DeclarationSpecifiers* spec) {
  if (spec == NULL) {
    return;
  }

  free(spec);
}

static void release_Declarator(Declarator* d) {
  if (d == NULL) {
    return;
  }

  free(d->name);
  release_Declarator(d->decl);
  release_expr(d->length);
  free(d);
}

static void release_declaration(Declaration* d) {
  if (d == NULL) {
    return;
  }

  release_DeclarationSpecifiers(d->spec);
  release_Declarator(d->declarator);
  release_Type(d->type);
  free(d);
}

static void release_TypeName(TypeName* t) {
  release_declaration(t);
}

static void release_expr(Expr* e) {
  if (e == NULL) {
    return;
  }

  release_expr(e->lhs);
  release_expr(e->rhs);
  release_expr(e->expr);
  free(e->var);
  release_ExprVec(e->args);
  release_TypeName(e->cast_to);
  release_Type(e->cast_type);
  release_Type(e->type);
  release_expr(e->cond);
  release_expr(e->then_);
  release_expr(e->else_);

  free(e);
}

DEFINE_VECTOR(release_expr, Expr*, ExprVec)

static void release_statement(Statement* stmt) {
  if (stmt == NULL) {
    return;
  }

  release_expr(stmt->expr);
  free(stmt);
}

static void release_BlockItem(BlockItem* item) {
  if (item == NULL) {
    return;
  }

  release_statement(item->stmt);
  release_declaration(item->decl);
  free(item);
}

DEFINE_LIST(release_BlockItem, BlockItem*, BlockItemList)

DeclarationSpecifiers* new_DeclarationSpecifiers() {
  return calloc(1, sizeof(DeclarationSpecifiers));
}

ParameterDecl* new_ParameterDecl(DeclarationSpecifiers* spec, Declarator* decl) {
  ParameterDecl* d = calloc(1, sizeof(ParameterDecl));
  d->spec          = spec;
  d->decl          = decl;
  return d;
}

static void release_ParameterDecl(ParameterDecl* d) {
  if (d == NULL) {
    return;
  }

  release_DeclarationSpecifiers(d->spec);
  release_Declarator(d->decl);
  free(d);
}

DEFINE_LIST(release_ParameterDecl, ParameterDecl*, ParamList)

static void release_FunctionDef(FunctionDef* def) {
  if (def == NULL) {
    return;
  }
  release_DeclarationSpecifiers(def->spec);
  release_Declarator(def->decl);
  release_ParamList(def->params);
  release_BlockItemList(def->items);
  release_Type(def->type);
  free(def);
}
static void release_FunctionDecl(FunctionDecl* decl) {
  if (decl == NULL) {
    return;
  }
  release_DeclarationSpecifiers(decl->spec);
  release_Declarator(decl->decl);
  release_ParamList(decl->params);
  release_Type(decl->type);
  free(decl);
}
static void release_ExternalDecl(ExternalDecl* edecl) {
  release_FunctionDef(edecl->func);
  release_FunctionDecl(edecl->func_decl);
  free(edecl);
}

DEFINE_LIST(release_ExternalDecl, ExternalDecl*, TranslationUnit)

void release_AST(AST* t) {
  release_TranslationUnit(t);
}

// printer functions
static void print_expr(FILE* p, Expr* expr);

static void print_DeclarationSpecifiers(FILE* p, DeclarationSpecifiers* s) {
  BaseType b = s->base_type;
  if (b & BT_SIGNED) {
    fputs("signed ", p);
    b &= ~BT_SIGNED;  // clear
  }
  if (b & BT_UNSIGNED) {
    fputs("unsigned ", p);
    b &= ~BT_UNSIGNED;  // clear
  }
  if (b == 0) {
    return;
  }
  switch (b) {
    case BT_INT:
      fputs("int", p);
      break;
    case BT_LONG:
      fputs("long", p);
      break;
    case BT_CHAR:
      fputs("char", p);
      break;
    case BT_SHORT:
      fputs("short", p);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static void print_Declarator(FILE* p, Declarator* d) {
  switch (d->kind) {
    case DE_DIRECT_ABSTRACT:
      for (unsigned i = 0; i < d->num_ptrs; i++) {
        fprintf(p, "*");
      }
      break;
    case DE_DIRECT:
      for (unsigned i = 0; i < d->num_ptrs; i++) {
        fprintf(p, "*");
      }
      fprintf(p, "%s", d->name);
      break;
    case DE_ARRAY:
      print_Declarator(p, d->decl);
      fputs("[", p);
      print_expr(p, d->length);
      fputs("]", p);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static void print_declaration(FILE* p, Declaration* d) {
  print_DeclarationSpecifiers(p, d->spec);
  fputs(" ", p);
  print_Declarator(p, d->declarator);
  fputs(";", p);
}

static void print_TypeName(FILE* p, TypeName* t) {
  print_DeclarationSpecifiers(p, t->spec);
  fputs(" ", p);
  print_Declarator(p, t->declarator);
}

DECLARE_VECTOR_PRINTER(ExprVec)

static void print_expr(FILE* p, Expr* expr) {
  switch (expr->kind) {
    case ND_NUM:
      fprintf(p, "%d", expr->num);
      return;
    case ND_VAR:
      fprintf(p, "%s", expr->var);
      return;
    case ND_ASSIGN:
      fprintf(p, "(");
      print_expr(p, expr->lhs);
      fprintf(p, " = ");
      print_expr(p, expr->rhs);
      fprintf(p, ")");
      return;
    case ND_COMMA:
      fprintf(p, "(");
      print_expr(p, expr->lhs);
      fprintf(p, ", ");
      print_expr(p, expr->rhs);
      fprintf(p, ")");
      return;
    case ND_COMPOUND_ASSIGN:
      fprintf(p, "(");
      print_expr(p, expr->lhs);
      print_binop(p, expr->binop);
      fprintf(p, "= ");
      print_expr(p, expr->rhs);
      fprintf(p, ")");
      return;
    case ND_BINOP:
      fprintf(p, "(");
      print_binop(p, expr->binop);
      fprintf(p, " ");
      print_expr(p, expr->lhs);
      fprintf(p, " ");
      print_expr(p, expr->rhs);
      fprintf(p, ")");
      return;
    case ND_UNAOP:
      fprintf(p, "(");
      print_unaop(p, expr->unaop);
      fprintf(p, " ");
      print_expr(p, expr->expr);
      fprintf(p, ")");
      return;
    case ND_CALL:
      print_expr(p, expr->lhs);
      fprintf(p, "(");
      print_ExprVec(p, expr->args);
      fprintf(p, ")");
      return;
    case ND_CAST:
      fprintf(p, "(");
      if (expr->cast_to == NULL) {
        print_Type(p, expr->cast_type);
      } else {
        print_TypeName(p, expr->cast_to);
      }
      fprintf(p, ")");
      print_expr(p, expr->expr);
      return;
    case ND_ADDR:
      fprintf(p, "(& ");
      print_expr(p, expr->expr);
      fprintf(p, ")");
      return;
    case ND_ADDR_ARY:
      fprintf(p, "(&ary ");
      print_expr(p, expr->expr);
      fprintf(p, ")");
      return;
    case ND_DEREF:
      fprintf(p, "(* ");
      print_expr(p, expr->expr);
      fprintf(p, ")");
      return;
    case ND_COND:
      fputs("(", p);
      print_expr(p, expr->cond);
      fputs("?", p);
      print_expr(p, expr->then_);
      fputs(":", p);
      print_expr(p, expr->else_);
      fputs(")", p);
      return;
    default:
      CCC_UNREACHABLE;
  }
}
DEFINE_VECTOR_PRINTER(print_expr, ",", "", ExprVec)

static void print_statement(FILE* p, Statement* stmt);

static void print_BlockItem(FILE* p, BlockItem* item) {
  switch (item->kind) {
    case BI_DECL:
      print_declaration(p, item->decl);
      break;
    case BI_STMT:
      print_statement(p, item->stmt);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

static void print_ParameterDecl(FILE* p, ParameterDecl* d) {
  print_DeclarationSpecifiers(p, d->spec);
  fputs(" ", p);
  print_Declarator(p, d->decl);
}

DECLARE_LIST_PRINTER(BlockItemList)
DEFINE_LIST_PRINTER(print_BlockItem, "\n", "\n", BlockItemList)

DECLARE_LIST_PRINTER(ParamList)
DEFINE_LIST_PRINTER(print_ParameterDecl, ",", "", ParamList)

DECLARE_LIST_PRINTER(TranslationUnit)
static void print_FunctionDef(FILE* p, FunctionDef* def) {
  print_DeclarationSpecifiers(p, def->spec);
  fputs(" ", p);
  print_Declarator(p, def->decl);
  fprintf(p, " (");
  print_ParamList(p, def->params);
  fprintf(p, ") {\n");
  print_BlockItemList(p, def->items);
  fprintf(p, "}\n");
}
static void print_FunctionDecl(FILE* p, FunctionDecl* decl) {
  print_DeclarationSpecifiers(p, decl->spec);
  fputs(" ", p);
  print_Declarator(p, decl->decl);
  fprintf(p, " (");
  print_ParamList(p, decl->params);
  fprintf(p, ");\n");
}
static void print_ExternalDecl(FILE* p, ExternalDecl* edecl) {
  switch (edecl->kind) {
    case EX_FUNC:
      print_FunctionDef(p, edecl->func);
      break;
    case EX_FUNC_DECL:
      print_FunctionDecl(p, edecl->func_decl);
      break;
    default:
      CCC_UNREACHABLE;
  }
}
DEFINE_LIST_PRINTER(print_ExternalDecl, "\n", "\n", TranslationUnit)

void print_statement(FILE* p, Statement* d) {
  switch (d->kind) {
    case ST_EXPRESSION:
      print_expr(p, d->expr);
      fputs(";", p);
      break;
    case ST_RETURN:
      fputs("return ", p);
      if (d->expr != NULL) {
        print_expr(p, d->expr);
      }
      fputs(";", p);
      break;
    case ST_IF:
      fputs("if (", p);
      print_expr(p, d->expr);
      fputs(") ", p);
      print_statement(p, d->then_);
      fputs(" else ", p);
      print_statement(p, d->else_);
      break;
    case ST_NULL:
      fputs(";", p);
      break;
    case ST_WHILE:
      fputs("while (", p);
      print_expr(p, d->expr);
      fputs(") ", p);
      print_statement(p, d->body);
      break;
    case ST_DO:
      fputs("do ", p);
      print_statement(p, d->body);
      fputs(" while (", p);
      print_expr(p, d->expr);
      fputs(");", p);
      break;
    case ST_FOR:
      fputs("for (", p);
      if (d->init != NULL) {
        print_expr(p, d->init);
      }
      fputs("; ", p);
      print_expr(p, d->before);
      fputs("; ", p);
      if (d->after != NULL) {
        print_expr(p, d->after);
      }
      fputs(") ", p);
      print_statement(p, d->body);
      break;
    case ST_BREAK:
      fputs("break;", p);
      break;
    case ST_CONTINUE:
      fputs("continue;", p);
      break;
    case ST_COMPOUND:
      fputs("{ ", p);
      print_BlockItemList(p, d->items);
      fputs(" }", p);
      break;
    default:
      CCC_UNREACHABLE;
  }
}

void print_AST(FILE* p, AST* ast) {
  print_TranslationUnit(p, ast);
}

// constructors
Expr* new_node(ExprKind kind, Expr* lhs, Expr* rhs) {
  Expr* node      = calloc(1, sizeof(Expr));
  node->kind      = kind;
  node->lhs       = lhs;
  node->rhs       = rhs;
  node->expr      = NULL;
  node->var       = NULL;
  node->args      = NULL;
  node->cast_to   = NULL;
  node->cast_type = NULL;
  node->type      = NULL;
  node->cond      = NULL;
  node->then_     = NULL;
  node->else_     = NULL;
  return node;
}

Expr* new_node_num(int num) {
  Expr* node = new_node(ND_NUM, NULL, NULL);
  node->num  = num;
  return node;
}

Expr* new_node_var(char* ident) {
  Expr* node = new_node(ND_VAR, NULL, NULL);
  node->var  = strdup(ident);
  return node;
}

Expr* new_node_binop(BinopKind kind, Expr* lhs, Expr* rhs) {
  Expr* node  = new_node(ND_BINOP, lhs, rhs);
  node->binop = kind;
  return node;
}

Expr* new_node_unaop(UnaopKind kind, Expr* expr) {
  Expr* node  = new_node(ND_UNAOP, NULL, NULL);
  node->unaop = kind;
  node->expr  = expr;
  return node;
}

Expr* new_node_addr(Expr* expr) {
  Expr* node = new_node(ND_ADDR, NULL, NULL);
  node->expr = expr;
  return node;
}

Expr* new_node_addr_ary(Expr* expr) {
  Expr* node = new_node(ND_ADDR_ARY, NULL, NULL);
  node->expr = expr;
  return node;
}

Expr* new_node_deref(Expr* expr) {
  Expr* node = new_node(ND_DEREF, NULL, NULL);
  node->expr = expr;
  return node;
}

Expr* new_node_assign(Expr* lhs, Expr* rhs) {
  return new_node(ND_ASSIGN, lhs, rhs);
}

Expr* new_node_comma(Expr* lhs, Expr* rhs) {
  return new_node(ND_COMMA, lhs, rhs);
}

Expr* new_node_compound_assign(BinopKind kind, Expr* lhs, Expr* rhs) {
  Expr* node  = new_node(ND_COMPOUND_ASSIGN, lhs, rhs);
  node->binop = kind;
  return node;
}

Expr* new_node_cast(TypeName* ty, Expr* opr) {
  Expr* node    = new_node(ND_CAST, NULL, NULL);
  node->cast_to = ty;
  node->expr    = opr;
  return node;
}

Expr* new_node_cond(Expr* cond, Expr* then_, Expr* else_) {
  Expr* node  = new_node(ND_COND, NULL, NULL);
  node->then_ = then_;
  node->cond  = cond;
  node->else_ = else_;
  return node;
}

Expr* shallow_copy_node(Expr* e) {
  Expr* node = new_node(0, NULL, NULL);
  *node      = *e;
  return node;
}

Declarator* new_Declarator(DeclaratorKind kind) {
  Declarator* d = calloc(1, sizeof(Declarator));
  d->kind       = kind;
  d->name_ref   = NULL;
  d->num_ptrs   = 0;
  d->name       = NULL;
  d->decl       = NULL;
  d->length     = NULL;
  return d;
}

bool is_abstract_declarator(Declarator* d) {
  return d->name_ref == NULL;
}

Declaration* new_declaration(DeclarationSpecifiers* spec, Declarator* s) {
  assert(!is_abstract_declarator(s));
  Declaration* d = calloc(1, sizeof(Declaration));
  d->spec        = spec;
  d->declarator  = s;
  d->type        = NULL;
  return d;
}

TypeName* new_TypeName(DeclarationSpecifiers* spec, Declarator* s) {
  assert(is_abstract_declarator(s));
  TypeName* d   = calloc(1, sizeof(TypeName));
  d->spec       = spec;
  d->declarator = s;
  d->type       = NULL;
  return d;
}

Statement* new_statement(StmtKind kind, Expr* expr) {
  Statement* s = calloc(1, sizeof(Statement));
  s->kind      = kind;
  s->expr      = expr;
  return s;
}

BlockItem* new_block_item(BlockItemKind kind, Statement* stmt, Declaration* decl) {
  BlockItem* item = calloc(1, sizeof(BlockItem));
  item->kind      = kind;
  item->stmt      = stmt;
  item->decl      = decl;
  return item;
}

FunctionDef* new_function_def() {
  FunctionDef* def = calloc(1, sizeof(FunctionDef));
  def->spec        = NULL;
  def->decl        = NULL;
  def->params      = NULL;
  def->items       = NULL;
  def->type        = NULL;
  return def;
}

FunctionDecl* new_function_decl() {
  FunctionDecl* decl = calloc(1, sizeof(FunctionDecl));
  decl->spec         = NULL;
  decl->decl         = NULL;
  decl->params       = NULL;
  decl->type         = NULL;
  return decl;
}

ExternalDecl* new_external_decl(ExtDeclKind kind) {
  ExternalDecl* edecl = calloc(1, sizeof(ExternalDecl));
  edecl->kind         = kind;
  edecl->func         = NULL;
  edecl->func_decl    = NULL;
  return edecl;
}
