#include "cc.h"

char peek_char() {
  char c = fgetc(stdin);
  ungetc(c, stdin);
  return c;
}

char get_char() {
  return fgetc(stdin);
}

bool read_char(char c) {
  if (peek_char() == c) {
    get_char();
    return true;
  }
  return false;
}
