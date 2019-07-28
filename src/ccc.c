#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "error.h"
#include "ir.h"
#include "lexer.h"
#include "liveness.h"
#include "parser.h"
#include "reg_alloc.h"
#include "reorder.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Invalid number of arguments\n");
    return 1;
  }

  char* input = argv[1];

  TokenList* tokens = tokenize(input);
  /* print_TokenList(stderr, tokens); */

  AST* tree = parse(tokens);
  release_TokenList(tokens);
  /* print_AST(stderr, tree); */

  IR* ir = generate_IR(tree);
  release_AST(tree);
  reorder_blocks(ir);
  RegIntervals* v = liveness(ir);
  reg_alloc(num_regs, v, ir);
  release_RegIntervals(v);
  print_IR(stdout, ir);

  codegen(stdout, ir);
  release_IR(ir);

  return 0;
}
