#include "as.h"

void assemble(char *input, char *output) {
  Vector *lines = as_tokenize(input);
  Vector *stmts = as_parse(lines);
  TransUnit *trans_unit = as_encode(stmts);
  gen_obj(trans_unit, output);
}
