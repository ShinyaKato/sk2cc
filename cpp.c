#include "sk2cc.h"

Map *macros;
Vector *pos_stack;
Vector *source_stack;
int stack_size;

void push_source(Vector *pp_tokens) {
  int *pos = (int *) calloc(1, sizeof(int));
  vector_push(pos_stack, pos);
  vector_push(source_stack, pp_tokens);
  stack_size++;
}

void pop_source() {
  vector_pop(source_stack);
  vector_pop(pos_stack);
  stack_size--;
}

Token *peek_pp_token() {
  int *pos = pos_stack->array[stack_size - 1];
  Vector *pp_tokens = source_stack->array[stack_size - 1];
  Token *token = pp_tokens->array[*pos];
  return token;
}

Token *get_pp_token() {
  int *pos = pos_stack->array[stack_size - 1];
  Vector *pp_tokens = source_stack->array[stack_size - 1];
  Token *token = pp_tokens->array[*pos];
  *pos = *pos + 1;
  return token;
}

Token *expect_pp_token(TokenType type) {
  Token *token = get_pp_token();
  if (token->type != type) {
    error(token, "unexpected token.");
  }
  return token;
}

bool check_pp_token(TokenType type) {
  return peek_pp_token()->type == type;
}

bool read_pp_token(TokenType type) {
  if (peek_pp_token()->type == type) {
    get_pp_token();
    return true;
  }
  return false;
}

bool read_directive(char *directive) {
  if (check_pp_token(tIDENTIFIER)) {
    Token *token = peek_pp_token();
    if (strcmp(token->identifier, directive) == 0) {
      get_pp_token();
      read_pp_token(tSPACE);
      return true;
    }
  }
  return false;
}

bool check_object_macro(Vector *tokens, int p) {
  Token *token = tokens->array[p];
  if (token->type != tIDENTIFIER) return false;

  Macro *macro = map_lookup(macros, token->identifier);
  return macro && macro->type == OBJECT_MACRO;
}

bool check_function_macro(Vector *tokens, int p) {
  Token *token = tokens->array[p];
  if (token->type != tIDENTIFIER) return false;

  Macro *macro = map_lookup(macros, token->identifier);
  if (macro && macro->type == FUNCTION_MACRO) {
    for (int i = p + 1; i < tokens->length; i++) {
      Token *next = tokens->array[i];
      if (next->type == tLPAREN) return true;
      if (next->type != tSPACE && next->type != tNEWLINE) break;
    }
  }

  return false;
}

Vector *replace_macro(Vector *tokens) {
  Vector *result = vector_new();

  for (int i = 0; i < tokens->length; i++) {
    Token *token = tokens->array[i];
    if (check_object_macro(tokens, i)) {
      Macro *macro = map_lookup(macros, token->identifier);
      vector_merge(result, macro->replace);
    } else if (check_function_macro(tokens, i)) {
      Macro *macro = map_lookup(macros, token->identifier);
      for (i++; i < tokens->length; i++) {
        Token *token = tokens->array[i];
        if (token->type == tLPAREN) break;
      }
      for (i++; i < tokens->length; i++) {
        Token *token = tokens->array[i];
        if (token->type == tRPAREN) break;
      }
      vector_merge(result, macro->replace);
    } else {
      vector_push(result, token);
    }
  }

  return result;
}

Vector *preprocessing_unit();

void define_directive() {
  MacroType type = OBJECT_MACRO;
  char *identifier = expect_pp_token(tIDENTIFIER)->identifier;

  Vector *params;
  if (read_pp_token(tLPAREN)) {
    type = FUNCTION_MACRO;
    params = vector_new();
    expect_pp_token(tRPAREN);
  }
  expect_pp_token(tSPACE);

  Vector *replace = vector_new();
  while (1) {
    Token *token = get_pp_token();
    if (token->type == tNEWLINE) break;
    vector_push(replace, token);
  }
  Token *last = replace->array[replace->length - 1];
  if (last->type == tSPACE) vector_pop(replace);

  Macro *macro = (Macro *) calloc(1, sizeof(Macro));
  macro->type = type;
  macro->params = params;
  macro->replace = replace;

  map_put(macros, identifier, macro);
}

Vector *include_directive() {
  char *filename = expect_pp_token(tSTRING_LITERAL)->string_value->buffer;
  Vector *src = scan(filename);
  Vector *pp_tokens = tokenize(src);
  push_source(pp_tokens);
  Vector *tokens = preprocessing_unit();
  pop_source();
  return tokens;
}

Vector *text_line() {
  Vector *line_tokens = vector_new();

  while (1) {
    Token *token = get_pp_token();
    vector_push(line_tokens, token);
    if (token->type == tNEWLINE) {
      if (check_pp_token(tHASH)) break;
      if (check_pp_token(tEND)) break;
    }
  }

  return replace_macro(line_tokens);
}

Vector *group() {
  Vector *tokens = vector_new();

  while (peek_pp_token()->type != tEND) {
    if (read_pp_token(tHASH)) {
      read_pp_token(tSPACE);
      if (read_directive("define")) {
        define_directive();
      } else if (read_directive("include")) {
        Vector *include_tokens = include_directive();
        vector_merge(tokens, include_tokens);
      } else {
        error(peek_pp_token(), "unknown preprocessing directive.");
      }
    } else {
      Vector *line_tokens = text_line();
      vector_merge(tokens, line_tokens);
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

  source_stack = vector_new();
  pos_stack = vector_new();
  push_source(pp_tokens);

  Vector *_tokens = preprocessing_unit();
  vector_push(_tokens, pp_tokens->array[pp_tokens->length - 1]);

  Vector *tokens = vector_new();
  for (int i = 0; i < _tokens->length; i++) {
    Token *token = _tokens->array[i];
    if (token->type == tSPACE || token->type == tNEWLINE) continue;
    vector_push(tokens, token);
  }

  return tokens;
}
