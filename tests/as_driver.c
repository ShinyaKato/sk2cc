#include "../as.h"

extern Vector *scan(char *file);
extern Vector *tokenize(char *file, Vector *source);
extern Unit *parse(Vector *lines);
extern Section *gen(Unit *unit);
extern void gen_elf(Section *section, char *output);

int main(int argc, char *argv[]) {
  char *input = "/dev/stdin";

  Vector *source = scan(input);
  Vector *lines = tokenize(input, source);
  Unit *unit = parse(lines);
  Section *section = gen(unit);

  for (int i = 0; i < section->text->length; i++) {
    if (i > 0) {
      printf(" ");
    }
    printf("%02x", section->text->buffer[i]);
  }

  return 0;
}
