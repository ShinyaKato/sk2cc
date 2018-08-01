#include "cc.h"

noreturn void error(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "error: ");
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);

  exit(1);
}
