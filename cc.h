#ifndef _CC_H_
#define _CC_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

struct string {
  int size, length;
  char *buffer;
};
typedef struct string String;

extern String *string_new();
extern void string_push(String *string, char c);

struct map {
  int count;
  char *keys[1024];
  void *values[1024];
};
typedef struct map Map;

extern Map *map_new();
extern int map_count(Map *map);
extern bool map_put(Map *map, char *key, void *value);
extern void *map_lookup(Map *map, char *key);

extern void error(char *message);

#endif
