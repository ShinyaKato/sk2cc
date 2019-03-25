typedef struct map {
  int count;
  char *keys[1024];
  void *values[1024];
} Map;

extern Map *map_new();
extern bool map_put(Map *map, char *key, void *value);
extern void *map_lookup(Map *map, char *key);
extern bool map_puti(Map *map, char *key, int value);
extern int map_lookupi(Map *map, char *key);
