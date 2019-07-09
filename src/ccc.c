#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "codegen.h"
#include "parser.h"
#include "error.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Invalid number of arguments\n");
    return 1;
  }

  char* input = argv[1];
  TokenList* tokens = tokenize(input);
  print_TokenList(stderr, tokens);
  Node* tree = parse(tokens);
  print_tree(stderr, tree);
  codegen(stdout, tree);

  return 0;
}
