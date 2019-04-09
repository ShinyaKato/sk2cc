#include "sk2cc.h"
#include "map.h"

Map *map_new() {
  Map *map = calloc(1, sizeof(Map));
  map->count = 0;
  map->capacity = 64;
  map->keys = calloc(map->capacity, sizeof(char *));
  map->values = calloc(map->capacity, sizeof(void *));
  return map;
}

void map_put(Map *map, char *key, void *value) {
  for (int i = 0; i < map->count; i++) {
    if (strcmp(map->keys[i], key) == 0) {
      map->values[i] = value;
      return;
    }
  }

  map->keys[map->count] = key;
  map->values[map->count] = value;
  map->count++;

  if (map->count == map->capacity) {
    map->capacity *= 2;
    map->keys = realloc(map->keys, sizeof(void *) * map->capacity);
    map->values = realloc(map->values, sizeof(void *) * map->capacity);
  }
}

void *map_lookup(Map *map, char *key) {
  for (int i = 0; i < map->count; i++) {
    if (strcmp(map->keys[i], key) == 0) {
      return map->values[i];
    }
  }

  return NULL;
}

void map_puti(Map *map, char *key, int value) {
  map_put(map, key, (void *) (intptr_t) value);
}

int map_lookupi(Map *map, char *key) {
  return (int) (intptr_t) map_lookup(map, key);
}
