#ifndef CCC_CONST_FOLD_TREE_H
#define CCC_CONST_FOLD_TREE_H

#include "ast.h"

void const_fold_tree(AST*);

bool get_constant(Expr*, long*);
void const_fold_expr(Expr*);

#endif
