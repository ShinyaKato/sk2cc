#include "as.h"

void assemble(char *input, char *output) {
  Vector *source = as_scan(input);
  Vector *lines = as_tokenize(input, source);
  Vector *stmts = as_parse(lines);
  TransUnit *trans_unit = as_encode(stmts);
  gen_obj(trans_unit, output);
}
