#include "cc.h"

void compile(char *input, bool cpp) {
  Vector *pp_tokens = tokenize(input);
  Vector *tokens = preprocess(pp_tokens);

  if (cpp) {
    for (int i = 0; i < tokens->length; i++) {
      Token *token = tokens->buffer[i];
      if (token->tk_type == TK_EOF) break;
      printf("%s", token->text);
    }
    exit(0);
  }

  TransUnit *trans_unit = parse(tokens);
  sema(trans_unit);
  alloc(trans_unit);
  gen(trans_unit);
}
