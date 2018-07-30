#include "cc.h"

int main(void) {
  lex_init();

  Node *node = parse();
  analyze(node);
  gen(node);

  return 0;
}
