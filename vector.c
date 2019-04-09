#include "sk2cc.h"
#include "vector.h"

Vector *vector_new() {
  Vector *vector = calloc(1, sizeof(Vector));

  int capacity = 64;
  vector->capacity = capacity;
  vector->length = 0;
  vector->buffer = calloc(capacity, sizeof(void *));
  vector->buffer[0] = NULL;

  return vector;
}

void vector_push(Vector *vector, void *value) {
  vector->buffer[vector->length++] = value;

  if (vector->length >= vector->capacity) {
    vector->capacity *= 2;
    vector->buffer = realloc(vector->buffer, sizeof(void *) * vector->capacity);
  }

  vector->buffer[vector->length] = NULL;
}

void *vector_pop(Vector *vector) {
  void *value = vector->buffer[--vector->length];
  vector->buffer[vector->length] = NULL;
  return value;
}

void vector_pushi(Vector *vector, int value) {
  vector_push(vector, (void *) (intptr_t) value);
}

int vector_popi(Vector *vector) {
  return (int) (intptr_t) vector_pop(vector);
}

void vector_merge(Vector *dest, Vector *src) {
  for (int i = 0; i < src->length; i++) {
    void *value = src->buffer[i];
    vector_push(dest, value);
  }
}
