#include <assert.h>
#include "../cc.h"

char keys[1025][8];
int values[1025];

int main(void) {
  Map *map = map_new();

  for (int i = 0; i < 1025; i++) {
    keys[i][0] = '0' + (i / 1000 % 10);
    keys[i][1] = '0' + (i / 100 % 10);
    keys[i][2] = '0' + (i / 10 % 10);
    keys[i][3] = '0' + (i / 1 % 10);
    keys[i][4] = '\0';
    values[i] = i;
  }

  for (int i = 0; i < 1024; i++) {
    assert(map_count(map) == i);
    assert(map_put(map, keys[i], &values[i]));
  }

  assert(map_count(map) == 1024);
  assert(!map_put(map, keys[1024], &values[1024]));

  for (int i = 0; i < 1024; i++) {
    assert(*((int *) map_lookup(map, keys[i])) == i);
  }

  assert(map_lookup(map, keys[1024]) == NULL);

  for (int i = 0; i < 1024; i++) {
    values[i] *= 1024;
    assert(map_put(map, keys[i], &values[i]));
    assert(*((int *) map_lookup(map, keys[i])) == i * 1024);
  }

  return 0;
}
