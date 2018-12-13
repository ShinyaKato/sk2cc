#include "string.h"

String *string_new() {
  String *string = (String *) calloc(1, sizeof(String));

  int capacity = 64;
  string->capacity = capacity;
  string->length = 0;
  string->buffer = (char *) calloc(capacity, sizeof(char));
  string->buffer[0] = '\0';

  return string;
}

void string_push(String *string, char c) {
  string->buffer[string->length++] = c;

  if (string->length >= string->capacity) {
    string->capacity *= 2;
    string->buffer = (char *) realloc(string->buffer, sizeof(char) * string->capacity);
  }

  string->buffer[string->length] = '\0';
}
