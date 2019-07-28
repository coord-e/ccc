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

  IR* ir1 = generate_IR(tree);
  release_AST(tree);
  reorder_blocks(ir1);
  liveness(ir1);
  /* IR* ir2 = reg_alloc(num_regs, ir1); */
  print_IR(stderr, ir1);

  /* codegen(stdout, ir2); */
  release_IR(ir1);

  return 0;
}
