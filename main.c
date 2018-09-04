#include "sk2cc.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("[usage] ./sk2cc filename\n");
    return 0;
  }

  char *filename = argv[1];
  Vector *src = scan(filename);
  Vector *pp_tokens = tokenize(src);
  Vector *tokens = preprocess(pp_tokens);
  Node *node = parse(tokens);
  gen(node);

  return 0;
}
