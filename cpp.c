#include "sk2cc.h"

typedef enum macro_type {
  OBJECT_MACRO,
  FUNCTION_MACRO
} MacroType;

typedef struct macro {
  MacroType mc_type;
  Vector *params;
  bool ellipsis;
  Vector *replace;
  bool expanded;
} Macro;

static Map *macros;

static Vector *replace_macro(Vector *tokens, char *filename, int lineno);
static Vector *preprocessing_unit();

static bool check_file_macro(Token *token) {
  if (token->tk_type == TK_IDENTIFIER) {
    return strcmp(token->identifier, "__FILE__") == 0;
  }

  return false;
}

static bool check_line_macro(Token *token) {
  if (token->tk_type == TK_IDENTIFIER) {
    return strcmp(token->identifier, "__LINE__") == 0;
  }

  return false;
}

static bool check_object_macro(Token *token) {
  if (token->tk_type == TK_IDENTIFIER) {
    Macro *macro = map_lookup(macros, token->identifier);
    return macro && !macro->expanded && macro->mc_type == OBJECT_MACRO;
  }

  return false;
}

static bool check_function_macro(Token *token) {
  if (token->tk_type == TK_IDENTIFIER && has_next_token() && check_token('(')) {
    Macro *macro = map_lookup(macros, token->identifier);
    return macro && !macro->expanded && macro->mc_type == FUNCTION_MACRO;
  }

  return false;
}

static Token *expand_file_macro(Token *token, char *filename) {
  if (!filename) {
    filename = token->loc->filename;
  }

  String *text = string_new();
  string_push(text, '"');
  string_write(text, filename);
  string_push(text, '"');

  String *literal = string_new();
  string_write(literal, filename);
  string_push(literal, '\0');

  Token *str = token_new(TK_STRING_LITERAL, text->buffer, token->loc);
  str->string_literal = literal;
  return str;
}

static Token *expand_line_macro(Token *token, int lineno) {
  if (!lineno) {
    lineno = token->loc->lineno;
  }

  String *text = string_new();
  for (int n = lineno; n > 0; n /= 10) {
    string_push(text, '0' + (n % 10));
  }
  for (int i = 0, j = text->length - 1; i < j; i++, j--) {
    char c = text->buffer[i];
    text->buffer[i] = text->buffer[j];
    text->buffer[j] = c;
  }

  Token *num = token_new(TK_PP_NUMBER, text->buffer, token->loc);
  num->pp_number = text->buffer;
  return num;
}

static Vector *expand_object_macro(Token *token, char *filename, int lineno) {
  if (!filename) {
    filename = token->loc->filename;
  }
  if (!lineno) {
    lineno = token->loc->lineno;
  }

  Macro *macro = map_lookup(macros, token->identifier);

  macro->expanded = true;
  Vector *tokens = replace_macro(macro->replace, filename, lineno);
  macro->expanded = false;

  return tokens;
}

static Vector *expand_function_macro(Token *token, char *filename, int lineno) {
  if (!filename) {
    filename = token->loc->filename;
  }
  if (!lineno) {
    lineno = token->loc->lineno;
  }

  Macro *macro = map_lookup(macros, token->identifier);
  Map *args = map_new();

  read_token(TK_SPACE);
  expect_token('(');
  for (int i = 0; i < macro->params->length; i++) {
    Token *param = macro->params->buffer[i];

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

    map_put(args, param->identifier, arg);

    if (i != macro->params->length - 1) {
      expect_token(',');
    } else {
      if (macro->ellipsis && !check_token(')')) {
        expect_token(',');
      }
    }
  }
  if (macro->ellipsis) {
    Vector *va_args = vector_new();
    int depth = 0;

    read_token(TK_SPACE);
    while (1) {
      Token *token = get_token();
      if (token->tk_type == '(') depth++;
      if (token->tk_type == ')') depth--;

      bool finished = depth == 0 && check_token(')');
      if (token->tk_type == TK_SPACE && finished) break;
      vector_push(va_args, token);
      if (finished) break;
    }

    map_put(args, "__VA_ARGS__", va_args);
  }
  expect_token(')');

  Scanner *prev = scanner_preserve(macro->replace);

  Vector *tokens = vector_new();
  while (has_next_token()) {
    Token *token = get_token();
    if (token->tk_type == TK_IDENTIFIER && map_lookup(args, token->identifier)) {
      Vector *arg = map_lookup(args, token->identifier);
      vector_merge(tokens, arg);
      continue;
    }
    vector_push(tokens, token);
  }

  scanner_restore(prev);

  macro->expanded = true;
  tokens = replace_macro(tokens, filename, lineno);
  macro->expanded = false;

  return tokens;
}

static Vector *replace_macro(Vector *tokens, char *filename, int lineno) {
  Scanner *prev = scanner_preserve(tokens);

  Vector *result = vector_new();
  while (has_next_token()) {
    Token *token = get_token();
    if (check_file_macro(token)) {
      vector_push(result, expand_file_macro(token, filename));
    } else if (check_line_macro(token)) {
      vector_push(result, expand_line_macro(token, lineno));
    } else if (check_object_macro(token)) {
      vector_merge(result, expand_object_macro(token, filename, lineno));
    } else if (check_function_macro(token)) {
      vector_merge(result, expand_function_macro(token, filename, lineno));
    } else {
      vector_push(result, token);
    }
  }

  scanner_restore(prev);

  return result;
}

static void define_directive() {
  char *identifier = expect_token(TK_IDENTIFIER)->identifier;

  MacroType mc_type;
  Vector *params = NULL;
  bool ellipsis = false;
  if (check_token(TK_SPACE) || check_token(TK_NEWLINE)) {
    mc_type = OBJECT_MACRO;
  } else if (read_token('(')) {
    mc_type = FUNCTION_MACRO;
    params = vector_new();
    if (!check_token(')')) {
      do {
        read_token(TK_SPACE);
        if (read_token(TK_ELLIPSIS)) {
          ellipsis = true;
          read_token(TK_SPACE);
          break;
        }
        vector_push(params, expect_token(TK_IDENTIFIER));
        read_token(TK_SPACE);
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
  macro->ellipsis = ellipsis;
  macro->replace = replace;

  map_put(macros, identifier, macro);
}

static Vector *include_directive() {
  char *filename = expect_token(TK_STRING_LITERAL)->string_literal->buffer;
  read_token(TK_SPACE);
  expect_token(TK_NEWLINE);

  Vector *pp_tokens = tokenize(filename);
  vector_pop(pp_tokens);

  Scanner *prev = scanner_preserve(pp_tokens);
  Vector *tokens = preprocessing_unit();
  scanner_restore(prev);

  return tokens;
}

static Vector *text_line() {
  Vector *text_tokens = vector_new();

  while (has_next_token()) {
    Token *token = get_token();
    vector_push(text_tokens, token);

    if (token->tk_type == TK_NEWLINE && check_token('#')) {
      break;
    }
  }

  return replace_macro(text_tokens, NULL, 0);
}

static Vector *group() {
  Vector *tokens = vector_new();

  while (has_next_token()) {
    Token *token = read_token('#');

    if (token) {
      read_token(TK_SPACE);

      // null directive
      if (read_token(TK_NEWLINE)) continue;

      char *directive = expect_token(TK_IDENTIFIER)->identifier;
      read_token(TK_SPACE);

      if (strcmp(directive, "define") == 0) {
        define_directive();
      } else if (strcmp(directive, "include") == 0) {
        vector_merge(tokens, include_directive());
      } else {
        ERROR(token, "invalid preprocessing directive.");
      }
    } else {
      vector_merge(tokens, text_line());
    }
  }

  return tokens;
}

static Vector *preprocessing_unit() {
  return group();
}

// preprocess

Vector *preprocess(Vector *pp_tokens) {
  macros = map_new();

  Token *eof = vector_pop(pp_tokens);
  scanner_init(pp_tokens);

  Vector *tokens = preprocessing_unit();
  vector_push(tokens, eof);

  return tokens;
}
