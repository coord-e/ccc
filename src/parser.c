#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "map.h"
#include "ops.h"
#include "parser.h"
#include "util.h"

typedef enum {
  NAME_TYPEDEF,
  NAME_VARIABLE,
} NameKind;

DECLARE_MAP(NameKind, NameMap)
static void release_NameKind(NameKind k) {}
static NameKind copy_NameKind(NameKind k) {
  return k;
}
DEFINE_MAP(copy_NameKind, release_NameKind, NameKind, NameMap)

typedef struct {
  TokenList* cur;
  NameMap* names;
} Env;

static Env* init_Env(TokenList* t) {
  Env* env   = calloc(1, sizeof(Env));
  env->cur   = t;
  env->names = new_NameMap(64);
  return env;
}

static void add_name(Env* env, const char* name, NameKind kind) {
  insert_NameMap(env->names, name, kind);
}

static void release_Env(Env* env) {
  release_NameMap(env->names);
  free(env);
}

static bool is_typedef_name(Env* env, const char* name) {
  NameKind k;
  if (!lookup_NameMap(env->names, name, &k)) {
    return false;
  }
  return k == NAME_TYPEDEF;
}

static void consume(Env* env) {
  env->cur = tail_TokenList(env->cur);
}

static Token* consuming(Env* env) {
  TokenList* p = env->cur;
  consume(env);
  return head_TokenList(p);
}

static Token* expect(Env* env, TokenKind k) {
  Token* r = consuming(env);
  if (r->kind != k) {
    error("unexpected token");
  }
  return r;
}

static TokenKind head_of(Env* env) {
  return head_TokenList(env->cur)->kind;
}

// if head_of(env) == k, consume it and return true.
// otherwise, nothing is consumed and false is returned.
static bool try
  (Env* env, TokenKind k) {
    if (head_of(env) == k) {
      consume(env);
      return true;
    }
    return false;
  }

static Expr* expr(Env* env);
static Expr* assign(Env* env);

static DirectDeclarator* try_direct_declarator(Env* env, bool is_abstract) {
  if (!is_abstract && head_of(env) != TK_IDENT) {
    return NULL;
  }

  DirectDeclarator* base = new_DirectDeclarator(is_abstract ? DE_DIRECT_ABSTRACT : DE_DIRECT);
  if (!is_abstract) {
    base->name     = strdup(expect(env, TK_IDENT)->ident);
    base->name_ref = base->name;
  }

  DirectDeclarator* d = base;

  while (head_of(env) == TK_LBRACKET) {
    consume(env);

    Expr* length;
    if (head_of(env) == TK_RBRACKET) {
      // length omitted
      length = NULL;
    } else {
      length = assign(env);
    }

    DirectDeclarator* ary = new_DirectDeclarator(DE_ARRAY);
    ary->decl             = d;
    ary->length           = length;
    ary->name_ref         = base->name;
    expect(env, TK_RBRACKET);

    d = ary;
  }
  return d;
}

static Declarator* try_declarator(Env* env, bool is_abstract) {
  TokenList* save = env->cur;

  unsigned num_ptrs = 0;
  while (head_of(env) == TK_STAR) {
    consume(env);
    num_ptrs++;
  }

  DirectDeclarator* d = try_direct_declarator(env, is_abstract);
  if (d == NULL) {
    env->cur = save;
    return NULL;
  }

  return new_Declarator(d, num_ptrs);
}

static Declarator* declarator(Env* env, bool is_abstract) {
  Declarator* d = try_declarator(env, is_abstract);
  if (d == NULL) {
    error("could not parse the declarator.");
  }

  return d;
}

static DeclaratorList* declarator_list(Env* env) {
  DeclaratorList* list = nil_DeclaratorList();
  DeclaratorList* cur  = list;

  if (head_of(env) == TK_SEMICOLON) {
    return list;
  }
  do {
    cur = snoc_DeclaratorList(declarator(env, false), cur);
  } while (try (env, TK_COMMA));

  return list;
}

static Expr* conditional(Env* env);

static Enumerator* enumerator(Env* env) {
  char* name  = strdup(expect(env, TK_IDENT)->ident);
  Expr* value = NULL;
  if (head_of(env) == TK_EQUAL) {
    consume(env);
    value = conditional(env);
  }

  return new_Enumerator(name, value);
}

static EnumeratorList* enumerator_list(Env* env) {
  EnumeratorList* list = nil_EnumeratorList();
  EnumeratorList* cur  = list;

  while (head_of(env) != TK_RBRACE) {
    cur = snoc_EnumeratorList(enumerator(env), cur);
    try
      (env, TK_COMMA);
  }

  return list;
}

static EnumSpecifier* enum_specifier(Env* env) {
  expect(env, TK_ENUM);

  char* tag = NULL;
  if (head_of(env) == TK_IDENT) {
    tag = strdup(expect(env, TK_IDENT)->ident);
  }

  if (head_of(env) == TK_LBRACE) {
    // ES_DECL
    consume(env);
    EnumSpecifier* s = new_EnumSpecifier(ES_DECL, tag);
    s->enums         = enumerator_list(env);
    expect(env, TK_RBRACE);
    return s;
  } else {
    // ES_NAME
    if (tag == NULL) {
      error("missing enum tag");
    }

    return new_EnumSpecifier(ES_NAME, tag);
  }
}

static DeclarationSpecifiers* declaration_specifiers(Env* env);

static StructDeclaration* struct_declaration(Env* env) {
  DeclarationSpecifiers* spec = declaration_specifiers(env);
  DeclaratorList* declarators = declarator_list(env);
  expect(env, TK_SEMICOLON);
  return new_StructDeclaration(spec, declarators);
}

static StructDeclarationList* struct_declaration_list(Env* env) {
  StructDeclarationList* list = nil_StructDeclarationList();
  StructDeclarationList* cur  = list;

  while (head_of(env) != TK_RBRACE) {
    cur = snoc_StructDeclarationList(struct_declaration(env), cur);
  }

  return list;
}

static StructSpecifier* struct_specifier(Env* env) {
  expect(env, TK_STRUCT);

  char* tag = NULL;
  if (head_of(env) == TK_IDENT) {
    tag = strdup(expect(env, TK_IDENT)->ident);
  }

  if (head_of(env) == TK_LBRACE) {
    // SS_DECL
    consume(env);
    StructSpecifier* s = new_StructSpecifier(SS_DECL, tag);
    s->declarations    = struct_declaration_list(env);
    expect(env, TK_RBRACE);
    return s;
  } else {
    // SS_NAME
    if (tag == NULL) {
      error("missing struct tag");
    }

    return new_StructSpecifier(SS_NAME, tag);
  }
}

static DeclarationSpecifiers* try_declaration_specifiers(Env* env) {
  BaseType bt              = 0;
  StructSpecifier* struct_ = NULL;
  EnumSpecifier* enum_     = NULL;
  bool is_typedef          = false;
  bool is_const            = false;
  bool is_static           = false;
  bool is_extern           = false;
  char* typedef_name       = NULL;

  TokenList* save = env->cur;

  for (;;) {
    switch (head_of(env)) {
      case TK_TYPEDEF:
        consume(env);
        is_typedef = true;
        break;
      case TK_CONST:
        consume(env);
        is_const = true;
        break;
      case TK_EXTERN:
        consume(env);
        is_extern = true;
        break;
      case TK_STATIC:
        consume(env);
        is_static = true;
        break;
      case TK_SIGNED:
        consume(env);
        bt |= BT_SIGNED;
        break;
      case TK_UNSIGNED:
        consume(env);
        bt |= BT_UNSIGNED;
        break;
      case TK_INT:
        consume(env);
        bt += BT_INT;
        break;
      case TK_BOOL:
        consume(env);
        bt += BT_BOOL;
        break;
      case TK_CHAR:
        consume(env);
        bt += BT_CHAR;
        break;
      case TK_LONG:
        consume(env);
        bt += BT_LONG;
        break;
      case TK_SHORT:
        consume(env);
        bt += BT_SHORT;
        break;
      case TK_VOID:
        consume(env);
        bt += BT_VOID;
        break;
      case TK_STRUCT:
        if (struct_ != NULL) {
          error("too many struct types in declaration specifiers");
        }
        struct_ = struct_specifier(env);
        break;
      case TK_ENUM:
        if (enum_ != NULL) {
          error("too many enum types in declaration specifiers");
        }
        enum_ = enum_specifier(env);
        break;
      case TK_IDENT: {
        char* ident = head_TokenList(env->cur)->ident;
        if (is_typedef_name(env, ident)) {
          if (typedef_name != NULL) {
            error("too many typedef names in declaration specifiers");
          }
          consume(env);
          typedef_name = ident;
          break;
        }
      }
        // fallthrough
      default:
        if (bt == 0 && struct_ == NULL && enum_ == NULL && typedef_name == NULL) {
          env->cur = save;
          return NULL;
        }
        if ((bool)bt + (bool)struct_ + (bool)enum_ + (bool)typedef_name != 1) {
          error("too many data types in declaration specifiers");
        }
        DeclarationSpecifiers* s = NULL;
        if (bt != 0) {
          s            = new_DeclarationSpecifiers(DS_BASE);
          s->base_type = bt;
        } else if (struct_ != NULL) {
          s          = new_DeclarationSpecifiers(DS_STRUCT);
          s->struct_ = struct_;
        } else if (enum_ != NULL) {
          s        = new_DeclarationSpecifiers(DS_ENUM);
          s->enum_ = enum_;
        } else {
          assert(typedef_name != NULL);
          s               = new_DeclarationSpecifiers(DS_TYPEDEF_NAME);
          s->typedef_name = strdup(typedef_name);
        }
        s->is_typedef = is_typedef;
        s->is_const   = is_const;
        s->is_static  = is_static;
        s->is_extern  = is_extern;
        return s;
    }
  }
}

static DeclarationSpecifiers* declaration_specifiers(Env* env) {
  DeclarationSpecifiers* s = try_declaration_specifiers(env);
  if (s == NULL) {
    print_TokenList(stderr, env->cur);
    error("could not parse declaration specifiers.");
  }
  return s;
}

static TypeName* try_type_name(Env* env) {
  DeclarationSpecifiers* s = try_declaration_specifiers(env);
  if (s == NULL) {
    return NULL;
  }

  Declarator* dor = try_declarator(env, true);
  if (dor == NULL) {
    return NULL;
  }

  return new_TypeName(s, dor);
}

static InitializerList* try_initializer_list(Env* env);

static Initializer* try_initializer(Env* env) {
  if (head_of(env) != TK_LBRACE) {
    Initializer* init = new_Initializer(IN_EXPR);
    init->expr        = assign(env);
    return init;
  }

  expect(env, TK_LBRACE);
  InitializerList* list = try_initializer_list(env);
  if (list == NULL) {
    return NULL;
  }
  expect(env, TK_RBRACE);

  Initializer* init = new_Initializer(IN_LIST);
  init->list        = list;
  return init;
}

static Initializer* initializer(Env* env) {
  Initializer* init = try_initializer(env);
  if (init == NULL) {
    error("could not parse initializer");
  }
  return init;
}

static InitializerList* try_initializer_list(Env* env) {
  TokenList* save = env->cur;

  InitializerList* cur  = nil_InitializerList();
  InitializerList* list = cur;

  if (head_of(env) == TK_RBRACE) {
    return list;
  }

  do {
    Initializer* init = try_initializer(env);
    if (init == NULL) {
      env->cur = save;
      return NULL;
    }

    cur = snoc_InitializerList(init, cur);
  } while (try (env, TK_COMMA));

  return list;
}

static InitDeclarator* try_init_declarator(Env* env, bool is_typedef) {
  Declarator* decl = try_declarator(env, false);
  if (decl == NULL) {
    return NULL;
  }

  if (decl->direct->name_ref != NULL) {
    add_name(env, decl->direct->name_ref, is_typedef ? NAME_TYPEDEF : NAME_VARIABLE);
  }

  Initializer* init = NULL;
  if (head_of(env) == TK_EQUAL) {
    consume(env);
    init = initializer(env);
  }

  return new_InitDeclarator(decl, init);
}

static InitDeclaratorList* try_init_declarator_list(Env* env, bool is_typedef) {
  TokenList* save = env->cur;

  InitDeclaratorList* cur  = nil_InitDeclaratorList();
  InitDeclaratorList* list = cur;

  if (head_of(env) == TK_SEMICOLON) {
    return list;
  }

  do {
    InitDeclarator* init = try_init_declarator(env, is_typedef);
    if (init == NULL) {
      env->cur = save;
      return NULL;
    }

    cur = snoc_InitDeclaratorList(init, cur);
  } while (try (env, TK_COMMA));

  return list;
}

static InitDeclaratorList* init_declarator_list(Env* env, bool is_typedef) {
  InitDeclaratorList* list = try_init_declarator_list(env, is_typedef);
  if (list == NULL) {
    error("could not parse the init declarator list");
  }
  return list;
}

static Declaration* try_declaration(Env* env) {
  DeclarationSpecifiers* s = try_declaration_specifiers(env);
  if (s == NULL) {
    return NULL;
  }

  InitDeclaratorList* dor = try_init_declarator_list(env, s->is_typedef);
  if (dor == NULL) {
    return NULL;
  }

  Declaration* d = new_declaration(s, dor);

  if (consuming(env)->kind != TK_SEMICOLON) {
    return NULL;
  }

  return d;
}

static Expr* term(Env* env) {
  if (head_of(env) == TK_LPAREN) {
    consume(env);
    Expr* node = expr(env);
    if (head_of(env) == TK_RPAREN) {
      consume(env);
      return node;
    } else {
      error("unmatched parentheses.");
    }
  } else {
    if (head_of(env) == TK_NUMBER) {
      return new_node_num(consuming(env)->number);
    } else if (head_of(env) == TK_IDENT) {
      return new_node_var(consuming(env)->ident);
    } else if (head_of(env) == TK_STRING) {
      Token* tk = consuming(env);
      return new_node_string(tk->string, tk->length);
    } else {
      error("unexpected token.");
    }
  }
}

static ExprVec* argument_list(Env* env) {
  ExprVec* args = new_ExprVec(1);

  if (head_of(env) == TK_RPAREN) {
    return args;
  }

  do {
    push_ExprVec(args, assign(env));
  } while (try (env, TK_COMMA));

  return args;
}

static Expr* postfix(Env* env) {
  Expr* node = term(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_LPAREN:
        // function call
        consume(env);
        ExprVec* args = argument_list(env);
        expect(env, TK_RPAREN);

        Expr* call = new_node(ND_CALL, NULL, NULL);
        call->lhs  = node;
        call->args = args;
        node       = call;
        break;
      case TK_LBRACKET:
        // array subscript
        // e[i] -> *(e + i)
        consume(env);
        Expr* idx = expr(env);
        Expr* n   = new_node_deref(new_node_binop(BINOP_ADD, node, idx));
        expect(env, TK_RBRACKET);
        node = n;
        break;
      case TK_DOT:
        consume(env);
        node = new_node_member(node, expect(env, TK_IDENT)->ident);
        break;
      case TK_ARROW:
        consume(env);
        // `e->ident` is converted to `(e*)->ident`
        node = new_node_member(new_node_deref(node), expect(env, TK_IDENT)->ident);
        break;
      case TK_DOUBLE_PLUS: {
        consume(env);
        // `e++` is converted to `(e+=1)-1`
        Expr* a = new_node_compound_assign(BINOP_ADD, node, new_node_num(1));
        node    = new_node_binop(BINOP_SUB, a, new_node_num(1));
        break;
      }
      case TK_DOUBLE_MINUS: {
        consume(env);
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

static Expr* unary(Env* env) {
  switch (head_of(env)) {
    case TK_PLUS:
      consume(env);
      return new_node_unaop(UNAOP_POSITIVE, postfix(env));
    case TK_MINUS:
      consume(env);
      return new_node_unaop(UNAOP_INTEGER_NEG, postfix(env));
    case TK_TILDE:
      consume(env);
      return new_node_unaop(UNAOP_BITWISE_NEG, postfix(env));
    case TK_EXCL:
      consume(env);
      // `!e` is equivalent to `(0 == e)` (section 6.5.3.3)
      return new_node_binop(BINOP_EQ, new_node_num(0), postfix(env));
    case TK_STAR:
      consume(env);
      return new_node_deref(postfix(env));
    case TK_AND:
      consume(env);
      return new_node_addr(postfix(env));
    case TK_DOUBLE_PLUS:
      consume(env);
      // `++e` is equivalent to `e+=1`
      return new_node_compound_assign(BINOP_ADD, postfix(env), new_node_num(1));
    case TK_DOUBLE_MINUS:
      consume(env);
      // `--e` is equivalent to `e-=1`
      return new_node_compound_assign(BINOP_SUB, postfix(env), new_node_num(1));
    case TK_SIZEOF:
      consume(env);
      if (head_of(env) == TK_LPAREN) {
        TokenList* save = env->cur;
        consume(env);
        TypeName* ty = try_type_name(env);
        if (ty != NULL) {
          expect(env, TK_RPAREN);
          return new_node_sizeof_type(ty);
        }
        env->cur = save;
      }
      return new_node_sizeof_expr(postfix(env));
    default:
      return postfix(env);
  }
}

static Expr* cast(Env* env) {
  if (head_of(env) != TK_LPAREN) {
    return unary(env);
  }

  TokenList* save = env->cur;
  consume(env);

  TypeName* ty = try_type_name(env);
  if (ty == NULL) {
    env->cur = save;
    return unary(env);
  }

  expect(env, TK_RPAREN);

  return new_node_cast(ty, cast(env));
}

static Expr* mul(Env* env) {
  Expr* node = cast(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_STAR:
        consume(env);
        node = new_node_binop(BINOP_MUL, node, cast(env));
        break;
      case TK_SLASH:
        consume(env);
        node = new_node_binop(BINOP_DIV, node, cast(env));
        break;
      case TK_PERCENT:
        consume(env);
        node = new_node_binop(BINOP_REM, node, cast(env));
        break;
      default:
        return node;
    }
  }
}

static Expr* add(Env* env) {
  Expr* node = mul(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_PLUS:
        consume(env);
        node = new_node_binop(BINOP_ADD, node, mul(env));
        break;
      case TK_MINUS:
        consume(env);
        node = new_node_binop(BINOP_SUB, node, mul(env));
        break;
      default:
        return node;
    }
  }
}

static Expr* shift(Env* env) {
  Expr* node = add(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_RIGHT:
        consume(env);
        node = new_node_binop(BINOP_SHIFT_RIGHT, node, add(env));
        break;
      case TK_LEFT:
        consume(env);
        node = new_node_binop(BINOP_SHIFT_LEFT, node, add(env));
        break;
      default:
        return node;
    }
  }
}

static Expr* relational(Env* env) {
  Expr* node = shift(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_GT:
        consume(env);
        node = new_node_binop(BINOP_GT, node, shift(env));
        break;
      case TK_GE:
        consume(env);
        node = new_node_binop(BINOP_GE, node, shift(env));
        break;
      case TK_LT:
        consume(env);
        node = new_node_binop(BINOP_LT, node, shift(env));
        break;
      case TK_LE:
        consume(env);
        node = new_node_binop(BINOP_LE, node, shift(env));
        break;
      default:
        return node;
    }
  }
}

static Expr* equality(Env* env) {
  Expr* node = relational(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_EQ:
        consume(env);
        node = new_node_binop(BINOP_EQ, node, relational(env));
        break;
      case TK_NE:
        consume(env);
        node = new_node_binop(BINOP_NE, node, relational(env));
        break;
      default:
        return node;
    }
  }
}

static Expr* bit_and(Env* env) {
  Expr* node = equality(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_AND:
        consume(env);
        node = new_node_binop(BINOP_AND, node, equality(env));
        break;
      default:
        return node;
    }
  }
}

static Expr* bit_xor(Env* env) {
  Expr* node = bit_and(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_HAT:
        consume(env);
        node = new_node_binop(BINOP_XOR, node, bit_and(env));
        break;
      default:
        return node;
    }
  }
}

static Expr* bit_or(Env* env) {
  Expr* node = bit_xor(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_VERTICAL:
        consume(env);
        node = new_node_binop(BINOP_OR, node, bit_xor(env));
        break;
      default:
        return node;
    }
  }
}

static Expr* logic_and(Env* env) {
  Expr* node = bit_or(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_DOUBLE_AND:
        // e1 && e2
        // is equivalent to
        // e1 ? (e2 ? 1 : 0) : 0
        consume(env);
        node = new_node_cond(node, new_node_cond(bit_or(env), new_node_num(1), new_node_num(0)),
                             new_node_num(0));
        break;
      default:
        return node;
    }
  }
}

static Expr* logic_or(Env* env) {
  Expr* node = logic_and(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_DOUBLE_VERTICAL:
        // e1 || e2
        // is equivalent to
        // e1 ? 1 : (e2 ? 1 : 0)
        consume(env);
        node = new_node_cond(node, new_node_num(1),
                             new_node_cond(logic_and(env), new_node_num(1), new_node_num(0)));
        break;
      default:
        return node;
    }
  }
}

static Expr* conditional(Env* env) {
  Expr* node = logic_or(env);

  switch (head_of(env)) {
    case TK_QUESTION:
      consume(env);
      Expr* then_ = expr(env);
      expect(env, TK_COLON);
      return new_node_cond(node, then_, conditional(env));
    default:
      return node;
  }
}

static Expr* assign(Env* env) {
  Expr* node = conditional(env);

  // `=` has right associativity
  switch (head_of(env)) {
    case TK_EQUAL:
      consume(env);
      return new_node_assign(node, assign(env));
    case TK_STAR_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_MUL, node, assign(env));
    case TK_SLASH_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_DIV, node, assign(env));
    case TK_PERCENT_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_REM, node, assign(env));
    case TK_PLUS_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_ADD, node, assign(env));
    case TK_MINUS_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_SUB, node, assign(env));
    case TK_LEFT_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_SHIFT_LEFT, node, assign(env));
    case TK_RIGHT_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_SHIFT_RIGHT, node, assign(env));
    case TK_AND_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_AND, node, assign(env));
    case TK_HAT_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_XOR, node, assign(env));
    case TK_VERTICAL_EQUAL:
      consume(env);
      return new_node_compound_assign(BINOP_OR, node, assign(env));
    default:
      return node;
  }
}

static Expr* expr(Env* env) {
  Expr* node = assign(env);

  for (;;) {
    switch (head_of(env)) {
      case TK_COMMA:
        consume(env);
        node = new_node_comma(node, assign(env));
        break;
      default:
        return node;
    }
  }
}

static BlockItemList* block_item_list(Env* env);

static Statement* statement(Env* env) {
  switch (head_of(env)) {
    case TK_RETURN: {
      consume(env);
      Expr* e;
      if (head_of(env) == TK_SEMICOLON) {
        e = NULL;
      } else {
        e = expr(env);
      }
      expect(env, TK_SEMICOLON);
      return new_statement(ST_RETURN, e);
    }
    case TK_IF: {
      consume(env);
      expect(env, TK_LPAREN);
      Expr* c = expr(env);
      expect(env, TK_RPAREN);
      Statement* then_ = statement(env);
      Statement* else_;
      if (head_of(env) == TK_ELSE) {
        consume(env);
        else_ = statement(env);
      } else {
        else_ = new_statement(ST_NULL, NULL);
      }
      Statement* s = new_statement(ST_IF, c);
      s->then_     = then_;
      s->else_     = else_;
      return s;
    }
    case TK_WHILE: {
      consume(env);
      expect(env, TK_LPAREN);
      Expr* c = expr(env);
      expect(env, TK_RPAREN);
      Statement* body = statement(env);
      Statement* s    = new_statement(ST_WHILE, c);
      s->body         = body;
      return s;
    }
    case TK_DO: {
      consume(env);
      Statement* body = statement(env);
      expect(env, TK_WHILE);
      expect(env, TK_LPAREN);
      Expr* c = expr(env);
      expect(env, TK_RPAREN);
      expect(env, TK_SEMICOLON);
      Statement* s = new_statement(ST_DO, c);
      s->body      = body;
      return s;
    }
    case TK_FOR: {
      consume(env);
      expect(env, TK_LPAREN);

      Expr* init             = NULL;
      Declaration* init_decl = NULL;
      if (head_of(env) != TK_SEMICOLON) {
        init_decl = try_declaration(env);
        if (init_decl == NULL) {
          init = expr(env);
          expect(env, TK_SEMICOLON);
        }
      } else {
        consume(env);
      }

      Expr* before;
      if (head_of(env) == TK_SEMICOLON) {
        before = new_node_num(1);
      } else {
        before = expr(env);
      }

      expect(env, TK_SEMICOLON);

      Expr* after;
      if (head_of(env) == TK_RPAREN) {
        after = NULL;
      } else {
        after = expr(env);
      }

      expect(env, TK_RPAREN);
      Statement* body = statement(env);

      Statement* s = new_statement(ST_FOR, NULL);
      s->init_decl = init_decl;
      s->init      = init;
      s->before    = before;
      s->after     = after;
      s->body      = body;
      return s;
    }
    case TK_BREAK: {
      consume(env);
      expect(env, TK_SEMICOLON);
      return new_statement(ST_BREAK, NULL);
    }
    case TK_CONTINUE: {
      consume(env);
      expect(env, TK_SEMICOLON);
      return new_statement(ST_CONTINUE, NULL);
    }
    case TK_LBRACE: {
      consume(env);
      Statement* s = new_statement(ST_COMPOUND, NULL);
      s->items     = block_item_list(env);
      expect(env, TK_RBRACE);
      return s;
    }
    case TK_SEMICOLON: {
      consume(env);
      return new_statement(ST_NULL, NULL);
    }
    case TK_CASE: {
      consume(env);
      Expr* e = conditional(env);
      expect(env, TK_COLON);
      Statement* s = new_statement(ST_CASE, e);
      s->body      = statement(env);
      return s;
    }
    case TK_DEFAULT: {
      consume(env);
      expect(env, TK_COLON);
      Statement* s = new_statement(ST_DEFAULT, NULL);
      s->body      = statement(env);
      return s;
    }
    case TK_SWITCH: {
      consume(env);
      expect(env, TK_LPAREN);
      Expr* cond = expr(env);
      expect(env, TK_RPAREN);
      Statement* s = new_statement(ST_SWITCH, cond);
      s->body      = statement(env);
      return s;
    }
    case TK_GOTO: {
      consume(env);
      char* name    = expect(env, TK_IDENT)->ident;
      Statement* s  = new_statement(ST_GOTO, NULL);
      s->label_name = strdup(name);
      return s;
    }
    case TK_IDENT: {
      if (head_TokenList(tail_TokenList(env->cur))->kind == TK_COLON) {
        char* name = expect(env, TK_IDENT)->ident;
        expect(env, TK_COLON);
        Statement* s  = new_statement(ST_LABEL, NULL);
        s->label_name = strdup(name);
        s->body       = statement(env);
        return s;
      }
      // fallthough
    }
    default: {
      Expr* e = expr(env);
      expect(env, TK_SEMICOLON);
      return new_statement(ST_EXPRESSION, e);
    }
  }
}

static BlockItem* block_item(Env* env) {
  TokenList* save = env->cur;
  Declaration* d  = try_declaration(env);
  if (d) {
    return new_block_item(BI_DECL, NULL, d);
  }

  env->cur = save;
  return new_block_item(BI_STMT, statement(env), NULL);
}

static BlockItemList* block_item_list(Env* env) {
  BlockItemList* cur  = nil_BlockItemList();
  BlockItemList* list = cur;

  while (head_of(env) != TK_END && head_of(env) != TK_RBRACE) {
    cur = snoc_BlockItemList(block_item(env), cur);
  }
  return list;
}

static ParamList* parameter_list(Env* env) {
  ParamList* cur  = nil_ParamList();
  ParamList* list = cur;

  if (head_of(env) == TK_RPAREN) {
    return list;
  }

  do {
    if (head_of(env) == TK_ELIPSIS) {
      consume(env);
      cur = snoc_ParamList(new_ParameterDecl(PD_ELIPSIS), cur);
    } else {
      DeclarationSpecifiers* s = declaration_specifiers(env);
      Declarator* d            = try_declarator(env, false);
      if (d == NULL) {
        d = declarator(env, true);
      }
      ParameterDecl* pd = new_ParameterDecl(PD_PARAM);
      pd->spec          = s;
      pd->decl          = d;
      cur               = snoc_ParamList(pd, cur);
    }
  } while (try (env, TK_COMMA));

  return list;
}

static ExternalDecl* external_declaration(Env* env) {
  DeclarationSpecifiers* spec = declaration_specifiers(env);

  TokenList* save = env->cur;
  Declarator* d   = try_declarator(env, false);
  if (d == NULL) {
    expect(env, TK_SEMICOLON);
    Declaration* decl   = new_declaration(spec, nil_InitDeclaratorList());
    ExternalDecl* edecl = new_external_decl(EX_DECL);
    edecl->decl         = decl;
    return edecl;
  }

  if (head_of(env) != TK_LPAREN) {
    // TODO: insufficient duplicated parsing of a declarator
    env->cur                 = save;
    InitDeclaratorList* list = init_declarator_list(env, spec->is_typedef);
    expect(env, TK_SEMICOLON);

    Declaration* decl = new_declaration(spec, list);

    ExternalDecl* edecl = new_external_decl(EX_DECL);
    edecl->decl         = decl;
    return edecl;
  }

  expect(env, TK_LPAREN);
  ParamList* params = parameter_list(env);
  expect(env, TK_RPAREN);
  if (head_of(env) == TK_LBRACE) {
    consume(env);
    FunctionDef* def = new_function_def();
    def->spec        = spec;
    def->decl        = d;
    def->params      = params;
    def->items       = block_item_list(env);
    expect(env, TK_RBRACE);

    ExternalDecl* edecl = new_external_decl(EX_FUNC);
    edecl->func         = def;
    return edecl;
  } else {
    expect(env, TK_SEMICOLON);

    FunctionDecl* decl = new_function_decl();
    decl->spec         = spec;
    decl->decl         = d;
    decl->params       = params;

    ExternalDecl* edecl = new_external_decl(EX_FUNC_DECL);
    edecl->func_decl    = decl;
    return edecl;
  }
}

static TranslationUnit* translation_unit(Env* env) {
  TranslationUnit* cur  = nil_TranslationUnit();
  TranslationUnit* list = cur;

  while (head_of(env) != TK_END) {
    cur = snoc_TranslationUnit(external_declaration(env), cur);
  }
  return list;
}

// parse tokens into AST
AST* parse(TokenList* t) {
  Env* env              = init_Env(t);
  TranslationUnit* unit = translation_unit(env);
  release_Env(env);
  return unit;
}
