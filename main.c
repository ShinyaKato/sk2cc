#define bool _Bool
#define false 0
#define true 1

typedef struct _IO_FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int fprintf(FILE *stream, char *format, ...);
int strcmp(char *s1, char *s2);
void exit(int status);

extern void compile(char *input, bool cpp);
extern void assemble(char *input, char *output);

int main(int argc, char **argv) {
  char *command = argv[0];

  if (argc >= 2 && strcmp(argv[1], "--as") == 0) {
    if (argc != 4) {
      fprintf(stderr, "usage: %s --as [input file]\n", command);
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
      fprintf(stderr, "usage: %s [input  file]\n", command);
      exit(1);
    }

    char *input = argv[1];
    compile(input, false);
  }

  return 0;
}
