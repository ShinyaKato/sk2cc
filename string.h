typedef int size_t;

void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

typedef struct string {
  int size, length;
  char *buffer;
} String;

extern String *string_new();
extern void string_push(String *string, char c);
