#include "sk2cc.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("[usage] ./sk2cc filename\n");
    return 0;
  }

  char *file = argv[1];
  char *buffer = scan(file);
  Vector *pp_tokens = tokenize(buffer);
  Vector *tokens = preprocess(pp_tokens);
  Node *node = parse(tokens);
  gen(node);

  return 0;
}
