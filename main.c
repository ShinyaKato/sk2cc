#include "sk2cc.h"

extern void compile(char *input, bool cpp);
extern void assemble(char *input, char *output);

int main(int argc, char **argv) {
  char *command = argv[0];

  if (argc >= 2 && strcmp(argv[1], "--as") == 0) {
    if (argc != 4) {
      fprintf(stderr, "usage: %s --as [input file] [output file]\n", command);
      exit(1);
    }

    char *input = argv[2];
    char *output = argv[3];
    assemble(input, output);
  } else if (argc >= 2 && strcmp(argv[1], "--cpp") == 0) {
    if (argc != 3) {
      fprintf(stderr, "usage: %s --cpp [input file]\n", command);
      exit(1);
    }

    char *input = argv[2];
    compile(input, true);
  } else {
    if (argc != 2) {
      fprintf(stderr, "usage: %s [input file]\n", command);
      exit(1);
    }

    char *input = argv[1];
    compile(input, false);
  }

  return 0;
}
