#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "ops.h"
#include "parser.h"
#include "util.h"

static void consume(TokenList** t) {
  *t = tail_TokenList(*t);
}

static Token consuming(TokenList** t) {
  TokenList* p = *t;
  consume(t);
  return head_TokenList(p);
}

static Token expect(TokenList** t, TokenKind k) {
  Token r = consuming(t);
  if (r.kind != k) {
    error("unexpected token");
  }
  return r;
}

static TokenKind head_of(TokenList** t) {
  return head_TokenList(*t).kind;
}

// if head_of(t) == k, consume it and return true.
// otherwise, nothing is consumed and false is returned.
static bool try
  (TokenList** t, TokenKind k) {
    if (head_of(t) == k) {
      consume(t);
      return true;
    }
    return false;
  }

static Expr* expr(TokenList** t);
static Expr* assign(TokenList** t);

static DirectDeclarator* try_direct_declarator(TokenList** t, bool is_abstract) {
  if (!is_abstract && head_of(t) != TK_IDENT) {
    return NULL;
  }

  DirectDeclarator* base = new_DirectDeclarator(is_abstract ? DE_DIRECT_ABSTRACT : DE_DIRECT);
  if (!is_abstract) {
    base->name     = strdup(expect(t, TK_IDENT).ident);
    base->name_ref = base->name;
  }

  DirectDeclarator* d = base;

  while (head_of(t) == TK_LBRACKET) {
    consume(t);

    Expr* length;
    if (head_of(t) == TK_RBRACKET) {
      // length omitted
      length = NULL;
    } else {
      length = assign(t);
    }

    DirectDeclarator* ary = new_DirectDeclarator(DE_ARRAY);
    ary->decl             = d;
    ary->length           = length;
    ary->name_ref         = base->name;
    expect(t, TK_RBRACKET);

    d = ary;
  }
  return d;
}

static Declarator* try_declarator(TokenList** t, bool is_abstract) {
  TokenList* save = *t;

  unsigned num_ptrs = 0;
  while (head_of(t) == TK_STAR) {
    consume(t);
    num_ptrs++;
  }

  DirectDeclarator* d = try_direct_declarator(t, is_abstract);
  if (d == NULL) {
    *t = save;
    return NULL;
  }

  return new_Declarator(d, num_ptrs);
}

static Declarator* declarator(TokenList** t, bool is_abstract) {
  Declarator* d = try_declarator(t, is_abstract);
  if (d == NULL) {
    error("could not parse the declarator.");
  }

  return d;
}

static DeclaratorList* declarator_list(TokenList** t) {
  DeclaratorList* list = nil_DeclaratorList();
  DeclaratorList* cur  = list;

  if (head_of(t) == TK_SEMICOLON) {
    return list;
  }
  do {
    cur = snoc_DeclaratorList(declarator(t, false), cur);
  } while (try (t, TK_COMMA));

  return list;
}

static DeclarationSpecifiers* declaration_specifiers(TokenList** t);

static StructDeclaration* struct_declaration(TokenList** t) {
  DeclarationSpecifiers* spec = declaration_specifiers(t);
  DeclaratorList* declarators = declarator_list(t);
  expect(t, TK_SEMICOLON);
  return new_StructDeclaration(spec, declarators);
}

static StructDeclarationList* struct_declaration_list(TokenList** t) {
  StructDeclarationList* list = nil_StructDeclarationList();
  StructDeclarationList* cur  = list;

  while (head_of(t) == TK_RBRACE) {
    cur = snoc_StructDeclarationList(struct_declaration(t), cur);
  }

  return list;
}

static StructSpecifier* struct_specifier(TokenList** t) {
  expect(t, TK_STRUCT);

  char* tag;
  if (head_of(t) == TK_IDENT) {
    tag = strdup(expect(t, TK_IDENT).ident);
  }

  if (head_of(t) == TK_LPAREN) {
    // SS_DECL
    consume(t);
    StructSpecifier* s = new_StructSpecifier(SS_DECL, tag);
    s->declarations    = struct_declaration_list(t);
    expect(t, TK_RPAREN);
    return s;
  } else {
    // SS_NAME
    if (tag == NULL) {
      error("missing struct tag");
    }

    return new_StructSpecifier(SS_NAME, tag);
  }
}

static DeclarationSpecifiers* try_declaration_specifiers(TokenList** t) {
  BaseType bt              = 0;
  StructSpecifier* struct_ = NULL;

  for (;;) {
    switch (head_of(t)) {
      case TK_SIGNED:
        consume(t);
        bt |= BT_SIGNED;
        break;
      case TK_UNSIGNED:
        consume(t);
        bt |= BT_UNSIGNED;
        break;
      case TK_INT:
        consume(t);
        bt += BT_INT;
        break;
      case TK_BOOL:
        consume(t);
        bt += BT_BOOL;
        break;
      case TK_CHAR:
        consume(t);
        bt += BT_CHAR;
        break;
      case TK_LONG:
        consume(t);
        bt += BT_LONG;
        break;
      case TK_SHORT:
        consume(t);
        bt += BT_SHORT;
        break;
      case TK_VOID:
        consume(t);
        bt += BT_VOID;
        break;
      case TK_STRUCT:
        if (struct_ != NULL) {
          error("too many data types in declaration specifiers");
        }
        struct_ = struct_specifier(t);
        break;
      default:
        if (bt != 0 && struct_ != NULL) {
          error("too many data types in declaration specifiers");
        }
        if (bt == 0 && struct_ == NULL) {
          return NULL;
        }
        if (bt != 0) {
          DeclarationSpecifiers* s = new_DeclarationSpecifiers(DS_BASE);
          s->base_type             = bt;
          return s;
        } else {
          assert(struct_ != NULL);
          DeclarationSpecifiers* s = new_DeclarationSpecifiers(DS_STRUCT);
          s->struct_               = struct_;
          return s;
        }
    }
  }
}

static DeclarationSpecifiers* declaration_specifiers(TokenList** t) {
  DeclarationSpecifiers* s = try_declaration_specifiers(t);
  if (s == NULL) {
    error("could not parse declaration specifiers.");
  }
  return s;
}

static TypeName* try_type_name(TokenList** t) {
  DeclarationSpecifiers* s = try_declaration_specifiers(t);
  if (s == NULL) {
    return NULL;
  }

  Declarator* dor = try_declarator(t, true);
  if (dor == NULL) {
    return NULL;
  }

  return new_TypeName(s, dor);
}

static InitializerList* try_initializer_list(TokenList** t);

static Initializer* try_initializer(TokenList** t) {
  if (head_of(t) != TK_LBRACE) {
    Initializer* init = new_Initializer(IN_EXPR);
    init->expr        = assign(t);
    return init;
  }

  expect(t, TK_LBRACE);
  InitializerList* list = try_initializer_list(t);
  if (list == NULL) {
    return NULL;
  }
  expect(t, TK_RBRACE);

  Initializer* init = new_Initializer(IN_LIST);
  init->list        = list;
  return init;
}

static Initializer* initializer(TokenList** t) {
  Initializer* init = try_initializer(t);
  if (init == NULL) {
    error("could not parse initializer");
  }
  return init;
}

static InitializerList* try_initializer_list(TokenList** t) {
  TokenList* save = *t;

  InitializerList* cur  = nil_InitializerList();
  InitializerList* list = cur;

  if (head_of(t) == TK_RBRACE) {
    return list;
  }

  do {
    Initializer* init = try_initializer(t);
    if (init == NULL) {
      *t = save;
      return NULL;
    }

    cur = snoc_InitializerList(init, cur);
  } while (try (t, TK_COMMA));

  return list;
}

static InitDeclarator* try_init_declarator(TokenList** t) {
  Declarator* decl = try_declarator(t, false);
  if (decl == NULL) {
    return NULL;
  }

  Initializer* init = NULL;
  if (head_of(t) == TK_EQUAL) {
    consume(t);
    init = initializer(t);
  }

  return new_InitDeclarator(decl, init);
}

static InitDeclaratorList* try_init_declarator_list(TokenList** t) {
  TokenList* save = *t;

  InitDeclaratorList* cur  = nil_InitDeclaratorList();
  InitDeclaratorList* list = cur;

  if (head_of(t) == TK_SEMICOLON) {
    return list;
  }

  do {
    InitDeclarator* init = try_init_declarator(t);
    if (init == NULL) {
      *t = save;
      return NULL;
    }

    cur = snoc_InitDeclaratorList(init, cur);
  } while (try (t, TK_COMMA));

  return list;
}

static InitDeclaratorList* init_declarator_list(TokenList** t) {
  InitDeclaratorList* list = try_init_declarator_list(t);
  if (list == NULL) {
    error("could not parse the init declarator list");
  }
  return list;
}

static Declaration* try_declaration(TokenList** t) {
  DeclarationSpecifiers* s = try_declaration_specifiers(t);
  if (s == NULL) {
    return NULL;
  }

  InitDeclaratorList* dor = try_init_declarator_list(t);
  if (dor == NULL) {
    return NULL;
  }

  Declaration* d = new_declaration(s, dor);

  if (consuming(t).kind != TK_SEMICOLON) {
    return NULL;
  }

  return d;
}

static Expr* term(TokenList** t) {
  if (head_of(t) == TK_LPAREN) {
    consume(t);
    Expr* node = expr(t);
    if (head_of(t) == TK_RPAREN) {
      consume(t);
      return node;
    } else {
      error("unmatched parentheses.");
    }
  } else {
    if (head_of(t) == TK_NUMBER) {
      return new_node_num(consuming(t).number);
    } else if (head_of(t) == TK_IDENT) {
      return new_node_var(consuming(t).ident);
    } else if (head_of(t) == TK_STRING) {
      Token tk = consuming(t);
      return new_node_string(tk.string, tk.length);
    } else {
      error("unexpected token.");
    }
  }
}

static ExprVec* argument_list(TokenList** t) {
  ExprVec* args = new_ExprVec(1);

  if (head_of(t) == TK_RPAREN) {
    return args;
  }

  do {
    push_ExprVec(args, assign(t));
  } while (try (t, TK_COMMA));

  return args;
}

static Expr* postfix(TokenList** t) {
  Expr* node = term(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_LPAREN:
        // function call
        consume(t);
        ExprVec* args = argument_list(t);
        expect(t, TK_RPAREN);

        Expr* call = new_node(ND_CALL, NULL, NULL);
        call->lhs  = node;
        call->args = args;
        node       = call;
        break;
      case TK_LBRACKET:
        // array subscript
        // e[i] -> *(e + i)
        consume(t);
        Expr* idx = expr(t);
        Expr* n   = new_node_deref(new_node_binop(BINOP_ADD, node, idx));
        expect(t, TK_RBRACKET);
        node = n;
        break;
      case TK_DOUBLE_PLUS: {
        consume(t);
        // `e++` is converted to `(e+=1)-1`
        Expr* a = new_node_compound_assign(BINOP_ADD, node, new_node_num(1));
        node    = new_node_binop(BINOP_SUB, a, new_node_num(1));
        break;
      }
      case TK_DOUBLE_MINUS: {
        consume(t);
        // `e--` is converted to `(e-=1)+1`
        Expr* a = new_node_compound_assign(BINOP_SUB, node, new_node_num(1));
        node    = new_node_binop(BINOP_ADD, a, new_node_num(1));
        break;
      }
      default:
        return node;
    }
  }
}

static Expr* unary(TokenList** t) {
  switch (head_of(t)) {
    case TK_PLUS:
      consume(t);
      return new_node_unaop(UNAOP_POSITIVE, postfix(t));
    case TK_MINUS:
      consume(t);
      return new_node_unaop(UNAOP_INTEGER_NEG, postfix(t));
    case TK_TILDE:
      consume(t);
      return new_node_unaop(UNAOP_BITWISE_NEG, postfix(t));
    case TK_EXCL:
      consume(t);
      // `!e` is equivalent to `(0 == e)` (section 6.5.3.3)
      return new_node_binop(BINOP_EQ, new_node_num(0), postfix(t));
    case TK_STAR:
      consume(t);
      return new_node_deref(postfix(t));
    case TK_AND:
      consume(t);
      return new_node_addr(postfix(t));
    case TK_DOUBLE_PLUS:
      consume(t);
      // `++e` is equivalent to `e+=1`
      return new_node_compound_assign(BINOP_ADD, postfix(t), new_node_num(1));
    case TK_DOUBLE_MINUS:
      consume(t);
      // `--e` is equivalent to `e-=1`
      return new_node_compound_assign(BINOP_SUB, postfix(t), new_node_num(1));
    case TK_SIZEOF:
      consume(t);
      if (head_of(t) == TK_LPAREN) {
        TokenList* save = *t;
        consume(t);
        TypeName* ty = try_type_name(t);
        if (ty != NULL) {
          expect(t, TK_RPAREN);
          return new_node_sizeof_type(ty);
        }
        *t = save;
      }
      return new_node_sizeof_expr(postfix(t));
    default:
      return postfix(t);
  }
}

static Expr* cast(TokenList** t) {
  if (head_of(t) != TK_LPAREN) {
    return unary(t);
  }

  TokenList* save = *t;
  consume(t);

  TypeName* ty = try_type_name(t);
  if (ty == NULL) {
    *t = save;
    return unary(t);
  }

  expect(t, TK_RPAREN);

  return new_node_cast(ty, cast(t));
}

static Expr* mul(TokenList** t) {
  Expr* node = cast(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_STAR:
        consume(t);
        node = new_node_binop(BINOP_MUL, node, cast(t));
        break;
      case TK_SLASH:
        consume(t);
        node = new_node_binop(BINOP_DIV, node, cast(t));
        break;
      case TK_PERCENT:
        consume(t);
        node = new_node_binop(BINOP_REM, node, cast(t));
        break;
      default:
        return node;
    }
  }
}

static Expr* add(TokenList** t) {
  Expr* node = mul(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_PLUS:
        consume(t);
        node = new_node_binop(BINOP_ADD, node, mul(t));
        break;
      case TK_MINUS:
        consume(t);
        node = new_node_binop(BINOP_SUB, node, mul(t));
        break;
      default:
        return node;
    }
  }
}

static Expr* shift(TokenList** t) {
  Expr* node = add(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_RIGHT:
        consume(t);
        node = new_node_binop(BINOP_SHIFT_RIGHT, node, add(t));
        break;
      case TK_LEFT:
        consume(t);
        node = new_node_binop(BINOP_SHIFT_LEFT, node, add(t));
        break;
      default:
        return node;
    }
  }
}

static Expr* relational(TokenList** t) {
  Expr* node = shift(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_GT:
        consume(t);
        node = new_node_binop(BINOP_GT, node, shift(t));
        break;
      case TK_GE:
        consume(t);
        node = new_node_binop(BINOP_GE, node, shift(t));
        break;
      case TK_LT:
        consume(t);
        node = new_node_binop(BINOP_LT, node, shift(t));
        break;
      case TK_LE:
        consume(t);
        node = new_node_binop(BINOP_LE, node, shift(t));
        break;
      default:
        return node;
    }
  }
}

static Expr* equality(TokenList** t) {
  Expr* node = relational(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_EQ:
        consume(t);
        node = new_node_binop(BINOP_EQ, node, relational(t));
        break;
      case TK_NE:
        consume(t);
        node = new_node_binop(BINOP_NE, node, relational(t));
        break;
      default:
        return node;
    }
  }
}

static Expr* bit_and(TokenList** t) {
  Expr* node = equality(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_AND:
        consume(t);
        node = new_node_binop(BINOP_AND, node, equality(t));
        break;
      default:
        return node;
    }
  }
}

static Expr* bit_xor(TokenList** t) {
  Expr* node = bit_and(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_HAT:
        consume(t);
        node = new_node_binop(BINOP_XOR, node, bit_and(t));
        break;
      default:
        return node;
    }
  }
}

static Expr* bit_or(TokenList** t) {
  Expr* node = bit_xor(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_VERTICAL:
        consume(t);
        node = new_node_binop(BINOP_OR, node, bit_xor(t));
        break;
      default:
        return node;
    }
  }
}

static Expr* logic_and(TokenList** t) {
  Expr* node = bit_or(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_DOUBLE_AND:
        // e1 && e2
        // is equivalent to
        // e1 ? (e2 ? 1 : 0) : 0
        consume(t);
        node = new_node_cond(node, new_node_cond(bit_or(t), new_node_num(1), new_node_num(0)),
                             new_node_num(0));
        break;
      default:
        return node;
    }
  }
}

static Expr* logic_or(TokenList** t) {
  Expr* node = logic_and(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_DOUBLE_VERTICAL:
        // e1 || e2
        // is equivalent to
        // e1 ? 1 : (e2 ? 1 : 0)
        consume(t);
        node = new_node_cond(node, new_node_num(1),
                             new_node_cond(logic_and(t), new_node_num(1), new_node_num(0)));
        break;
      default:
        return node;
    }
  }
}

static Expr* conditional(TokenList** t) {
  Expr* node = logic_or(t);

  switch (head_of(t)) {
    case TK_QUESTION:
      consume(t);
      Expr* then_ = expr(t);
      expect(t, TK_COLON);
      return new_node_cond(node, then_, conditional(t));
    default:
      return node;
  }
}

static Expr* assign(TokenList** t) {
  Expr* node = conditional(t);

  // `=` has right associativity
  switch (head_of(t)) {
    case TK_EQUAL:
      consume(t);
      return new_node_assign(node, assign(t));
    case TK_STAR_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_MUL, node, assign(t));
    case TK_SLASH_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_DIV, node, assign(t));
    case TK_PERCENT_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_REM, node, assign(t));
    case TK_PLUS_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_ADD, node, assign(t));
    case TK_MINUS_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_SUB, node, assign(t));
    case TK_LEFT_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_SHIFT_LEFT, node, assign(t));
    case TK_RIGHT_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_SHIFT_RIGHT, node, assign(t));
    case TK_AND_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_AND, node, assign(t));
    case TK_HAT_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_XOR, node, assign(t));
    case TK_VERTICAL_EQUAL:
      consume(t);
      return new_node_compound_assign(BINOP_OR, node, assign(t));
    default:
      return node;
  }
}

static Expr* expr(TokenList** t) {
  Expr* node = assign(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_COMMA:
        consume(t);
        node = new_node_comma(node, assign(t));
        break;
      default:
        return node;
    }
  }
}

static BlockItemList* block_item_list(TokenList** t);

static Statement* statement(TokenList** t) {
  switch (head_of(t)) {
    case TK_RETURN: {
      consume(t);
      Expr* e;
      if (head_of(t) == TK_SEMICOLON) {
        e = NULL;
      } else {
        e = expr(t);
      }
      expect(t, TK_SEMICOLON);
      return new_statement(ST_RETURN, e);
    }
    case TK_IF: {
      consume(t);
      expect(t, TK_LPAREN);
      Expr* c = expr(t);
      expect(t, TK_RPAREN);
      Statement* then_ = statement(t);
      Statement* else_;
      if (head_of(t) == TK_ELSE) {
        consume(t);
        else_ = statement(t);
      } else {
        else_ = new_statement(ST_NULL, NULL);
      }
      Statement* s = new_statement(ST_IF, c);
      s->then_     = then_;
      s->else_     = else_;
      return s;
    }
    case TK_WHILE: {
      consume(t);
      expect(t, TK_LPAREN);
      Expr* c = expr(t);
      expect(t, TK_RPAREN);
      Statement* body = statement(t);
      Statement* s    = new_statement(ST_WHILE, c);
      s->body         = body;
      return s;
    }
    case TK_DO: {
      consume(t);
      Statement* body = statement(t);
      expect(t, TK_WHILE);
      expect(t, TK_LPAREN);
      Expr* c = expr(t);
      expect(t, TK_RPAREN);
      expect(t, TK_SEMICOLON);
      Statement* s = new_statement(ST_DO, c);
      s->body      = body;
      return s;
    }
    case TK_FOR: {
      consume(t);
      expect(t, TK_LPAREN);

      Expr* init;
      if (head_of(t) == TK_SEMICOLON) {
        init = NULL;
      } else {
        init = expr(t);
      }

      expect(t, TK_SEMICOLON);

      Expr* before;
      if (head_of(t) == TK_SEMICOLON) {
        before = new_node_num(1);
      } else {
        before = expr(t);
      }

      expect(t, TK_SEMICOLON);

      Expr* after;
      if (head_of(t) == TK_RPAREN) {
        after = NULL;
      } else {
        after = expr(t);
      }

      expect(t, TK_RPAREN);
      Statement* body = statement(t);

      Statement* s = new_statement(ST_FOR, NULL);
      s->init      = init;
      s->before    = before;
      s->after     = after;
      s->body      = body;
      return s;
    }
    case TK_BREAK: {
      consume(t);
      expect(t, TK_SEMICOLON);
      return new_statement(ST_BREAK, NULL);
    }
    case TK_CONTINUE: {
      consume(t);
      expect(t, TK_SEMICOLON);
      return new_statement(ST_CONTINUE, NULL);
    }
    case TK_LBRACE: {
      consume(t);
      Statement* s = new_statement(ST_COMPOUND, NULL);
      s->items     = block_item_list(t);
      expect(t, TK_RBRACE);
      return s;
    }
    case TK_SEMICOLON: {
      consume(t);
      return new_statement(ST_NULL, NULL);
    }
    case TK_CASE: {
      consume(t);
      Expr* e = conditional(t);
      expect(t, TK_COLON);
      Statement* s = new_statement(ST_CASE, e);
      s->body      = statement(t);
      return s;
    }
    case TK_DEFAULT: {
      consume(t);
      expect(t, TK_COLON);
      Statement* s = new_statement(ST_DEFAULT, NULL);
      s->body      = statement(t);
      return s;
    }
    case TK_SWITCH: {
      consume(t);
      expect(t, TK_LPAREN);
      Expr* cond = expr(t);
      expect(t, TK_RPAREN);
      Statement* s = new_statement(ST_SWITCH, cond);
      s->body      = statement(t);
      return s;
    }
    case TK_GOTO: {
      consume(t);
      char* name    = expect(t, TK_IDENT).ident;
      Statement* s  = new_statement(ST_GOTO, NULL);
      s->label_name = strdup(name);
      return s;
    }
    case TK_IDENT: {
      if (head_TokenList(tail_TokenList(*t)).kind == TK_COLON) {
        char* name = expect(t, TK_IDENT).ident;
        expect(t, TK_COLON);
        Statement* s  = new_statement(ST_LABEL, NULL);
        s->label_name = strdup(name);
        s->body       = statement(t);
        return s;
      }
      // fallthough
    }
    default: {
      Expr* e = expr(t);
      expect(t, TK_SEMICOLON);
      return new_statement(ST_EXPRESSION, e);
    }
  }
}

static BlockItem* block_item(TokenList** t) {
  TokenList* save = *t;
  Declaration* d  = try_declaration(t);
  if (d) {
    return new_block_item(BI_DECL, NULL, d);
  }

  *t = save;
  return new_block_item(BI_STMT, statement(t), NULL);
}

static BlockItemList* block_item_list(TokenList** t) {
  BlockItemList* cur  = nil_BlockItemList();
  BlockItemList* list = cur;

  while (head_of(t) != TK_END && head_of(t) != TK_RBRACE) {
    cur = snoc_BlockItemList(block_item(t), cur);
  }
  return list;
}

static ParamList* parameter_list(TokenList** t) {
  ParamList* cur  = nil_ParamList();
  ParamList* list = cur;

  if (head_of(t) == TK_RPAREN) {
    return list;
  }

  do {
    DeclarationSpecifiers* s = declaration_specifiers(t);
    Declarator* d            = try_declarator(t, false);
    if (d == NULL) {
      d = declarator(t, true);
    }
    cur = snoc_ParamList(new_ParameterDecl(s, d), cur);
  } while (try (t, TK_COMMA));

  return list;
}

static ExternalDecl* external_declaration(TokenList** t) {
  DeclarationSpecifiers* spec = declaration_specifiers(t);

  TokenList* save = *t;
  Declarator* d   = declarator(t, false);

  if (head_of(t) != TK_LPAREN) {
    // TODO: insufficient duplicated parsing of a declarator
    *t                       = save;
    InitDeclaratorList* list = init_declarator_list(t);
    expect(t, TK_SEMICOLON);

    Declaration* decl = new_declaration(spec, list);

    ExternalDecl* edecl = new_external_decl(EX_DECL);
    edecl->decl         = decl;
    return edecl;
  }

  expect(t, TK_LPAREN);
  ParamList* params = parameter_list(t);
  expect(t, TK_RPAREN);
  if (head_of(t) == TK_LBRACE) {
    consume(t);
    FunctionDef* def = new_function_def();
    def->spec        = spec;
    def->decl        = d;
    def->params      = params;
    def->items       = block_item_list(t);
    expect(t, TK_RBRACE);

    ExternalDecl* edecl = new_external_decl(EX_FUNC);
    edecl->func         = def;
    return edecl;
  } else {
    expect(t, TK_SEMICOLON);

    FunctionDecl* decl = new_function_decl();
    decl->spec         = spec;
    decl->decl         = d;
    decl->params       = params;

    ExternalDecl* edecl = new_external_decl(EX_FUNC_DECL);
    edecl->func_decl    = decl;
    return edecl;
  }
}

static TranslationUnit* translation_unit(TokenList** t) {
  TranslationUnit* cur  = nil_TranslationUnit();
  TranslationUnit* list = cur;

  while (head_of(t) != TK_END) {
    cur = snoc_TranslationUnit(external_declaration(t), cur);
  }
  return list;
}

// parse tokens into AST
AST* parse(TokenList* t) {
  return translation_unit(&t);
}
