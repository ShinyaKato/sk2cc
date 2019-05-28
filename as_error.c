#include "as.h"

noreturn void as_error(Location *loc, char *filename, int lineno, char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s:%d:%d: error: ", loc->filename, loc->lineno + 1, loc->column + 1);
  vfprintf(stderr, format, ap);
  fprintf(stderr, " (at %s:%d)\n", filename, lineno);
  va_end(ap);

  fprintf(stderr, " %s\n", loc->line_ptr);
  fprintf(stderr, " %*s^\n", loc->column, " ");

  exit(1);
}
