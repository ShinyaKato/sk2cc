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
  "continue",
  "break",
  "return",

  // identifiers, constants, string literals
  "identifier",
  "integer constant",
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

  // assertion
  fprintf(stderr, "unknown token type: %d\n", tk_type);
  exit(1);
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
