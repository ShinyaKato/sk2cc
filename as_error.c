#include "as.h"

noreturn void errorf(char *file, int lineno, int column, char *line, char *format, ...) {
  fprintf(stderr, "%s:%d:%d: ", file, lineno + 1, column + 1);

  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);

  fprintf(stderr, " %s\n", line);

  for (int i = 0; i < column + 1; i++) {
    fprintf(stderr, " ");
  }
  fprintf(stderr, "^\n");

  exit(1);
}
