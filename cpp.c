#include "cc.h"

void directives(Vector *process_token, Vector *token_vector) {
  bool newline = true;

  for (int i = 0; i < token_vector->length; i++) {
    Token *token = token_vector->array[i];

    if (token->type == tNEWLINE) {
      newline = true;
      continue;
    }

    if (newline && token->type == tHASH) {
      i++;
      Token *directive = token_vector->array[i];

      if (directive->type == tIDENTIFIER && strcmp(directive->identifier, "include") == 0) {
        i++;
        Token *header = token_vector->array[i];

        if (header->type == tSTRING_LITERAL) {
          char *file = header->string_value->buffer;
          char *src = read_source(file);
          Vector *tokens = lexical_analyze(src);
          directives(process_token, tokens);
          vector_pop(process_token);
          continue;
        } else {
          while (1) {
            Token *next = token_vector->array[i++];
            if (next->type == tNEWLINE) break;
          }
          continue;
        }
      }
    }

    vector_push(process_token, token);
    newline = false;
  }
}

Vector *preprocess(Vector *token_vector) {
  Vector *process_tokens = vector_new();
  directives(process_tokens, token_vector);

  return process_tokens;
}
