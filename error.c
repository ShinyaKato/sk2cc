#include "cc.h"

noreturn void error(Location *loc, char *__file, int __lineno, char *format, ...) {
  char *filename = loc->filename;
  char *line_ptr = loc->line_ptr;
  int lineno = loc->lineno;
  int column = loc->column;

  // error message
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s:%d:%d: error: ", filename, lineno, column);
  vfprintf(stderr, format, ap);
  fprintf(stderr, " (at %s:%d)\n", __file, __lineno);
  va_end(ap);

  // location
  fprintf(stderr, " %s\n", line_ptr);
  for (int i = 0; i < column; i++) {
    fprintf(stderr, " ");
  }
  fprintf(stderr, "^\n");

  exit(1);
}

noreturn void internal_error(char *format, ...) {
  va_list ap;
  fprintf(stderr, "internal error: ");
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fprintf(stderr, "\n");

  exit(1);
}
