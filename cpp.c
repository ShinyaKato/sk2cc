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

Map *macros;

bool check_object_macro(Token *token) {
  if (token->tk_type != TK_IDENTIFIER) return false;

  Macro *macro = map_lookup(macros, token->identifier);
  return macro && macro->mc_type == OBJECT_MACRO;
}

bool check_function_macro(Token *token) {
  if (token->tk_type != TK_IDENTIFIER) return false;

  Macro *macro = map_lookup(macros, token->identifier);
  return macro && macro->mc_type == FUNCTION_MACRO && check_token('(');
}

Vector *expand_object_macro(Token *token) {
  Macro *macro = map_lookup(macros, token->identifier);
  return macro->replace;
}

Vector *expand_function_macro(Token *token) {
  Macro *macro = map_lookup(macros, token->identifier);

  Map *args = map_new();
  int args_count = 0;

  expect_token('(');
  if (!check_token(')')) {
    do {
      Vector *arg = vector_new();
      int depth = 0;

      read_token(TK_SPACE);
      while (1) {
        Token *token = get_token();
        if (token->tk_type == '(') depth++;
        if (token->tk_type == ')') depth--;

        bool finished = depth == 0 && (check_token(',') || check_token(')'));
        if (token->tk_type == TK_SPACE && finished) break;
        vector_push(arg, token);

        if (finished) break;
      }

      Token *param = macro->params->buffer[args_count++];
      map_put(args, param->identifier, arg);
    } while (read_token(','));
  }
  expect_token(')');

  Scanner *prev = scanner_preserve(macro->replace);

  Vector *result = vector_new();
  while (has_next_token()) {
    Token *token = get_token();
    if (token->tk_type == TK_IDENTIFIER && map_lookup(args, token->identifier)) {
      Vector *arg = map_lookup(args, token->identifier);
      vector_merge(result, arg);
      continue;
    }
    vector_push(result, token);
  }

  scanner_restore(prev);

  return result;
}

Vector *replace_macro(Vector *tokens) {
  Scanner *prev = scanner_preserve(tokens);

  Vector *result = vector_new();
  while (has_next_token()) {
    Token *token = get_token();
    if (check_object_macro(token)) {
      vector_merge(result, expand_object_macro(token));
    } else if (check_function_macro(token)) {
      vector_merge(result, expand_function_macro(token));
    } else {
      vector_push(result, token);
    }
  }

  scanner_restore(prev);

  return result;
}

Vector *preprocessing_unit();

void define_directive() {
  char *identifier = expect_token(TK_IDENTIFIER)->identifier;

  MacroType mc_type;
  Vector *params;
  if (check_token(TK_SPACE) || check_token(TK_NEWLINE)) {
    mc_type = OBJECT_MACRO;
    params = NULL;
  } else if (read_token('(')) {
    mc_type = FUNCTION_MACRO;
    params = vector_new();
    if (!check_token(')')) {
      do {
        read_token(TK_SPACE);
        Token *param = expect_token(TK_IDENTIFIER);
        read_token(TK_SPACE);
        vector_push(params, param);
      } while (read_token(','));
    }
    expect_token(')');
  }

  Vector *replace = vector_new();
  if (!check_token(TK_NEWLINE)) {
    expect_token(TK_SPACE);
    while (!check_token(TK_NEWLINE)) {
      Token *token = get_token();
      if (token->tk_type == TK_SPACE && check_token(TK_NEWLINE)) break;
      vector_push(replace, token);
    }
  }
  expect_token(TK_NEWLINE);

  Macro *macro = (Macro *) calloc(1, sizeof(Macro));
  macro->mc_type = mc_type;
  macro->params = params;
  macro->replace = replace;

  map_put(macros, identifier, macro);
}

Vector *include_directive() {
  char *filename = expect_token(TK_STRING_LITERAL)->string_literal->buffer;
  read_token(TK_SPACE);
  expect_token(TK_NEWLINE);

  Vector *pp_tokens = tokenize(filename);

  Scanner *prev = scanner_preserve(pp_tokens);
  Vector *tokens = preprocessing_unit();
  scanner_restore(prev);

  return tokens;
}

Vector *text_line() {
  Vector *text_tokens = vector_new();

  while (1) {
    Token *token = get_token();
    vector_push(text_tokens, token);

    if (token->tk_type == TK_NEWLINE) {
      if (check_token('#')) break;
      if (check_token(TK_EOF)) break;
    }
  }

  return replace_macro(text_tokens);
}

Vector *group() {
  Vector *tokens = vector_new();

  while (!check_token(TK_EOF)) {
    if (read_token('#')) {
      read_token(TK_SPACE);

      char *directive = expect_token(TK_IDENTIFIER)->identifier;
      read_token(TK_SPACE);

      if (strcmp(directive, "define") == 0) {
        define_directive();
      } else if (strcmp(directive, "include") == 0) {
        Vector *include_tokens = include_directive();
        vector_merge(tokens, include_tokens);
      } else {
        error(peek_token(), "invalid preprocessing directive.");
      }
    } else {
      Vector *text_tokens = text_line();
      vector_merge(tokens, text_tokens);
    }
  }

  return tokens;
}

Vector *preprocessing_unit() {
  Vector *tokens = group();
  return tokens;
}

Vector *preprocess(Vector *pp_tokens) {
  macros = map_new();

  scanner_init(pp_tokens);

  Vector *tokens = preprocessing_unit();
  vector_push(tokens, pp_tokens->buffer[pp_tokens->length - 1]);

  return tokens;
}
