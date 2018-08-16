#include "cc.h"

char *read_source(char *file) {
  FILE *fp = fopen(file, "r");

  String *src = string_new();
  while(1) {
    char c = fgetc(fp);
    if (c == EOF) break;
    string_push(src, c);
  }

  return src->buffer;
}
