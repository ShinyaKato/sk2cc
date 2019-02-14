#include "sk2cc.h"

typedef enum macro_type {
  OBJECT_MACRO,
  FUNCTION_MACRO
} MacroType;

typedef struct macro {
  MacroType mc_type;
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
  return sc->tokens->buffer[sc->pos];
}

Token *scanner_get(Scanner *sc) {
  return sc->tokens->buffer[sc->pos++];
}

Token *scanner_expect(Scanner *sc, int tk_type) {
  Token *token;
  while (sc->pos < sc->tokens->length) {
    token = scanner_get(sc);
    if (token->tk_type == tk_type) return token;
    if (token->tk_type != TK_SPACE) break;
  }
  error(token, "unexpected token.");
}

bool scanner_check(Scanner *sc, int tk_type) {
  for (int i = sc->pos; i < sc->tokens->length; i++) {
    Token *token = sc->tokens->buffer[i];
    if (token->tk_type == tk_type) return true;
    if (token->tk_type != TK_SPACE) break;
  }
  return false;
}

bool scanner_read(Scanner *sc, int tk_type) {
  if (scanner_check(sc, tk_type)) {
    scanner_expect(sc, tk_type);
    return true;
  }
  return false;
}

bool check_object_macro(Token *token) {
  if (token->tk_type != TK_IDENTIFIER) return false;

  Macro *macro = map_lookup(macros, token->identifier);
  return macro && macro->mc_type == OBJECT_MACRO;
}

bool check_function_macro(Token *token, Scanner *sc) {
  if (token->tk_type != TK_IDENTIFIER) return false;

  Macro *macro = map_lookup(macros, token->identifier);
  return macro && macro->mc_type == FUNCTION_MACRO && scanner_check(sc, '(');
}

Vector *expand_object_macro(Token *token) {
  Macro *macro = map_lookup(macros, token->identifier);
  return macro->replace;
}

Vector *expand_function_macro(Token *token, Scanner *sc) {
  Macro *macro = map_lookup(macros, token->identifier);
  Map *args = map_new();
  int args_count = 0;

  scanner_expect(sc, '(');
  if (!scanner_check(sc, ')')) {
    do {
      Vector *arg = vector_new();
      int depth = 0;

      scanner_read(sc, TK_SPACE);
      while (1) {
        Token *token = scanner_get(sc);
        if (token->tk_type == '(') depth++;
        if (token->tk_type == ')') depth--;

        bool end = depth == 0 && (scanner_check(sc, ',') || scanner_check(sc, ')'));
        if (token->tk_type == TK_SPACE && end) break;
        vector_push(arg, token);
        if (end) break;
      }

      Token *param = macro->params->buffer[args_count++];
      map_put(args, param->identifier, arg);
    } while (scanner_read(sc, ','));
  }
  scanner_expect(sc, ')');

  Vector *result = vector_new();
  Scanner *macro_sc = scanner_new(macro->replace);
  while (macro_sc->pos < macro_sc->tokens->length) {
    Token *token = scanner_get(macro_sc);
    if (token->tk_type == TK_IDENTIFIER && map_lookup(args, token->identifier)) {
      Vector *arg = map_lookup(args, token->identifier);
      vector_merge(result, arg);
      continue;
    }
    vector_push(result, token);
  }

  return result;
}

Vector *replace_macro(Vector *tokens) {
  Scanner *sc = scanner_new(tokens);
  Vector *result = vector_new();

  while (sc->pos < sc->tokens->length) {
    Token *token = scanner_get(sc);
    if (check_object_macro(token)) {
      vector_merge(result, expand_object_macro(token));
    } else if (check_function_macro(token, sc)) {
      vector_merge(result, expand_function_macro(token, sc));
    } else {
      vector_push(result, token);
    }
  }

  return result;
}

Vector *preprocessing_unit(Scanner *sc);

void define_directive(Scanner *sc) {
  char *identifier = scanner_expect(sc, TK_IDENTIFIER)->identifier;

  MacroType mc_type;
  Vector *params;
  if (scanner_peek(sc)->tk_type == TK_SPACE || scanner_peek(sc)->tk_type == TK_NEWLINE) {
    mc_type = OBJECT_MACRO;
  } else if (scanner_peek(sc)->tk_type == '(') {
    scanner_get(sc);
    mc_type = FUNCTION_MACRO;
    params = vector_new();
    if (!scanner_check(sc, ')')) {
      do {
        Token *param = scanner_expect(sc, TK_IDENTIFIER);
        vector_push(params, param);
      } while (scanner_read(sc, ','));
    }
    scanner_expect(sc, ')');
  }

  Vector *replace = vector_new();
  if (!scanner_check(sc, TK_NEWLINE)) {
    scanner_expect(sc, TK_SPACE);
    while (1) {
      Token *token = scanner_get(sc);
      if (token->tk_type == TK_SPACE && scanner_check(sc, TK_NEWLINE)) break;
      vector_push(replace, token);
      if (scanner_check(sc, TK_NEWLINE)) break;
    }
  }
  scanner_expect(sc, TK_NEWLINE);

  Macro *macro = (Macro *) calloc(1, sizeof(Macro));
  macro->mc_type = mc_type;
  macro->params = params;
  macro->replace = replace;

  map_put(macros, identifier, macro);
}

Vector *include_directive(Scanner *sc) {
  char *filename = scanner_expect(sc, TK_STRING_LITERAL)->string_literal->buffer;
  Vector *pp_tokens = tokenize(filename);
  Scanner *next_sc = scanner_new(pp_tokens);
  Vector *tokens = preprocessing_unit(next_sc);
  return tokens;
}

Vector *text_line(Scanner *sc) {
  Vector *line_tokens = vector_new();

  while (1) {
    Token *token = scanner_get(sc);
    vector_push(line_tokens, token);
    if (token->tk_type == TK_NEWLINE) {
      if (scanner_check(sc, '#')) break;
      if (scanner_check(sc, TK_EOF)) break;
    }
  }

  return replace_macro(line_tokens);
}

Vector *group(Scanner *sc) {
  Vector *tokens = vector_new();

  while (!scanner_check(sc, TK_EOF)) {
    if (scanner_read(sc, '#')) {
      Token *directive = scanner_expect(sc, TK_IDENTIFIER);
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

  Vector *tokens = preprocessing_unit(sc);
  vector_push(tokens, pp_tokens->buffer[pp_tokens->length - 1]);

  return tokens;
}
