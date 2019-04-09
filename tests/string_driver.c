#include <assert.h>
#include "../sk2cc.h"
#include "../string.h"

int main(void) {
  String *string = string_new();

  assert(string->buffer[0] == '\0');

  for (int i = 0; i < 64; i++) {
    assert(string->length == i);
    assert(string->capacity == 64);

    char c = 'a' + i % 26;
    string_push(string, c);

    assert(string->buffer[i] == c);
    assert(string->buffer[i + 1] == '\0');
  }

  for (int i = 64; i < 128; i++) {
    assert(string->length == i);
    assert(string->capacity == 128);

    char c = 'a' + i % 26;
    string_push(string, c);

    assert(string->buffer[i] == c);
    assert(string->buffer[i + 1] == '\0');
  }

  assert(string->length == 128);
  assert(string->capacity == 256);

  return 0;
}
