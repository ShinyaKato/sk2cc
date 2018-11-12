#include "as.h"

extern Vector *scan(char *file);
extern Vector *tokenize(char *file, Vector *source);
extern void parse(Unit *unit, Vector *lines);
extern void gen(Unit *unit);
extern void gen_elf(Unit *unit, char *output);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s [input file] [output file]\n", argv[0]);
    exit(1);
  }
  char *input = argv[1];
  char *output = argv[2];

  Vector *source = scan(input);
  Vector *lines = tokenize(input, source);

  Unit *unit = (Unit *) calloc(1, sizeof(Unit));
  parse(unit, lines);

  gen(unit);
  gen_elf(unit, output);

  return 0;
}
