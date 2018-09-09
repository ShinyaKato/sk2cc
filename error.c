#include "sk2cc.h"

noreturn void error(Token *token, char *format, ...) {
  va_list ap;

  SourceChar *schar = *(token->schar);
  char *filename = schar->filename;
  char *line_ptr = schar->line_ptr;
  int lineno = schar->lineno;
  int column = schar->column;

  int len = 0;
  while (line_ptr[len] != '\n') len++;
  line_ptr[len] = '\0';

  fprintf(stderr, "%s:%d:%d: ", filename, lineno + 1, column + 1);
  fprintf(stderr, "error: ");
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fprintf(stderr, "\n");

  fprintf(stderr, " ");
  fprintf(stderr, "%s\n", line_ptr);

  fprintf(stderr, " ");
  for (int i = 0; i < column; i++) fprintf(stderr, " ");
  fprintf(stderr, "^\n");

  exit(1);
}
