#define NULL ((void *) 0)

#define bool _Bool
#define false 0
#define true 1

typedef int size_t;

void *calloc(size_t nmemb, size_t size);

int strcmp(char *s1, char *s2);

typedef struct map {
  int count;
  char *keys[1024];
  void *values[1024];
} Map;

extern Map *map_new();
extern bool map_put(Map *map, char *key, void *value);
extern void *map_lookup(Map *map, char *key);
