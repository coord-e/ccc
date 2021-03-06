#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arch.h"
#include "codegen.h"
#include "const_fold_tree.h"
#include "data_flow.h"
#include "dead_code_elim.h"
#include "error.h"
#include "ir.h"
#include "lexer.h"
#include "mem2reg.h"
#include "merge.h"
#include "parser.h"
#include "peephole.h"
#include "propagation.h"
#include "reg_alloc.h"
#include "reorder.h"
#include "sema.h"

static char doc[] = "ccc: c compiler";

static char args_doc[] =
    "[--emit-tokens FILE] [--emit-ast FILE] [--emit-ir1 FILE] [--emit-ir2 FILE] [-On] -o FILE "
    "SOURCE";

static struct argp_option options[] = {
    {"emit-tokens", 't', "FILE", 0, "Dump tokens to the file"},
    {"emit-ast1", 'a', "FILE", 0, "Dump parsed ast to the file"},
    {"emit-ast2", 's', "FILE", 0, "Dump analyzed ast to the file"},
    {"emit-ir1", 'c', "FILE", 0, "Dump the initial IR to the file"},
    {"emit-ir2", 'i', "FILE", 0, "Dump the target-specific IR to the file"},
    {"emit-ir3", 'f', "FILE", 0, "Dump the final IR to the file"},
    {"optimize", 'O', "INTEGER", 0, "Number of optimization iterations"},
    {"output", 'o', "FILE", 0, "Output to FILE"},
    {0}};

typedef struct {
  char* emit_tokens;
  char* emit_ast1;
  char* emit_ast2;
  char* emit_ir1;
  char* emit_ir2;
  char* emit_ir3;

  unsigned optimize;

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
      opts->emit_ast1 = arg;
      break;
    case 's':
      opts->emit_ast2 = arg;
      break;
    case 'c':
      opts->emit_ir1 = arg;
      break;
    case 'f':
      opts->emit_ir3 = arg;
      break;
    case 'i':
      opts->emit_ir2 = arg;
      break;
    case 'o':
      opts->output = arg;
      break;
    case 'O':
      opts->optimize = atoi(arg);
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

static bool is_hyphen(const char* path) {
  return path[0] == '-' && path[1] == '\0';
}

static FILE* open_file(const char* path, const char* mode) {
  if (mode[0] == 'w' && is_hyphen(path)) {
    return stdout;
  }
  FILE* f = fopen(path, mode);
  if (f == NULL) {
    error("could not open \"%s\": %s", path, strerror(errno));
  }
  return f;
}

static void close_file(FILE* f) {
  if (f == stdout) {
    return;
  }
  fclose(f);
}

static char* read_file(const char* path) {
  FILE* f = open_file(path, "rb");
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
  Options opts = {0};
  argp_parse(&argp, argc, argv, 0, 0, &opts);

  char* input = read_file(opts.source);

  TokenList* tokens = tokenize(input);
  free(input);
  if (opts.emit_tokens != NULL) {
    FILE* f = open_file(opts.emit_tokens, "w");
    print_TokenList(f, tokens);
    close_file(f);
  }

  AST* tree = parse(tokens);
  release_TokenList(tokens);
  if (opts.emit_ast1 != NULL) {
    FILE* f = open_file(opts.emit_ast1, "w");
    print_AST(f, tree);
    close_file(f);
  }

  sema(tree);
  if (opts.emit_ast2 != NULL) {
    FILE* f = open_file(opts.emit_ast2, "w");
    print_AST(f, tree);
    close_file(f);
  }

  const_fold_tree(tree);

  // TODO: reduce the number of semantic analysis
  sema(tree);
  IR* ir = generate_IR(tree);
  release_AST(tree);
  if (opts.emit_ir1 != NULL) {
    FILE* f = open_file(opts.emit_ir1, "w");
    print_IR(f, ir);
    close_file(f);
  }

  arch(ir);
  if (opts.emit_ir2 != NULL) {
    FILE* f = open_file(opts.emit_ir2, "w");
    print_IR(f, ir);
    close_file(f);
  }

  for (unsigned i = 0; i < opts.optimize + 1; i++) {
    peephole(ir);
    mem2reg(ir);

    reach_data_flow(ir);
    propagation(ir);

    live_data_flow(ir);
    dead_code_elim(ir);

    remove_dead_blocks(ir);
    merge_blocks(ir);
    reorder_blocks(ir);
  }

  live_data_flow(ir);
  reg_alloc(num_regs, ir);

  if (opts.emit_ir3 != NULL) {
    FILE* f = open_file(opts.emit_ir3, "w");
    print_IR(f, ir);
    close_file(f);
  }

  FILE* f = open_file(opts.output, "w");
  codegen(f, ir);
  close_file(f);

  release_IR(ir);

  return 0;
}
