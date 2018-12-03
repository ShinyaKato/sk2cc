#include "as.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s [input file] [output file]\n", argv[0]);
    exit(1);
  }
  char *input = argv[1];
  char *output = argv[2];

  Vector *source = scan(input);
  Vector *lines = tokenize(input, source);
  Vector *stmts = parse(lines);
  TransUnit *trans_unit = encode(stmts);
  gen_obj(trans_unit, output);

  return 0;
}
