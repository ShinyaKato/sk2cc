typedef struct map {
  int count, capacity;
  char **keys;
  void **values;
} Map;

extern Map *map_new();
extern void map_put(Map *map, char *key, void *value);
extern void *map_lookup(Map *map, char *key);
extern void map_puti(Map *map, char *key, int value);
extern int map_lookupi(Map *map, char *key);
