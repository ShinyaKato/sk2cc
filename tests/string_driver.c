#include <assert.h>
#include "../cc.h"

int main(void) {
  String *string = string_new();

  assert(string->buffer[0] == '\0');

  for (int i = 0; i < 64; i++) {
    assert(string->length == i);
    assert(string->size == 64);

    char c = 'a' + i % 26;
    string_push(string, c);

    assert(string->buffer[i] == c);
    assert(string->buffer[i + 1] == '\0');
  }

  for (int i = 64; i < 128; i++) {
    assert(string->length == i);
    assert(string->size == 128);

    char c = 'a' + i % 26;
    string_push(string, c);

    assert(string->buffer[i] == c);
    assert(string->buffer[i + 1] == '\0');
  }

  assert(string->length == 128);
  assert(string->size == 256);

  return 0;
}
