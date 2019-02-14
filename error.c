#include "sk2cc.h"

noreturn void error(Token *token, char *format, ...) {
  char *filename = token->filename;
  char *line_ptr = token->line_ptr;
  int lineno = token->lineno;
  int column = token->column;

  int len = 0;
  while (line_ptr[len] != '\n' && line_ptr[len] != '\0') len++;
  line_ptr[len] = '\0';

  va_list ap;
  fprintf(stderr, "%s:%d:%d: error: ", filename, lineno, column);
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fprintf(stderr, "\n");

  fprintf(stderr, " ");
  fprintf(stderr, "%s\n", line_ptr);

  for (int i = 0; i < column; i++) fprintf(stderr, " ");
  fprintf(stderr, "^\n");

  exit(1);
}
