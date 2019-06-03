#include "../as.h"

int main(int argc, char *argv[]) {
  Vector *tokens = as_tokenize("-");
  Vector *stmts = as_parse(tokens);
  as_sema(stmts);
  TransUnit *trans_unit = as_encode(stmts);

  Binary *text = trans_unit->text->bin;
  for (int i = 0; i < text->length; i++) {
    if (i > 0) printf(" ");
    printf("%02x", text->buffer[i]);
  }

  return 0;
}
