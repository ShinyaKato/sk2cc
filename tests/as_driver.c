#include "../as.h"

int main(int argc, char *argv[]) {
  char *input = "/dev/stdin";

  Vector *source = scan(input);
  Vector *lines = tokenize(input, source);
  Vector *stmts = parse(lines);
  TransUnit *trans_unit = encode(stmts);

  Binary *text = trans_unit->text;
  for (int i = 0; i < text->length; i++) {
    if (i > 0) printf(" ");
    printf("%02x", text->buffer[i]);
  }

  return 0;
}
