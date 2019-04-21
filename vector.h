typedef struct vector {
  int capacity, length;
  void **buffer;
} Vector;

extern Vector *vector_new(void);
extern void vector_push(Vector *vector, void *value);
extern void *vector_pop(Vector *vector);
extern void *vector_last(Vector *vector);
extern void vector_pushi(Vector *vector, int value);
extern int vector_popi(Vector *vector);
extern int vector_lasti(Vector *vector);
extern void vector_merge(Vector *dest, Vector *src);
