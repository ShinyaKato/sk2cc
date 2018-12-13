#include "vector.h"

Vector *vector_new() {
  Vector *vector = (Vector *) calloc(1, sizeof(Vector));

  int capacity = 64;
  vector->capacity = capacity;
  vector->length = 0;
  vector->buffer = (void **) calloc(capacity, sizeof(void *));
  vector->buffer[0] = NULL;

  return vector;
}

void vector_push(Vector *vector, void *value) {
  vector->buffer[vector->length++] = value;

  if (vector->length >= vector->capacity) {
    vector->capacity *= 2;
    vector->buffer = (void **) realloc(vector->buffer, sizeof(void *) * vector->capacity);
  }

  vector->buffer[vector->length] = NULL;
}

void *vector_pop(Vector *vector) {
  return vector->buffer[--vector->length];
}

void vector_merge(Vector *dest, Vector *src) {
  for (int i = 0; i < src->length; i++) {
    void *value = src->buffer[i];
    vector_push(dest, value);
  }
}
