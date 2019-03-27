#include "../as.h"

int main(int argc, char *argv[]) {
  char *input = "/dev/stdin";

  Vector *source = as_scan(input);
  Vector *lines = as_tokenize(input, source);
  Vector *stmts = as_parse(lines);
  TransUnit *trans_unit = as_encode(stmts);

  Binary *text = trans_unit->text->bin;
  for (int i = 0; i < text->length; i++) {
    if (i > 0) printf(" ");
    printf("%02x", text->buffer[i]);
  }

  return 0;
}
