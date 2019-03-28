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

// tokens

static Vector *stash_tokens, *stash_pos;
static Token **tokens;
static int pos;

static bool has_next() {
  return tokens[pos] != NULL;
}

static Token *get() {
  return tokens[pos++];
}

static Token *check(TokenType tk_type) {
  if (tokens[pos]->tk_type == tk_type) {
    return tokens[pos];
  }
  return NULL;
}

static Token *read(TokenType tk_type) {
  if (tokens[pos]->tk_type == tk_type) {
    return tokens[pos++];
  }
  return NULL;
}

static Token *expect(TokenType tk_type) {
  if (tokens[pos]->tk_type == tk_type) {
    return tokens[pos++];
  }

  if (isprint(tk_type)) {
    ERROR(tokens[pos], "'%c' is expected.", tk_type);
  }
  ERROR(tokens[pos], "unexpected token.");
}

static void stash(Vector *_tokens) {
  vector_push(stash_tokens, tokens);
  vector_pushi(stash_pos, pos);
  tokens = (Token **) _tokens->buffer;
  pos = 0;
}

static void restore() {
  tokens = vector_pop(stash_tokens);
  pos = vector_popi(stash_pos);
}

// macro replacement

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
  if (token->tk_type == TK_IDENTIFIER && has_next() && check('(')) {
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

  Token *str = calloc(1, sizeof(Token));
  str->tk_type = TK_STRING_LITERAL;
  str->text = text->buffer;
  str->loc = token->loc;
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

  Token *num = calloc(1, sizeof(Token));
  num->tk_type = TK_PP_NUMBER;
  num->text = text->buffer;
  num->loc = token->loc;
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

  read(TK_SPACE);
  expect('(');
  for (int i = 0; i < macro->params->length; i++) {
    Token *param = macro->params->buffer[i];

    Vector *arg = vector_new();
    int depth = 0;

    read(TK_SPACE);
    while (1) {
      Token *token = get();
      if (token->tk_type == '(') depth++;
      if (token->tk_type == ')') depth--;

      bool finished = depth == 0 && (check(',') || check(')'));
      if (token->tk_type == TK_SPACE && finished) break;
      vector_push(arg, token);
      if (finished) break;
    }

    map_put(args, param->identifier, arg);

    if (i != macro->params->length - 1) {
      expect(',');
    } else {
      if (macro->ellipsis && !check(')')) {
        expect(',');
      }
    }
  }
  if (macro->ellipsis) {
    Vector *va_args = vector_new();
    int depth = 0;

    read(TK_SPACE);
    while (1) {
      Token *token = get();
      if (token->tk_type == '(') depth++;
      if (token->tk_type == ')') depth--;

      bool finished = depth == 0 && check(')');
      if (token->tk_type == TK_SPACE && finished) break;
      vector_push(va_args, token);
      if (finished) break;
    }

    map_put(args, "__VA_ARGS__", va_args);
  }
  expect(')');

  stash(macro->replace);

  Vector *tokens = vector_new();
  while (has_next()) {
    Token *token = get();
    if (token->tk_type == TK_IDENTIFIER && map_lookup(args, token->identifier)) {
      Vector *arg = map_lookup(args, token->identifier);
      vector_merge(tokens, arg);
      continue;
    }
    vector_push(tokens, token);
  }

  restore();

  macro->expanded = true;
  tokens = replace_macro(tokens, filename, lineno);
  macro->expanded = false;

  return tokens;
}

static Vector *replace_macro(Vector *tokens, char *filename, int lineno) {
  stash(tokens);

  Vector *result = vector_new();
  while (has_next()) {
    Token *token = get();
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

  restore();

  return result;
}

// directives

static void define_directive() {
  char *identifier = expect(TK_IDENTIFIER)->identifier;

  MacroType mc_type;
  Vector *params = NULL;
  bool ellipsis = false;
  if (check(TK_SPACE) || check(TK_NEWLINE)) {
    mc_type = OBJECT_MACRO;
  } else if (read('(')) {
    mc_type = FUNCTION_MACRO;
    params = vector_new();
    if (!check(')')) {
      do {
        read(TK_SPACE);
        if (read(TK_ELLIPSIS)) {
          ellipsis = true;
          read(TK_SPACE);
          break;
        }
        vector_push(params, expect(TK_IDENTIFIER));
        read(TK_SPACE);
      } while (read(','));
    }
    expect(')');
  }

  Vector *replace = vector_new();
  if (!check(TK_NEWLINE)) {
    expect(TK_SPACE);
    while (!check(TK_NEWLINE)) {
      Token *token = get();
      if (token->tk_type == TK_SPACE && check(TK_NEWLINE)) break;
      vector_push(replace, token);
    }
  }
  expect(TK_NEWLINE);

  Macro *macro = (Macro *) calloc(1, sizeof(Macro));
  macro->mc_type = mc_type;
  macro->params = params;
  macro->ellipsis = ellipsis;
  macro->replace = replace;

  map_put(macros, identifier, macro);
}

static Vector *include_directive() {
  char *filename = expect(TK_STRING_LITERAL)->string_literal->buffer;
  read(TK_SPACE);
  expect(TK_NEWLINE);

  Vector *pp_tokens = tokenize(filename);
  vector_pop(pp_tokens);

  stash(pp_tokens);

  Vector *tokens = preprocessing_unit();

  restore();

  return tokens;
}

static Vector *text_line() {
  Vector *text_tokens = vector_new();

  while (has_next()) {
    Token *token = get();
    vector_push(text_tokens, token);

    if (token->tk_type == TK_NEWLINE) {
      if (!has_next()) break;
      if (check('#')) break;
    }
  }

  return replace_macro(text_tokens, NULL, 0);
}

static Vector *group() {
  Vector *tokens = vector_new();

  while (has_next()) {
    Token *token = read('#');

    if (token) {
      read(TK_SPACE);

      // null directive
      if (read(TK_NEWLINE)) continue;

      char *directive = expect(TK_IDENTIFIER)->identifier;
      read(TK_SPACE);

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

Vector *preprocess(Vector *_tokens) {
  Token *eof = vector_pop(_tokens);

  macros = map_new();

  stash_tokens = vector_new();
  stash_pos = vector_new();
  tokens = (Token **) _tokens->buffer;
  pos = 0;

  Vector *tokens = preprocessing_unit();
  vector_push(tokens, eof);

  return tokens;
}
