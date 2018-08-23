#include "cc.h"

int buffer_pos, lineno, column;
char *buffer, *source;

Token *token_new() {
  Token *token = (Token *) calloc(1, sizeof(Token));
  token->lineno = lineno;
  token->column = column;
  token->source = source;
  return token;
}

char peek_char() {
  return buffer[buffer_pos];
}

char get_char() {
  char c = buffer[buffer_pos++];
  column++;
  if (c == '\n') {
    lineno++;
    column = 0;
    source = &buffer[buffer_pos];
  }
  return c;
}

void expect_char(char c) {
  if (peek_char() != c) {
    error(token_new(), "%c is expected.", c);
  }
  get_char();
}

bool read_char(char c) {
  if (peek_char() == c) {
    get_char();
    return true;
  }
  return false;
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
  error(token_new(), "invalid escape sequence.");
}

Token *lex() {
  while (read_char(' '));

  Token *token = token_new();
  if (isdigit(peek_char())) {
    String *s = string_new();
    while (isdigit(peek_char())) {
      string_push(s, get_char());
    }
    if (peek_char() == '.') {
      string_push(s, get_char());
      while (isdigit(peek_char())) {
        string_push(s, get_char());
      }
      token->type = tFLOAT_CONST;
      token->double_value = strtod(s->buffer, NULL);
    } else {
      int int_value = 0;
      for (int i = 0; i < s->length; i++) {
        int_value = int_value * 10 + (s->buffer[i] - '0');
      }
      token->type = tINT_CONST;
      token->int_value = int_value;
    }
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
    } else if (strcmp(identifier->buffer, "double") == 0) {
      token->type = tDOUBLE;
    } else if (strcmp(identifier->buffer, "struct") == 0) {
      token->type = tSTRUCT;
    } else if (strcmp(identifier->buffer, "enum") == 0) {
      token->type = tENUM;
    } else if (strcmp(identifier->buffer, "typedef") == 0) {
      token->type = tTYPEDEF;
    } else if (strcmp(identifier->buffer, "extern") == 0) {
      token->type = tEXTERN;
    } else if (strcmp(identifier->buffer, "_Noreturn") == 0) {
      token->type = tNORETURN;
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
    if (read_char('=')) {
      token->type = tMUL_ASSIGN;
    } else {
      token->type = tMUL;
    }
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
    if (read_char('.')) {
      if (!read_char('.')) {
        error(token, "invalid token '..'");
      }
      token->type = tELLIPSIS;
    } else {
      token->type = tDOT;
    }
  } else if (read_char('#')) {
    token->type = tHASH;
  } else if (read_char('\n')) {
    token->type = tNEWLINE;
  } else if (peek_char() == '\0') {
    token->type = tEND;
  } else {
    error(token, "unexpected character: %c.", peek_char());
  }

  return token;
}

Vector *tokenize(char *input_buffer) {
  buffer_pos = 0;
  lineno = 0;
  column = 0;
  buffer = input_buffer;
  source = input_buffer;

  Vector *pp_tokens = vector_new();
  while (1) {
    Token *pp_token = lex();
    vector_push(pp_tokens, pp_token);
    if (pp_token->type == tEND) break;
  }

  return pp_tokens;
}
