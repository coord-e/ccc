#include <argp.h>
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

static char doc[] = "ccc: c compiler";

static char args_doc[] =
    "[--emit-tokens FILE] [--emit-ast FILE] [--emit-ir1 FILE] [--emit-ir2 FILE] -o FILE SOURCE";

static struct argp_option options[] = {
    {"emit-tokens", 't', "FILE", 0, "Dump tokens to the file"},
    {"emit-ast", 'a', "FILE", 0, "Dump parsed ast to the file"},
    {"emit-ir1", 'c', "FILE", 0, "Dump the initial IR to the file"},
    {"emit-ir2", 'i', "FILE", 0, "Dump the final IR to the file"},
    {"output", 'o', "FILE", 0, "Output to FILE"},
    {0}};

typedef struct {
  char* emit_tokens;
  char* emit_ast;
  char* emit_ir1;
  char* emit_ir2;

  char* output;
  char* source;
} Options;

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
  Options* opts = state->input;

  switch (key) {
    case 't':
      opts->emit_tokens = arg;
      break;
    case 'a':
      opts->emit_ast = arg;
      break;
    case 'c':
      opts->emit_ir1 = arg;
      break;
    case 'i':
      opts->emit_ir2 = arg;
      break;
    case 'o':
      opts->output = arg;
      break;

    case ARGP_KEY_ARG:
      if (state->arg_num >= 1) {
        argp_usage(state);
      }
      opts->source = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < 1) {
        argp_usage(state);
      } else if (opts->output == NULL) {
        argp_usage(state);
      }
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

char* read_file(const char* path) {
  FILE* f = fopen(path, "rb");
  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char* buf = malloc(size + 1);
  fread(buf, 1, size, f);
  fclose(f);

  buf[size] = 0;

  return buf;
}

int main(int argc, char** argv) {
  Options opts;
  argp_parse(&argp, argc, argv, 0, 0, &opts);

  char* input = read_file(opts.source);

  TokenList* tokens = tokenize(input);
  free(input);
  if (opts.emit_tokens != NULL) {
    FILE* f = fopen(opts.emit_tokens, "w");
    print_TokenList(f, tokens);
    fclose(f);
  }

  AST* tree = parse(tokens);
  release_TokenList(tokens);
  if (opts.emit_ast != NULL) {
    FILE* f = fopen(opts.emit_ast, "w");
    print_AST(f, tree);
    fclose(f);
  }

  IR* ir = generate_IR(tree);
  release_AST(tree);
  if (opts.emit_ir1 != NULL) {
    FILE* f = fopen(opts.emit_ir1, "w");
    print_IR(f, ir);
    fclose(f);
  }

  reorder_blocks(ir);

  RegIntervals* v = liveness(ir);
  reg_alloc(num_regs, v, ir);
  release_RegIntervals(v);

  if (opts.emit_ir2 != NULL) {
    FILE* f = fopen(opts.emit_ir2, "w");
    print_IR(f, ir);
    fclose(f);
  }

  FILE* f = fopen(opts.output, "w");
  codegen(f, ir);
  fclose(f);
  release_IR(ir);

  return 0;
}
