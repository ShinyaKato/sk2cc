#include "cc.h"

char *token_type_name[] = {
  "tVOID",
  "tBOOL",
  "tCHAR",
  "tINT",
  "tSTRUCT",
  "tENUM",
  "tTYPEDEF",
  "tSIZEOF",
  "tALIGNOF",
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
  "tDOT",
  "tARROW",
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
  "tADD_ASSIGN",
  "tSUB_ASSIGN",
  "tCOMMA",
  "tEND"
};

bool has_next_token = false;
Token *next_token;

Token *token_new() {
  Token *token = (Token *) calloc(1, sizeof(Token));
  return token;
}

char get_escape_sequence() {
  if (read_char('\'')) return '\'';
  if (read_char('"')) return '"';
  if (read_char('?')) return '?';
  if (read_char('\\')) return '\\';
  if (read_char('a')) return '\a';
  if (read_char('b')) return '\b';
  if (read_char('f')) return '\f';
  if (read_char('n')) return '\n';
  if (read_char('r')) return '\r';
  if (read_char('v')) return '\v';
  if (read_char('t')) return '\t';
  if (read_char('0')) return '\0';
  error("invalid escape sequence.");
}

Token *lex() {
  while (peek_char() == ' ' || peek_char() == '\n') {
    get_char();
  }

  if (peek_char() == EOF) {
    Token *token = token_new();
    token->type = tEND;
    return token;
  }

  Token *token = token_new();

  if (isdigit(peek_char())) {
    int int_value = 0;
    while (isdigit(peek_char())) {
      int_value = int_value * 10 + (get_char() - '0');
    }
    token->type = tINT_CONST;
    token->int_value = int_value;
  } else if (read_char('\'')) {
    int int_value;
    if (read_char('\\')) {
      int_value = get_escape_sequence();
    } else {
      int_value = get_char();
    }
    expect_char('\'');
    token->type = tINT_CONST;
    token->int_value = int_value;
  } else if (read_char('"')) {
    String *string_value = string_new();
    while (peek_char() != '"') {
      char c = get_char();
      if (c == '\\') c = get_escape_sequence();
      string_push(string_value, c);
    }
    get_char();
    string_push(string_value, '\0');
    token->type = tSTRING_LITERAL;
    token->string_value = string_value;
  } else if (isalpha(peek_char()) || peek_char() == '_') {
    String *identifier = string_new();
    string_push(identifier, get_char());
    while (isalnum(peek_char()) || peek_char() == '_') {
      string_push(identifier, get_char());
    }
    if (strcmp(identifier->buffer, "void") == 0) {
      token->type = tVOID;
    } else if (strcmp(identifier->buffer, "_Bool") == 0) {
      token->type = tBOOL;
    } else if (strcmp(identifier->buffer, "char") == 0) {
      token->type = tCHAR;
    } else if (strcmp(identifier->buffer, "int") == 0) {
      token->type = tINT;
    } else if (strcmp(identifier->buffer, "struct") == 0) {
      token->type = tSTRUCT;
    } else if (strcmp(identifier->buffer, "enum") == 0) {
      token->type = tENUM;
    } else if (strcmp(identifier->buffer, "typedef") == 0) {
      token->type = tTYPEDEF;
    } else if (strcmp(identifier->buffer, "sizeof") == 0) {
      token->type = tSIZEOF;
    } else if (strcmp(identifier->buffer, "_Alignof") == 0) {
      token->type = tALIGNOF;
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
  } else if (read_char('~')) {
    token->type = tNOT;
  } else if (read_char('+')) {
    if (read_char('+')) {
      token->type = tINC;
    } else if (read_char('=')) {
      token->type = tADD_ASSIGN;
    } else {
      token->type = tADD;
    }
  } else if (read_char('-')) {
    if (read_char('>')) {
      token->type = tARROW;
    } else if (read_char('-')) {
      token->type = tDEC;
    } else if (read_char('=')) {
      token->type = tSUB_ASSIGN;
    } else {
      token->type = tSUB;
    }
  } else if (read_char('*')) {
    token->type = tMUL;
  } else if (read_char('/')) {
    token->type = tDIV;
  } else if (read_char('%')) {
    token->type = tMOD;
  } else if (read_char('<')) {
    if (read_char('=')) {
      token->type = tLTE;
    } else if (read_char('<')) {
      token->type = tLSHIFT;
    } else {
      token->type = tLT;
    }
  } else if (read_char('>')) {
    if (read_char('=')) {
      token->type = tGTE;
    } else if (read_char('>')) {
      token->type = tRSHIFT;
    } else {
      token->type = tGT;
    }
  } else if (read_char('=')) {
    if (read_char('=')) {
      token->type = tEQ;
    } else {
      token->type = tASSIGN;
    }
  } else if (read_char('!')) {
    if (read_char('=')) {
      token->type = tNEQ;
    } else {
      token->type = tLNOT;
    }
  } else if (read_char('&')) {
    if (read_char('&')) {
      token->type = tLAND;
    } else {
      token->type = tAND;
    }
  } else if (read_char('|')) {
    if (read_char('|')) {
      token->type = tLOR;
    } else {
      token->type = tOR;
    }
  } else if (read_char('^')) {
    token->type = tXOR;
  } else if (read_char('?')) {
    token->type = tQUESTION;
  } else if (read_char(':')) {
    token->type = tCOLON;
  } else if (read_char(';')) {
    token->type = tSEMICOLON;
  } else if (read_char('[')) {
    token->type = tLBRACKET;
  } else if (read_char(']')) {
    token->type = tRBRACKET;
  } else if (read_char('(')) {
    token->type = tLPAREN;
  } else if (read_char(')')) {
    token->type = tRPAREN;
  } else if (read_char('{')) {
    token->type = tLBRACE;
  } else if (read_char('}')) {
    token->type = tRBRACE;
  } else if (read_char(',')) {
    token->type = tCOMMA;
  } else if (read_char('.')) {
    token->type = tDOT;
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

Token *optional_token(TokenType type) {
  if (peek_token()->type == type) {
    return get_token();
  }
  return NULL;
}

bool read_token(TokenType type) {
  bool equal = peek_token()->type == type;
  if (equal) get_token();
  return equal;
}
