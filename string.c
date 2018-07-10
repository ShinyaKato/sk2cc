#include "cc.h"

String *string_new() {
  String *string = (String *) malloc(sizeof(String));

  int init_size = 64;
  string->size = init_size;
  string->length = 0;
  string->buffer = (char *) malloc(sizeof(char) * init_size);
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
