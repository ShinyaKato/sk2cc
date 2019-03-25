#define bool _Bool
#define false 0
#define true 1
#define NULL ((void *) 0)
typedef signed long long intptr_t;
typedef unsigned long long uintptr_t;
typedef unsigned long long size_t;
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
int strcmp(char *s1, char *s2);

#include "map.h"

Map *map_new() {
  Map *map = calloc(1, sizeof(Map));
  map->count = 0;
  return map;
}

bool map_put(Map *map, char *key, void *value) {
  for (int i = 0; i < map->count; i++) {
    if (strcmp(map->keys[i], key) == 0) {
      map->values[i] = value;
      return true;
    }
  }

  if (map->count >= 1024) {
    return false;
  }

  map->keys[map->count] = key;
  map->values[map->count] = value;
  map->count++;

  return true;
}

void *map_lookup(Map *map, char *key) {
  for (int i = 0; i < map->count; i++) {
    if (strcmp(map->keys[i], key) == 0) {
      return map->values[i];
    }
  }

  return NULL;
}

bool map_puti(Map *map, char *key, int value) {
  return map_put(map, key, (void *) (intptr_t) value);
}

int map_lookupi(Map *map, char *key) {
  return (int) (intptr_t) map_lookup(map, key);
}
