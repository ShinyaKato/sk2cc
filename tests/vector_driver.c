#include <assert.h>
#include "../sk2cc.h"

int data[128];

int main(void) {
  Vector *vector = vector_new();

  assert(vector->array[0] == NULL);

  for (int i = 0; i < 64; i++) {
    assert(vector->length == i);
    assert(vector->size == 64);

    void *ptr = (void *) &data[i];
    vector_push(vector, ptr);

    assert(vector->array[i] == ptr);
    assert(vector->array[i + 1] == NULL);
  }

  for (int i = 64; i < 128; i++) {
    assert(vector->length == i);
    assert(vector->size == 128);

    void *ptr = (void *) &data[i];
    vector_push(vector, ptr);

    assert(vector->array[i] == ptr);
    assert(vector->array[i + 1] == NULL);
  }

  assert(vector->length == 128);
  assert(vector->size == 256);

  for (int i = 127; i >= 0; i--) {
    int *ptr = (int *) vector_pop(vector);
    assert(ptr == &data[i]);
    assert(vector->length == i);
  }

  return 0;
}
