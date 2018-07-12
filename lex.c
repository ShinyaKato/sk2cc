#include "cc.h"

Token *token_end;

bool has_next_token = false;
Token *next_token;

Token *token_new() {
  Token *token = (Token *) malloc(sizeof(Token));
  return token;
}

Token *lex() {
  while (1) {
    char c = peek_char();
    if (c != ' ' && c != '\n') break;
    get_char();
  }

  if (peek_char() == EOF) {
    return token_end;
  }

  Token *token = token_new();

  char c = get_char();
  if (isdigit(c)) {
    int n = c - '0';
    while (1) {
      char d = peek_char();
      if (!isdigit(d)) break;
      get_char();
      n = n * 10 + (d - '0');
    }
    token->type = tINT;
    token->int_value = n;
  }  else if (isalpha(c) || c == '_') {
    String *identifier = string_new();
    string_push(identifier, c);
    while (1) {
      char d = peek_char();
      if (!isalnum(d) && d != '_') break;
      get_char();
      string_push(identifier, d);
    }
    token->type = tIDENTIFIER;
    token->identifier = identifier->buffer;
  } else if (c == '~') {
    token->type = tNOT;
  } else if (c == '+') {
    token->type = tADD;
  } else if (c == '-') {
    token->type = tSUB;
  } else if (c == '*') {
    token->type = tMUL;
  } else if (c == '/') {
    token->type = tDIV;
  } else if (c == '%') {
    token->type = tMOD;
  } else if (c == '<') {
    char d = peek_char();
    if (d == '=') {
      token->type = tLTE;
      get_char();
    } else if (d == '<') {
      token->type = tLSHIFT;
      get_char();
    } else {
      token->type = tLT;
    }
  } else if (c == '>') {
    char d = peek_char();
    if (d == '=') {
      token->type = tGTE;
      get_char();
    } else if (d == '>') {
      token->type = tRSHIFT;
      get_char();
    } else {
      token->type = tGT;
    }
  } else if (c == '=') {
    if (peek_char() == '=') {
      token->type = tEQ;
      get_char();
    } else {
      token->type = tASSIGN;
    }
  } else if (c == '!') {
    if (peek_char() == '=') {
      token->type = tNEQ;
      get_char();
    } else {
      token->type = tLNOT;
    }
  } else if (c == '&') {
    if (peek_char() == '&') {
      token->type = tLAND;
      get_char();
    } else {
      token->type = tAND;
    }
  } else if (c == '|') {
    if (peek_char() == '|') {
      token->type = tLOR;
      get_char();
    } else {
      token->type = tOR;
    }
  } else if (c == '^') {
    token->type = tXOR;
  } else if (c == '?') {
    token->type = tQUESTION;
  } else if (c == ':') {
    token->type = tCOLON;
  } else if (c == ';') {
    token->type = tSEMICOLON;
  } else if (c == '(') {
    token->type = tLPAREN;
  } else if (c == ')') {
    token->type = tRPAREN;
  } else {
    error("unexpected character.");
  }

  return token;
}

Token *peek_token() {
  if (has_next_token) {
    return next_token;
  }
  has_next_token = true;
  return next_token = lex();
}

Token *get_token() {
  if (has_next_token) {
    has_next_token = false;
    return next_token;
  }
  return lex();
}

void lex_init() {
  token_end = token_new();
  token_end->type = tEND;
}
