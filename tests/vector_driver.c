#include <assert.h>
#include "../sk2cc.h"
#include "../vector.h"

int data[128];

int main(void) {
  Vector *vector = vector_new();

  assert(vector->buffer[0] == NULL);

  for (int i = 0; i < 64; i++) {
    assert(vector->length == i);
    assert(vector->capacity == 64);

    void *ptr = (void *) &data[i];
    vector_push(vector, ptr);

    assert(vector->buffer[i] == ptr);
    assert(vector->buffer[i + 1] == NULL);
  }

  for (int i = 64; i < 128; i++) {
    assert(vector->length == i);
    assert(vector->capacity == 128);

    void *ptr = (void *) &data[i];
    vector_push(vector, ptr);

    assert(vector->buffer[i] == ptr);
    assert(vector->buffer[i + 1] == NULL);
  }

  assert(vector->length == 128);
  assert(vector->capacity == 256);

  for (int i = 127; i >= 0; i--) {
    int *ptr = (int *) vector_pop(vector);
    assert(ptr == &data[i]);
    assert(vector->length == i);
  }

  return 0;
}
