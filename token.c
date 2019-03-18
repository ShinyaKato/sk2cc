#include "sk2cc.h"

char *tk_chars[] = {
  "[",
  "]",
  "(",
  ")",
  "{",
  "}",
  ".",
  "&",
  "*",
  "+",
  "-",
  "~",
  "!",
  "/",
  "%",
  "<",
  ">",
  "^",
  "|",
  "?",
  ":",
  ";",
  "=",
  ",",
  "#"
};

char *tk_names[] = {
  // newline, white space
  "newline",
  "white space",

  // pp-number
  "preprocessing number",

  // keywords for expressions
  "sizeof",
  "_Alignof",

  // keywords for declarations
  "typedef",
  "extern",
  "void",
  "char",
  "short",
  "int",
  "long",
  "signed",
  "unsigned",
  "_Bool",
  "struct",
  "enum",
  "_Noreturn",

  // keywords for statements
  "if",
  "else",
  "while",
  "do",
  "for",
  "goto",
  "continue",
  "break",
  "return",

  // identifiers, constants, string literals
  "identifier",
  "integer constant",
  "character constant",
  "string literal",

  // punctuators
  "->",
  "++",
  "--",
  "<<",
  ">>",
  "<=",
  ">=",
  "==",
  "!=",
  "&&",
  "||",
  "*=",
  "/=",
  "%=",
  "+=",
  "-=",
  "...",

  // EOF
  "end of file"
};

bool check_char_token(char c) {
  for (int i = 0; i < sizeof(tk_chars) / sizeof(char *); i++) {
    if (c == tk_chars[i][0]) {
      return true;
    }
  }

  return false;
}

char *token_name(TokenType tk_type) {
  for (int i = 0; i < sizeof(tk_chars) / sizeof(char *); i++) {
    if (tk_type == tk_chars[i][0]) {
      return tk_chars[i];
    }
  }

  if (128 <= tk_type && tk_type < 128 + sizeof(tk_names) / sizeof(char *)) {
    return tk_names[tk_type - 128];
  }

  // unreachable
  internal_error("unknown token type: %d\n", tk_type);
}

Token *token_new(TokenType tk_type, char *text, char *filename, char *line_ptr, int lineno, int column) {
  Token *token = calloc(1, sizeof(Token));
  token->tk_type = tk_type;
  token->tk_name = token_name(tk_type);
  token->text = text;
  token->filename = filename;
  token->line_ptr = line_ptr;
  token->lineno = lineno;
  token->column = column;
  return token;
}

Token *inspect_pp_number(Token *token) {
  char *pp_number = token->pp_number;
  int pos = 0;

  unsigned long long int_value = 0;
  bool int_decimal = false;
  if (pp_number[pos] == '0') {
    pos++;
    if (tolower(pp_number[pos]) == 'x') {
      pos++;
      while (isxdigit(pp_number[pos])) {
        char c = pp_number[pos++];
        int d = isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
        int_value = int_value * 16 + d;
      }
    } else {
      while ('0' <= pp_number[pos] && pp_number[pos] < '8') {
        int_value = int_value * 8 + (pp_number[pos++] - '0');
      }
    }
  } else {
    int_decimal = true;
    while (isdigit(pp_number[pos])) {
      int_value = int_value * 10 + (pp_number[pos++] - '0');
    }
  }

  bool int_u = false;
  bool int_l = false;
  bool int_ll = false;
  if (tolower(pp_number[pos]) == 'u') {
    int_u = true;
    pos++;
    if (tolower(pp_number[pos] == 'l')) {
      pos++;
      if (tolower(pp_number[pos] == 'l')) {
        int_ll = true;
        pos++;
      } else {
        int_l = true;
      }
    }
  } else if (tolower(pp_number[pos]) == 'l') {
    pos++;
    if (tolower(pp_number[pos]) == 'l') {
      int_ll = true;
      pos++;
    } else {
      int_l = true;
    }
    if (tolower(pp_number[pos]) == 'u') {
      int_u = true;
      pos++;
    }
  }

  if (pp_number[pos] != '\0') {
    error(token, "invalid preprocessing number.");
  }

  Token *new_token = token_new(TK_INTEGER_CONST, token->text, token->filename, token->line_ptr, token->column, token->lineno);
  new_token->int_value = int_value;
  new_token->int_decimal = int_decimal;
  new_token->int_u = int_u;
  new_token->int_l = int_l;
  new_token->int_ll = int_ll;
  return new_token;
}
