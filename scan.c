#include "sk2cc.h"

Scanner *cur_sc;

void scanner_init(Vector *tokens) {
  cur_sc = calloc(1, sizeof(Scanner));
  cur_sc->tokens = tokens;
  cur_sc->pos = 0;
}

Scanner *scanner_preserve(Vector *tokens) {
  Scanner *sc = cur_sc;
  scanner_init(tokens);
  return sc;
}

void scanner_restore(Scanner *sc) {
  cur_sc = sc;
}

bool has_next_token() {
  return cur_sc->pos < cur_sc->tokens->length;
}

Token *peek_token() {
  return cur_sc->tokens->buffer[cur_sc->pos];
}

Token *get_token() {
  return cur_sc->tokens->buffer[cur_sc->pos++];
}

bool check_token(TokenType tk_type) {
  return peek_token()->tk_type == tk_type;
}

bool check_next_token(TokenType tk_type) {
  if (!has_next_token()) return false;

  Token *token = cur_sc->tokens->buffer[cur_sc->pos + 1];
  return token->tk_type == tk_type;
}

Token *read_token(TokenType tk_type) {
  if (check_token(tk_type)) {
    return get_token();
  }

  return NULL;
}

Token *expect_token(TokenType tk_type) {
  if (check_token(tk_type)) {
    return get_token();
  }

  Token *token = peek_token();
  char *expected = token_name(tk_type);
  char *actual = token->tk_name;
  error(token, "%s is expected, but got %s.", expected, actual);
}
