typedef struct vector {
  int capacity, length;
  void **buffer;
} Vector;

extern Vector *vector_new();
extern void vector_push(Vector *vector, void *value);
extern void *vector_pop(Vector *vector);
extern void vector_merge(Vector *dest, Vector *src);
