#ifndef CCC_PARSER_H
#define CCC_PARSER_H

#include <stdio.h>

#include "ast.h"
#include "lexer.h"

// parse a list of tokens into AST.
AST* parse(TokenList* tokens);

#endif
