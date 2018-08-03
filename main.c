#include "cc.h"

int main(void) {
  Node *node = parse();
  analyze(node);
  gen(node);

  return 0;
}
