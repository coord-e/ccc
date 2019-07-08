#ifndef CCC_CODEGEN_H

#include <stdio.h>

#include "parser.h"

// generate x86_64 code and output it to stdout
void codegen(FILE*, Node* node);

#endif
