#include "cc.h"

char token_type_name[][32] = {
  "tINT",
  "tCHAR",
  "tIF",
  "tELSE",
  "tWHILE",
  "tDO",
  "tFOR",
  "tCONTINUE",
  "tBREAK",
  "tRETURN",
  "tIDENTIFIER",
  "tINT_CONST",
  "tSTRING_LITERAL",
  "tLBRACKET",
  "tRBRACKET",
  "tLPAREN",
  "tRPAREN",
  "tRBRACE",
  "tLBRACE",
  "tINC",
  "tDEC",
  "tNOT",
  "tLNOT",
  "tMUL",
  "tDIV",
  "tMOD",
  "tADD",
  "tSUB",
  "tLSHIFT",
  "tRSHIFT",
  "tLT",
  "tGT",
  "tLTE",
  "tGTE",
  "tEQ",
  "tNEQ",
  "tAND",
  "tXOR",
  "tOR",
  "tLAND",
  "tLOR",
  "tQUESTION",
  "tCOLON",
  "tSEMICOLON",
  "tASSIGN",
  "tCOMMA",
  "tEND"
};

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
    token->type = tINT_CONST;
    token->int_value = n;
  } else if (c == '"') {
    String *str = string_new();
    while (1) {
      char d = get_char();
      if (d == '"') break;
      string_push(str, d);
    }
    token->type = tSTRING_LITERAL;
    token->string_value = str;
  } else if (isalpha(c) || c == '_') {
    String *identifier = string_new();
    string_push(identifier, c);
    while (1) {
      char d = peek_char();
      if (!isalnum(d) && d != '_') break;
      get_char();
      string_push(identifier, d);
    }
    if (strcmp(identifier->buffer, "char") == 0) {
      token->type = tCHAR;
    } else if (strcmp(identifier->buffer, "int") == 0) {
      token->type = tINT;
    } else if (strcmp(identifier->buffer, "if") == 0) {
      token->type = tIF;
    } else if (strcmp(identifier->buffer, "else") == 0) {
      token->type = tELSE;
    } else if (strcmp(identifier->buffer, "while") == 0) {
      token->type = tWHILE;
    } else if (strcmp(identifier->buffer, "do") == 0) {
      token->type = tDO;
    } else if (strcmp(identifier->buffer, "for") == 0) {
      token->type = tFOR;
    } else if (strcmp(identifier->buffer, "continue") == 0) {
      token->type = tCONTINUE;
    } else if (strcmp(identifier->buffer, "break") == 0) {
      token->type = tBREAK;
    } else if (strcmp(identifier->buffer, "return") == 0) {
      token->type = tRETURN;
    } else {
      token->type = tIDENTIFIER;
      token->identifier = identifier->buffer;
    }
  } else if (c == '~') {
    token->type = tNOT;
  } else if (c == '+') {
    if (peek_char() == '+') {
      token->type = tINC;
      get_char();
    } else {
      token->type = tADD;
    }
  } else if (c == '-') {
    if (peek_char() == '-') {
      token->type = tDEC;
      get_char();
    } else {
      token->type = tSUB;
    }
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
  } else if (c == '[') {
    token->type = tLBRACKET;
  } else if (c == ']') {
    token->type = tRBRACKET;
  } else if (c == '(') {
    token->type = tLPAREN;
  } else if (c == ')') {
    token->type = tRPAREN;
  } else if (c == '{') {
    token->type = tLBRACE;
  } else if (c == '}') {
    token->type = tRBRACE;
  } else if (c == ',') {
    token->type = tCOMMA;
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

Token *expect_token(TokenType type) {
  Token *token = get_token();
  if (token->type != type) {
    error("%s is expected.", token_type_name[type]);
  }
  return token;
}

bool read_token(TokenType type) {
  bool equal = peek_token()->type == type;
  if (equal) get_token();
  return equal;
}

void lex_init() {
  token_end = token_new();
  token_end->type = tEND;
}
