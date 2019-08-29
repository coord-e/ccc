#ifndef CCC_CONST_FOLD_TREE_H
#define CCC_CONST_FOLD_TREE_H

#include "ast.h"

void const_fold_tree(AST*);

Expr* fold_expr(Expr*);

#endif
