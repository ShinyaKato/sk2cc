#include "cc.h"

int main(int argc, char **argv) {
  Node *node = parse();
  analyze(node);
  gen(node);

  return 0;
}
