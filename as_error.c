#include "as.h"

noreturn void as_error(Location *loc, char *file, int lineno, char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s:%d:%d: error: ", loc->filename, loc->lineno, loc->column);
  vfprintf(stderr, format, ap);
  fprintf(stderr, " (at %s:%d)\n", file, lineno);
  va_end(ap);

  fprintf(stderr, " %s\n", loc->line);
  fprintf(stderr, "%*s^\n", loc->column, " ");

  exit(1);
}
