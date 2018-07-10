#include "cc.h"

void error(char *message) {
  fprintf(stderr, "error: %s\n", message);
  exit(1);
}
