#include "cc.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("[usage] ./cc filename\n");
    return 0;
  }

  char *file = argv[1];
  char *src = read_source(file);
  Vector *tokens = lexical_analyze(src);
  Node *node = parse(tokens);
  analyze(node);
  gen(node);

  return 0;
}
