#include "as.h"

String *string_new() {
  String *string = (String *) calloc(1, sizeof(String));

  int init_size = 64;
  string->size = init_size;
  string->length = 0;
  string->buffer = (char *) calloc(init_size, sizeof(char));
  string->buffer[0] = '\0';

  return string;
}

void string_push(String *string, char c) {
  string->buffer[string->length++] = c;

  if (string->length >= string->size) {
    string->size *= 2;
    string->buffer = (char *) realloc(string->buffer, sizeof(char) * string->size);
  }

  string->buffer[string->length] = '\0';
}

void string_write(String *string, char *buffer) {
  for (int i = 0; buffer[i]; i++) {
    string_push(string, buffer[i]);
  }
}
