#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "error.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Invalid number of arguments\n");
    return 1;
  }

  char* input = argv[1];
  Token* tokens = tokenize(input);
  print_tokens(tokens);

  error("parser is not implemented yet");

  return 0;
}
