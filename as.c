#include "as.h"

void assemble(char *input, char *output) {
  Vector *tokens = as_tokenize(input);
  Vector *stmts = as_parse(tokens);
  as_sema(stmts);
  TransUnit *trans_unit = as_encode(stmts);
  gen_obj(trans_unit, output);
}
