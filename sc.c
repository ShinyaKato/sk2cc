#include "cc.h"

char peek_char() {
  char c = fgetc(stdin);
  ungetc(c, stdin);
  return c;
}

char get_char() {
  return fgetc(stdin);
}
