#include "sk2cc.h"

char *tk_name[] = {
  "void",
  "bool",
  "char",
  "short",
  "int",
  "unsigned",
  "struct",
  "enum",
  "typedef",
  "extern",
  "_Noreturn",
  "sizeof",
  "_Alignof",
  "if",
  "else",
  "while",
  "do",
  "for",
  "continue",
  "break",
  "return",
  "identifier",
  "integer constant",
  "string literal",
  "[",
  "]",
  "(",
  ")",
  "{",
  "}",
  ".",
  "->",
  "++",
  "--",
  "~",
  "!",
  "*",
  "/",
  "%",
  "+",
  "-",
  "<<",
  ">>",
  "<",
  ">",
  "<=",
  ">=",
  "==",
  "!=",
  "&",
  "^",
  "|",
  "&&",
  "||",
  "?",
  ":",
  ";",
  "...",
  "=",
  "+=",
  "-=",
  "*=",
  "/=",
  "%=",
  ",",
  "#",
  "space",
  "new line",
  "end of file"
};

Vector *src;
int src_pos;

Token *token_new() {
  Token *token = (Token *) calloc(1, sizeof(Token));
  token->schar = (SourceChar **) &(src->buffer[src_pos]);
  return token;
}

char peek_char() {
  SourceChar *schar = src->buffer[src_pos];
  return *(schar->char_ptr);
}

char get_char() {
  char c = peek_char();
  src_pos++;
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
  Token *token = token_new();
  if (isdigit(peek_char())) {
    String *s = string_new();
    while (isdigit(peek_char())) {
      string_push(s, get_char());
    }
    int int_value = 0;
    for (int i = 0; i < s->length; i++) {
      int_value = int_value * 10 + (s->buffer[i] - '0');
    }
    token->tk_type = tINT_CONST;
    token->int_value = int_value;
  } else if (read_char('\'')) {
    int int_value;
    if (read_char('\\')) {
      int_value = get_escape_sequence();
    } else {
      int_value = get_char();
    }
    expect_char('\'');
    token->tk_type = tINT_CONST;
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
    token->tk_type = tSTRING_LITERAL;
    token->string_value = string_value;
  } else if (isalpha(peek_char()) || peek_char() == '_') {
    String *identifier = string_new();
    string_push(identifier, get_char());
    while (isalnum(peek_char()) || peek_char() == '_') {
      string_push(identifier, get_char());
    }
    if (strcmp(identifier->buffer, "void") == 0) {
      token->tk_type = tVOID;
    } else if (strcmp(identifier->buffer, "_Bool") == 0) {
      token->tk_type = tBOOL;
    } else if (strcmp(identifier->buffer, "char") == 0) {
      token->tk_type = tCHAR;
    } else if (strcmp(identifier->buffer, "short") == 0) {
      token->tk_type = tSHORT;
    } else if (strcmp(identifier->buffer, "int") == 0) {
      token->tk_type = tINT;
    } else if (strcmp(identifier->buffer, "unsigned") == 0) {
      token->tk_type = tUNSIGNED;
    } else if (strcmp(identifier->buffer, "struct") == 0) {
      token->tk_type = tSTRUCT;
    } else if (strcmp(identifier->buffer, "enum") == 0) {
      token->tk_type = tENUM;
    } else if (strcmp(identifier->buffer, "typedef") == 0) {
      token->tk_type = tTYPEDEF;
    } else if (strcmp(identifier->buffer, "extern") == 0) {
      token->tk_type = tEXTERN;
    } else if (strcmp(identifier->buffer, "_Noreturn") == 0) {
      token->tk_type = tNORETURN;
    } else if (strcmp(identifier->buffer, "sizeof") == 0) {
      token->tk_type = tSIZEOF;
    } else if (strcmp(identifier->buffer, "_Alignof") == 0) {
      token->tk_type = tALIGNOF;
    } else if (strcmp(identifier->buffer, "if") == 0) {
      token->tk_type = tIF;
    } else if (strcmp(identifier->buffer, "else") == 0) {
      token->tk_type = tELSE;
    } else if (strcmp(identifier->buffer, "while") == 0) {
      token->tk_type = tWHILE;
    } else if (strcmp(identifier->buffer, "do") == 0) {
      token->tk_type = tDO;
    } else if (strcmp(identifier->buffer, "for") == 0) {
      token->tk_type = tFOR;
    } else if (strcmp(identifier->buffer, "continue") == 0) {
      token->tk_type = tCONTINUE;
    } else if (strcmp(identifier->buffer, "break") == 0) {
      token->tk_type = tBREAK;
    } else if (strcmp(identifier->buffer, "return") == 0) {
      token->tk_type = tRETURN;
    } else {
      token->tk_type = tIDENTIFIER;
      token->identifier = identifier->buffer;
    }
  } else if (read_char('~')) {
    token->tk_type = tNOT;
  } else if (read_char('+')) {
    if (read_char('+')) {
      token->tk_type = tINC;
    } else if (read_char('=')) {
      token->tk_type = tADD_ASSIGN;
    } else {
      token->tk_type = tADD;
    }
  } else if (read_char('-')) {
    if (read_char('>')) {
      token->tk_type = tARROW;
    } else if (read_char('-')) {
      token->tk_type = tDEC;
    } else if (read_char('=')) {
      token->tk_type = tSUB_ASSIGN;
    } else {
      token->tk_type = tSUB;
    }
  } else if (read_char('*')) {
    if (read_char('=')) {
      token->tk_type = tMUL_ASSIGN;
    } else {
      token->tk_type = tMUL;
    }
  } else if (read_char('/')) {
    if (read_char('=')) {
      token->tk_type = tDIV_ASSIGN;
    } else if (read_char('/')) {
      while (1) {
        char c = get_char();
        if (c == '\n') break;
      }
      token->tk_type = tSPACE;
    } else if (read_char('*')) {
      while (1) {
        char c = get_char();
        if (c == '*' && read_char('/')) break;
      }
      token->tk_type = tSPACE;
    } else {
      token->tk_type = tDIV;
    }
  } else if (read_char('%')) {
    if (read_char('=')) {
      token->tk_type = tMOD_ASSIGN;
    } else {
      token->tk_type = tMOD;
    }
  } else if (read_char('<')) {
    if (read_char('=')) {
      token->tk_type = tLTE;
    } else if (read_char('<')) {
      token->tk_type = tLSHIFT;
    } else {
      token->tk_type = tLT;
    }
  } else if (read_char('>')) {
    if (read_char('=')) {
      token->tk_type = tGTE;
    } else if (read_char('>')) {
      token->tk_type = tRSHIFT;
    } else {
      token->tk_type = tGT;
    }
  } else if (read_char('=')) {
    if (read_char('=')) {
      token->tk_type = tEQ;
    } else {
      token->tk_type = tASSIGN;
    }
  } else if (read_char('!')) {
    if (read_char('=')) {
      token->tk_type = tNEQ;
    } else {
      token->tk_type = tLNOT;
    }
  } else if (read_char('&')) {
    if (read_char('&')) {
      token->tk_type = tLAND;
    } else {
      token->tk_type = tAND;
    }
  } else if (read_char('|')) {
    if (read_char('|')) {
      token->tk_type = tLOR;
    } else {
      token->tk_type = tOR;
    }
  } else if (read_char('^')) {
    token->tk_type = tXOR;
  } else if (read_char('?')) {
    token->tk_type = tQUESTION;
  } else if (read_char(':')) {
    token->tk_type = tCOLON;
  } else if (read_char(';')) {
    token->tk_type = tSEMICOLON;
  } else if (read_char('[')) {
    token->tk_type = tLBRACKET;
  } else if (read_char(']')) {
    token->tk_type = tRBRACKET;
  } else if (read_char('(')) {
    token->tk_type = tLPAREN;
  } else if (read_char(')')) {
    token->tk_type = tRPAREN;
  } else if (read_char('{')) {
    token->tk_type = tLBRACE;
  } else if (read_char('}')) {
    token->tk_type = tRBRACE;
  } else if (read_char(',')) {
    token->tk_type = tCOMMA;
  } else if (read_char('.')) {
    if (read_char('.')) {
      if (!read_char('.')) {
        error(token, "invalid token '..'");
      }
      token->tk_type = tELLIPSIS;
    } else {
      token->tk_type = tDOT;
    }
  } else if (read_char('#')) {
    token->tk_type = tHASH;
  } else if (read_char(' ')) {
    token->tk_type = tSPACE;
  } else if (read_char('\n')) {
    token->tk_type = tNEWLINE;
  } else if (peek_char() == EOF) {
    token->tk_type = tEND;
  } else {
    error(token, "unexpected character: %c.", peek_char());
  }

  token->tk_name = tk_name[token->tk_type];
  token->schar_end = (SourceChar **) &(src->buffer[src_pos]);

  return token;
}

Vector *tokenize(Vector *input_src) {
  src = input_src;
  src_pos = 0;

  Vector *pp_tokens = vector_new();
  while (1) {
    Token *pp_token = lex();
    vector_push(pp_tokens, pp_token);
    if (pp_token->tk_type == tEND) break;
  }

  return pp_tokens;
}
