#include "as.h"

noreturn void errorf(char *file, int lineno, int column, char *line, char *__file, int __lineno, char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s:%d:%d: error: ", file, lineno + 1, column + 1);
  vfprintf(stderr, format, ap);
  fprintf(stderr, " (at %s:%d)\n", __file, __lineno);
  va_end(ap);

  fprintf(stderr, " %s\n", line);

  for (int i = 0; i < column + 1; i++) {
    fprintf(stderr, " ");
  }
  fprintf(stderr, "^\n");

  exit(1);
}
