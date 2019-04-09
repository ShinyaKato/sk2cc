#include <assert.h>
#include "../sk2cc.h"
#include "../map.h"

char keys[1024][8];
int values[1024], values2[1024];

int main(void) {
  Map *map = map_new();

  for (int i = 0; i < 1024; i++) {
    keys[i][0] = '0' + (i / 1000 % 10);
    keys[i][1] = '0' + (i / 100 % 10);
    keys[i][2] = '0' + (i / 10 % 10);
    keys[i][3] = '0' + (i / 1 % 10);
    keys[i][4] = '\0';
    values[i] = i;
    values2[i] = i * 3 + 1;
  }

  for (int i = 0; i < 1024; i++) {
    assert(map->count == i);
    map_put(map, keys[i], &values[i]);
  }
  assert(map->count == 1024);

  for (int i = 0; i < 1024; i++) {
    assert(*((int *) map_lookup(map, keys[i])) == i);
  }
  assert(map_lookup(map, "undefined_key") == NULL);

  for (int i = 0; i < 1024; i++) {
    map_put(map, keys[i], &values2[i]);
    assert(*((int *) map_lookup(map, keys[i])) == i * 3 + 1);
  }

  return 0;
}
