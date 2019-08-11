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
    push_ExprVec(args, expr(t));
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
        Expr* n   = new_node_unaop(UNAOP_DEREF, new_node_binop(BINOP_ADD, node, idx));
        expect(t, TK_RBRACKET);
        node = n;
        break;
      default:
        return node;
    }
  }
}

static Expr* unary(TokenList** t) {
  switch (head_of(t)) {
    case TK_PLUS:
      consume(t);
      // parse `+n` as `n`
      return postfix(t);
    case TK_MINUS:
      consume(t);
      // parse `-n` as `0 - n`
      return new_node_binop(BINOP_SUB, new_node_num(0), postfix(t));
    case TK_STAR:
      consume(t);
      return new_node_unaop(UNAOP_DEREF, postfix(t));
    case TK_AND:
      consume(t);
      return new_node_unaop(UNAOP_ADDR, postfix(t));
    default:
      return postfix(t);
  }
}

static Expr* mul(TokenList** t) {
  Expr* node = unary(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_STAR:
        consume(t);
        node = new_node_binop(BINOP_MUL, node, unary(t));
        break;
      case TK_SLASH:
        consume(t);
        node = new_node_binop(BINOP_DIV, node, unary(t));
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

static Expr* relational(TokenList** t) {
  Expr* node = add(t);

  for (;;) {
    switch (head_of(t)) {
      case TK_GT:
        consume(t);
        node = new_node_binop(BINOP_GT, node, add(t));
        break;
      case TK_GE:
        consume(t);
        node = new_node_binop(BINOP_GE, node, add(t));
        break;
      case TK_LT:
        consume(t);
        node = new_node_binop(BINOP_LT, node, add(t));
        break;
      case TK_LE:
        consume(t);
        node = new_node_binop(BINOP_LE, node, add(t));
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

static Expr* assign(TokenList** t) {
  Expr* node = equality(t);

  // `=` has right associativity
  switch (head_of(t)) {
    case TK_EQUAL:
      consume(t);
      return new_node_assign(node, assign(t));
    default:
      return node;
  }
}

static Expr* expr(TokenList** t) {
  return assign(t);
}

static Declarator* try_declarator(TokenList** t) {
  unsigned num_ptrs = 0;
  while (head_of(t) == TK_STAR) {
    consume(t);
    num_ptrs++;
  }

  if (head_of(t) != TK_IDENT) {
    return NULL;
  }

  Declarator* base = new_Declarator(DE_DIRECT);
  base->num_ptrs   = num_ptrs;
  base->name       = strdup(expect(t, TK_IDENT).ident);
  base->name_ref   = base->name;

  Declarator* d = base;

  while (head_of(t) == TK_LBRACKET) {
    consume(t);
    Declarator* ary = new_Declarator(DE_ARRAY);
    ary->decl       = d;
    ary->length     = assign(t);
    ary->name_ref   = base->name;
    expect(t, TK_RBRACKET);

    d = ary;
  }

  return d;
}

static Declarator* declarator(TokenList** t) {
  Declarator* d = try_declarator(t);
  if (d == NULL) {
    error("could not parse the declarator.");
  }

  return d;
}

static DeclarationSpecifiers* try_declaration_specifiers(TokenList** t) {
  DeclarationSpecifiers* s = new_DeclarationSpecifiers();

  for (;;) {
    switch (head_of(t)) {
      case TK_SIGNED:
        consume(t);
        s->type->base |= BT_SIGNED;
        break;
      case TK_UNSIGNED:
        consume(t);
        s->type->base |= BT_UNSIGNED;
        break;
      case TK_INT:
        consume(t);
        s->type->base += BT_INT;
        break;
      case TK_CHAR:
        consume(t);
        s->type->base += BT_CHAR;
        break;
      case TK_LONG:
        consume(t);
        s->type->base += BT_LONG;
        break;
      case TK_SHORT:
        consume(t);
        s->type->base += BT_SHORT;
        break;
      default:
        if (s->type->base == 0) {
          return NULL;
        } else {
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

static Declaration* try_declaration(TokenList** t) {
  DeclarationSpecifiers* s = try_declaration_specifiers(t);
  if (s == NULL) {
    return NULL;
  }

  Declarator* dor = try_declarator(t);
  if (dor == NULL) {
    return NULL;
  }

  Declaration* d = new_declaration(s, dor);

  if (consuming(t).kind != TK_SEMICOLON) {
    return NULL;
  }

  return d;
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
    Declarator* d            = declarator(t);
    cur                      = snoc_ParamList(new_ParameterDecl(s, d), cur);
  } while (try (t, TK_COMMA));

  return list;
}

static ExternalDecl* external_declaration(TokenList** t) {
  DeclarationSpecifiers* spec = declaration_specifiers(t);
  Declarator* d               = declarator(t);
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
