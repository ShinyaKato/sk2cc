#include "sk2cc.h"

Vector *scan(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    fprintf(stderr, "%s: failed to open file.\n", filename);
    exit(1);
  }

  String *file = string_new();
  while (1) {
    char c = fgetc(fp);
    string_push(file, c);
    if (c == EOF) break;
  }

  char *line_ptr = file->buffer;
  int lineno = 0, column = 0;

  Vector *src = vector_new();
  for (int i = 0; i < file->length; i++) {
    char c = file->buffer[i];

    SourceChar *schar = (SourceChar *) calloc(1, sizeof(SourceChar));
    schar->filename = filename;
    schar->char_ptr = &file->buffer[i];
    schar->line_ptr = line_ptr;
    schar->lineno = lineno;
    schar->column = column;

    vector_push(src, schar);

    column++;
    if (c == '\n') {
      line_ptr = &file->buffer[i + 1];
      lineno++;
      column = 0;
    }
  }

  return src;
}
