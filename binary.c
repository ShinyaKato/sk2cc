#include "sk2cc.h"
#include "binary.h"

Binary *binary_new(void) {
  int capacity = 256;
  Binary *binary = calloc(1, sizeof(Binary));
  binary->capacity = capacity;
  binary->buffer = calloc(capacity, sizeof(Byte));
  return binary;
}

void binary_push(Binary *binary, Byte byte) {
  if (binary->length >= binary->capacity) {
    binary->capacity *= 2;
    binary->buffer = realloc(binary->buffer, binary->capacity);
  }
  binary->buffer[binary->length++] = byte;
}

void binary_append(Binary *binary, int size, ...) {
  va_list ap;
  va_start(ap, size);
  for (int i = 0; i < size; i++) {
    Byte byte = va_arg(ap, int);
    binary_push(binary, byte);
  }
  va_end(ap);
}

void binary_write(Binary *binary, void *buffer, int size) {
  for (int i = 0; i < size; i++) {
    binary_push(binary, ((Byte *) buffer)[i]);
  }
}
