#include "vector.h"

Vector *vector_new() {
  Vector *vector = (Vector *) calloc(1, sizeof(Vector));

  int init_size = 64;
  vector->size = init_size;
  vector->length = 0;
  vector->array = (void **) calloc(init_size, sizeof(void *));
  vector->array[0] = NULL;

  return vector;
}

void vector_push(Vector *vector, void *value) {
  vector->array[vector->length++] = value;

  if (vector->length >= vector->size) {
    vector->size *= 2;
    vector->array = (void **) realloc(vector->array, sizeof(void *) * vector->size);
  }

  vector->array[vector->length] = NULL;
}

void *vector_pop(Vector *vector) {
  return vector->array[--vector->length];
}

void vector_merge(Vector *dest, Vector *src) {
  for (int i = 0; i < src->length; i++) {
    void *value = src->array[i];
    vector_push(dest, value);
  }
}
