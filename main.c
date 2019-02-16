#include "sk2cc.h"

void print_usage() {
  fprintf(stderr, "[usage] ./sk2cc filename\n");
  exit(0);
}

int main(int argc, char **argv) {
  char *filename = NULL;
  bool only_cpp = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-E") == 0) {
      only_cpp = true;
    } else {
      filename = argv[i];
    }
  }

  if (!filename) {
    print_usage();
  }

  Vector *pp_tokens = tokenize(filename);
  Vector *tokens = preprocess(pp_tokens);

  if (only_cpp) {
    for (int i = 0; i < tokens->length; i++) {
      Token *token = tokens->buffer[i];
      if (token->tk_type == TK_EOF) break;
      printf("%s", token->text);
    }
    exit(0);
  }

  TransUnit *node = parse(tokens);
  gen(node);

  return 0;
}
