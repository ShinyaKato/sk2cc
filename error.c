#include "cc.h"

noreturn void error(Token *token, char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%d:%d: ", token->lineno + 1, token->column + 1);
  fprintf(stderr, "error: ");
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);

  int length = 0;
  while (token->source[length] != '\n') length++;
  token->source[length] = '\0';

  fprintf(stderr, " ");
  fprintf(stderr, "%s\n", token->source);
  for (int i = 0; i < token->column + 1; i++) fprintf(stderr, " ");
  fprintf(stderr, "^\n");

  exit(1);
}
