#include "cc.h"

int main(void) {
  lex_init();
  parse_init();

  Node *node = parse();
  generate(node);

  return 0;
}
