#define NULL ((void *) 0)

typedef int size_t;

void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

typedef struct vector {
  int size, length;
  void **array;
} Vector;

extern Vector *vector_new();
extern void vector_push(Vector *vector, void *value);
extern void *vector_pop(Vector *vector);
extern void vector_merge(Vector *dest, Vector *src);
