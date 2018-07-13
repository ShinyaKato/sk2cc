#include "cc.h"

Vector *vector_new() {
  Vector *vector = (Vector *) malloc(sizeof(Vector));

  int init_size = 64;
  vector->size = init_size;
  vector->length = 0;
  vector->array = (void **) malloc(sizeof(void *) * init_size);
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
