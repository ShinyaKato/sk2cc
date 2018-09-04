#include "sk2cc.h"

Map *macros;

void directives(Vector *tokens, Vector *pp_tokens) {
  bool newline = true;

  for (int i = 0; i < pp_tokens->length; i++) {
    Token *token = pp_tokens->array[i];

    if (token->type == tNEWLINE) {
      newline = true;
      continue;
    }

    if (newline && token->type == tHASH) {
      i++;
      Token *directive = pp_tokens->array[i];

      if (directive->type == tIDENTIFIER && strcmp(directive->identifier, "include") == 0) {
        i++;
        Token *header = pp_tokens->array[i];

        if (header->type == tSTRING_LITERAL) {
          char *filename = header->string_value->buffer;
          char *buffer = scan(filename);
          Vector *include_pp_tokens = tokenize(buffer);
          directives(tokens, include_pp_tokens);
          vector_pop(tokens);
          continue;
        } else {
          while (1) {
            Token *next = pp_tokens->array[i++];
            if (next->type == tNEWLINE) break;
          }
          continue;
        }
      } else if (directive->type == tIDENTIFIER && strcmp(directive->identifier, "define") == 0) {
        i++;
        Token *id = pp_tokens->array[i++];

        Vector *sequence = vector_new();
        while (1) {
          Token *next = pp_tokens->array[i];
          if (next->type == tNEWLINE) break;
          i++;
          vector_push(sequence, next);
        }
        map_put(macros, id->identifier, sequence);

        continue;
      }
    }

    if (token->type == tIDENTIFIER && map_lookup(macros, token->identifier)) {
      Vector *sequence = map_lookup(macros, token->identifier);
      for (int j = 0; j < sequence->length; j++) {
        vector_push(tokens, sequence->array[j]);
      }
    } else {
      vector_push(tokens, token);
    }
    newline = false;
  }
}

Vector *preprocess(Vector *pp_tokens) {
  macros = map_new();

  Vector *tokens = vector_new();
  directives(tokens, pp_tokens);

  return tokens;
}
