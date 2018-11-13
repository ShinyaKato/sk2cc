#include "as.h"

extern Vector *scan(char *file);
extern Vector *tokenize(char *file, Vector *source);
extern Unit *parse(Vector *lines);
extern Section *gen(Unit *unit);
extern void gen_elf(Section *section, char *output);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s [input file] [output file]\n", argv[0]);
    exit(1);
  }
  char *input = argv[1];
  char *output = argv[2];

  Vector *source = scan(input);
  Vector *lines = tokenize(input, source);
  Unit *unit = parse(lines);
  Section *section = gen(unit);
  gen_elf(section, output);

  return 0;
}
