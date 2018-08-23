#include "cc.h"

char *scan(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    fprintf(stderr, "%s: failed to open file.", filename);
    exit(1);
  }

  String *src = string_new();
  while(1) {
    char c = fgetc(fp);
    if (c == EOF) break;
    string_push(src, c);
  }

  return src->buffer;
}
