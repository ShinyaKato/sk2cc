#include "sk2cc.h"

char *tk_name[] = {
  // white spaces
  "new line",
  "white space",

  // keywords for expressions
  "sizeof",
  "_Alignof",

  // keywords for declarations
  "void",
  "char",
  "short",
  "int",
  "unsigned",
  "_Bool",
  "struct",
  "enum",
  "typedef",
  "extern",
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

char *token_name(TokenType tk_type) {
  if (isascii(tk_type)) {
    String *name = string_new();
    string_push(name, tk_type);
    return name->buffer;
  }

  return tk_name[tk_type - 128];
}
