#include "sk2cc.h"

typedef enum macro_type {
  OBJECT_MACRO,
  FUNCTION_MACRO
} MacroType;

typedef struct macro {
  MacroType type;
  Vector *params;
  Vector *replace;
} Macro;

typedef struct scanner {
  Vector *tokens;
  int pos;
} Scanner;

Map *macros;

Scanner *scanner_new(Vector *tokens) {
  Scanner *sc = (Scanner *) calloc(1, sizeof(Scanner));
  sc->tokens = tokens;
  sc->pos = 0;
  return sc;
}

Token *scanner_peek(Scanner *sc) {
  return sc->tokens->array[sc->pos];
}

Token *scanner_get(Scanner *sc) {
  return sc->tokens->array[sc->pos++];
}

Token *scanner_expect(Scanner *sc, TokenType type) {
  Token *token;
  while (sc->pos < sc->tokens->length) {
    token = scanner_get(sc);
    if (token->type == type) return token;
    if (token->type != tSPACE) break;
  }
  error(token, "unexpected token.");
}

bool scanner_check(Scanner *sc, TokenType type) {
  for (int i = sc->pos; i < sc->tokens->length; i++) {
    Token *token = sc->tokens->array[i];
    if (token->type == type) return true;
    if (token->type != tSPACE) break;
  }
  return false;
}

bool scanner_read(Scanner *sc, TokenType type) {
  if (scanner_check(sc, type)) {
    scanner_expect(sc, type);
    return true;
  }
  return false;
}

bool check_object_macro(Token *token) {
  if (token->type != tIDENTIFIER) return false;

  Macro *macro = map_lookup(macros, token->identifier);
  return macro && macro->type == OBJECT_MACRO;
}

bool check_function_macro(Token *token, Scanner *sc) {
  if (token->type != tIDENTIFIER) return false;

  Macro *macro = map_lookup(macros, token->identifier);
  return macro && macro->type == FUNCTION_MACRO && scanner_check(sc, tLPAREN);
}

Vector *replace_macro(Vector *tokens) {
  Scanner *sc = scanner_new(tokens);
  Vector *result = vector_new();

  while (sc->pos < sc->tokens->length) {
    Token *token = scanner_get(sc);
    if (check_object_macro(token)) {
      Macro *macro = map_lookup(macros, token->identifier);
      vector_merge(result, macro->replace);
    } else if (check_function_macro(token, sc)) {
      Macro *macro = map_lookup(macros, token->identifier);
      Map *args = map_new();
      int args_count = 0;
      scanner_expect(sc, tLPAREN);
      if (!scanner_check(sc, tRPAREN)) {
        do {
          Vector *arg = vector_new();
          scanner_read(sc, tSPACE);
          while (1) {
            if (scanner_check(sc, tCOMMA) || scanner_check(sc, tRPAREN)) break;
            Token *token = scanner_get(sc);
            vector_push(arg, token);
          }
          Token *param = macro->params->array[args_count++];
          map_put(args, param->identifier, arg);
        } while (scanner_read(sc, tCOMMA));
      }
      scanner_expect(sc, tRPAREN);
      Scanner *macro_sc = scanner_new(macro->replace);
      while (macro_sc->pos < macro_sc->tokens->length) {
        Token *token = scanner_get(macro_sc);
        if (token->type == tIDENTIFIER && map_lookup(args, token->identifier)) {
          Vector *arg = map_lookup(args, token->identifier);
          vector_merge(result, arg);
          continue;
        }
        vector_push(result, token);
      }
    } else {
      vector_push(result, token);
    }
  }

  return result;
}

Vector *preprocessing_unit(Scanner *sc);

void define_directive(Scanner *sc) {
  char *identifier = scanner_expect(sc, tIDENTIFIER)->identifier;

  MacroType type;
  Vector *params;
  if (scanner_peek(sc)->type == tSPACE) {
    type = OBJECT_MACRO;
  } else if (scanner_peek(sc)->type == tLPAREN) {
    scanner_get(sc);
    type = FUNCTION_MACRO;
    params = vector_new();
    if (!scanner_check(sc, tRPAREN)) {
      do {
        Token *param = scanner_expect(sc, tIDENTIFIER);
        vector_push(params, param);
      } while (scanner_read(sc, tCOMMA));
    }
    scanner_expect(sc, tRPAREN);
  }
  scanner_expect(sc, tSPACE);

  Vector *replace = vector_new();
  while (1) {
    Token *token = scanner_get(sc);
    if (token->type == tSPACE && scanner_check(sc, tNEWLINE)) break;
    vector_push(replace, token);
    if (scanner_check(sc, tNEWLINE)) break;
  }
  scanner_expect(sc, tNEWLINE);

  Macro *macro = (Macro *) calloc(1, sizeof(Macro));
  macro->type = type;
  macro->params = params;
  macro->replace = replace;

  map_put(macros, identifier, macro);
}

Vector *include_directive(Scanner *sc) {
  char *filename = scanner_expect(sc, tSTRING_LITERAL)->string_value->buffer;
  Vector *src = scan(filename);
  Vector *pp_tokens = tokenize(src);
  Scanner *next_sc = scanner_new(pp_tokens);
  Vector *tokens = preprocessing_unit(next_sc);
  return tokens;
}

Vector *text_line(Scanner *sc) {
  Vector *line_tokens = vector_new();

  while (1) {
    Token *token = scanner_get(sc);
    vector_push(line_tokens, token);
    if (token->type == tNEWLINE) {
      if (scanner_check(sc, tHASH)) break;
      if (scanner_check(sc, tEND)) break;
    }
  }

  return replace_macro(line_tokens);
}

Vector *group(Scanner *sc) {
  Vector *tokens = vector_new();

  while (!scanner_check(sc, tEND)) {
    if (scanner_read(sc, tHASH)) {
      Token *directive = scanner_expect(sc, tIDENTIFIER);
      if (strcmp(directive->identifier, "define") == 0) {
        define_directive(sc);
      } else if (strcmp(directive->identifier, "include") == 0) {
        Vector *include_tokens = include_directive(sc);
        vector_merge(tokens, include_tokens);
      } else {
        error(scanner_peek(sc), "invalid preprocessing directive.");
      }
    } else {
      Vector *line_tokens = text_line(sc);
      vector_merge(tokens, line_tokens);
    }
  }

  return tokens;
}

Vector *preprocessing_unit(Scanner *sc) {
  Vector *tokens = group(sc);
  return tokens;
}

Vector *preprocess(Vector *pp_tokens) {
  macros = map_new();

  Scanner *sc = scanner_new(pp_tokens);

  Vector *_tokens = preprocessing_unit(sc);
  vector_push(_tokens, pp_tokens->array[pp_tokens->length - 1]);

  Vector *tokens = vector_new();
  for (int i = 0; i < _tokens->length; i++) {
    Token *token = _tokens->array[i];
    if (token->type == tSPACE || token->type == tNEWLINE) continue;
    vector_push(tokens, token);
  }

  return tokens;
}
