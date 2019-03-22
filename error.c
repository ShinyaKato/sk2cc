#include "sk2cc.h"

noreturn void error(Location *loc, char *format, ...) {
  char *filename = loc->filename;
  char *line_ptr = loc->line_ptr;
  int lineno = loc->lineno;
  int column = loc->column;

  // replace '\n' with '\0' for displaying location
  for (int i = 0; line_ptr[i]; i++) {
    if (line_ptr[i] == '\n') {
      line_ptr[i] = '\0';
      break;
    }
  }

  // error message
  va_list ap;
  fprintf(stderr, "%s:%d:%d: error: ", filename, lineno, column);
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fprintf(stderr, "\n");

  // location (line)
  fprintf(stderr, " %s\n", line_ptr);

  // location (column)
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
