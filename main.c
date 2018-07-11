#include "cc.h"

int main(void) {
  lex_init();
  parse_init();

  Node *node = parse();
  gen(node);

  return 0;
}
