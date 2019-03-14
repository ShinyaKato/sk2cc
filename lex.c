#include "sk2cc.h"

char *tk_chars[] = {
  "[", "]", "(", ")", "{", "}", ".", "&",
  "*", "+", "-", "~", "!", "/", "%", "<",
  ">", "^", "|", "?", ":", ";", "=", ",",
  "#"
};

char *tk_names[] = {
  // newline, white space
  "newline", "white space",

  // keywords for expressions
  "sizeof", "_Alignof",

  // keywords for declarations
  "void", "char", "short", "int",
  "unsigned", "_Bool", "struct", "enum",
  "typedef", "extern", "_Noreturn",

  // keywords for statements
  "if", "else", "while", "do",
  "for", "continue", "break", "return",

  // identifiers, constants, string literals
  "identifier", "integer constant", "string literal",

  // punctuators
  "->", "++", "--", "<<",
  ">>", "<=", ">=", "==",
  "!=", "&&", "||", "*=",
  "/=", "%=", "+=", "-=",
  "...",

  // EOF
  "end of file"
};

char *src;
int pos;

char *line_ptr;
int lineno;
int column;

String *token_text;
char *token_filename;
char *token_line_ptr;
int token_lineno;
int token_column;

char *token_name(TokenType tk_type) {
  for (int i = 0; i < sizeof(tk_chars) / sizeof(char*); i++) {
    if (tk_type == tk_chars[i][0]) {
      return tk_chars[i];
    }
  }

  if (128 <= tk_type && tk_type && 128 + sizeof(tk_names) / sizeof(char*)) {
    return tk_names[tk_type - 128];
  }

  // assertion
  fprintf(stderr, "unknown token type: %d\n", tk_type);
  exit(1);
}

Token *token_new(TokenType tk_type) {
  Token *token = calloc(1, sizeof(Token));
  token->tk_type = tk_type;
  token->text = token_text->buffer;
  token->filename = token_filename;
  token->line_ptr = token_line_ptr;
  token->lineno = token_lineno;
  token->column = token_column;
  return token;
}

void next_line() {
  line_ptr = &src[pos];
  lineno++;
  column = 1;
}

// skip '\' '\n' and concat previous and next lines
void skip_backslash_newline() {
  while (src[pos] == '\\' && src[pos + 1] == '\n') {
    pos += 2;
    next_line();
  }
}

char peek_char() {
  skip_backslash_newline();
  return src[pos];
}

char get_char() {
  skip_backslash_newline();

  // read the character and add it to the token text
  char c = src[pos++];
  string_push(token_text, c);

  column++;
  if (c == '\n') next_line();

  return c;
}

char expect_char(char c) {
  if (peek_char() != c) {
    error(token_new(-1), "%c is expected.", c);
  }
  return get_char();
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

  error(token_new(-1), "invalid escape sequence.");
}

Token *next_token() {
  skip_backslash_newline();

  // store the start position of the next token.
  token_text = string_new();
  token_line_ptr = line_ptr;
  token_lineno = lineno;
  token_column = column;

  // check EOF
  if (peek_char() == '\0') {
    return token_new(TK_EOF);
  }

  // get first character
  char c = get_char();

  // nwe line and white space
  if (c == '\n') {
    return token_new(TK_NEWLINE);
  }
  if (isspace(c)) {
    while (isspace(peek_char()) && peek_char() != '\n') {
      get_char();
    }
    return token_new(TK_SPACE);
  }

  // comment
  if (c == '/' && read_char('/')) { // line comment
    while (peek_char() != '\n') {
      get_char();
    }
    return token_new(TK_SPACE);
  }
  if (c == '/' && read_char('*')) { // block comment
    while (1) {
      char c = get_char();
      if (c == '*' && read_char('/')) break;
    }
    return token_new(TK_SPACE);
  }

  // keyword or identifier
  if (isalpha(c) || c == '_') {
    String *string = string_new();
    string_push(string, c);
    while (isalnum(peek_char()) || peek_char() == '_') {
      string_push(string, get_char());
    }

    // check keywords
    if (strcmp(string->buffer, "sizeof") == 0)
      return token_new(TK_SIZEOF);
    if (strcmp(string->buffer, "_Alignof") == 0)
      return token_new(TK_ALIGNOF);
    if (strcmp(string->buffer, "void") == 0)
      return token_new(TK_VOID);
    if (strcmp(string->buffer, "char") == 0)
      return token_new(TK_CHAR);
    if (strcmp(string->buffer, "short") == 0)
      return token_new(TK_SHORT);
    if (strcmp(string->buffer, "int") == 0)
      return token_new(TK_INT);
    if (strcmp(string->buffer, "unsigned") == 0)
      return token_new(TK_UNSIGNED);
    if (strcmp(string->buffer, "_Bool") == 0)
      return token_new(TK_BOOL);
    if (strcmp(string->buffer, "struct") == 0)
      return token_new(TK_STRUCT);
    if (strcmp(string->buffer, "enum") == 0)
      return token_new(TK_ENUM);
    if (strcmp(string->buffer, "typedef") == 0)
      return token_new(TK_TYPEDEF);
    if (strcmp(string->buffer, "extern") == 0)
      return token_new(TK_EXTERN);
    if (strcmp(string->buffer, "_Noreturn") == 0)
      return token_new(TK_NORETURN);
    if (strcmp(string->buffer, "if") == 0)
      return token_new(TK_IF);
    if (strcmp(string->buffer, "else") == 0)
      return token_new(TK_ELSE);
    if (strcmp(string->buffer, "while") == 0)
      return token_new(TK_WHILE);
    if (strcmp(string->buffer, "do") == 0)
      return token_new(TK_DO);
    if (strcmp(string->buffer, "for") == 0)
      return token_new(TK_FOR);
    if (strcmp(string->buffer, "continue") == 0)
      return token_new(TK_CONTINUE);
    if (strcmp(string->buffer, "break") == 0)
      return token_new(TK_BREAK);
    if (strcmp(string->buffer, "return") == 0)
      return token_new(TK_RETURN);

    Token *token = token_new(TK_IDENTIFIER);
    token->identifier = string->buffer;
    return token;
  }

  // integer constant
  if (isdigit(c)) {
    int int_value = c - '0';
    while (isdigit(peek_char())) {
      int_value = int_value * 10 + (get_char() - '0');
    }

    Token *token = token_new(TK_INTEGER_CONST);
    token->int_value = int_value;
    return token;
  }

  // character constant
  if (c == '\'') {
    char c = get_char();
    if (c == '\\') {
      c = get_escape_sequence();
    }
    expect_char('\'');

    Token *token = token_new(TK_INTEGER_CONST);
    token->int_value = c;
    return token;
  }

  // string literal
  if (c == '"') {
    String *string_literal = string_new();
    while (!(peek_char() == '"' || peek_char() == '\0')) {
      char c = get_char();
      if (c == '\\') {
        c = get_escape_sequence();
      }
      string_push(string_literal, c);
    }
    expect_char('"');
    string_push(string_literal, '\0');

    Token *token = token_new(TK_STRING_LITERAL);
    token->string_literal = string_literal;
    return token;
  }

  // punctuators
  if (c == '-' && read_char('>')) return token_new(TK_ARROW);      // ->
  if (c == '+' && read_char('+')) return token_new(TK_INC);        // ++
  if (c == '-' && read_char('-')) return token_new(TK_DEC);        // --
  if (c == '<' && read_char('<')) return token_new(TK_LSHIFT);     // <<
  if (c == '>' && read_char('>')) return token_new(TK_RSHIFT);     // >>
  if (c == '<' && read_char('=')) return token_new(TK_LTE);        // <=
  if (c == '>' && read_char('=')) return token_new(TK_GTE);        // >=
  if (c == '=' && read_char('=')) return token_new(TK_EQ);         // ==
  if (c == '!' && read_char('=')) return token_new(TK_NEQ);        // !=
  if (c == '&' && read_char('&')) return token_new(TK_AND);        // &&
  if (c == '|' && read_char('|')) return token_new(TK_OR);         // ||
  if (c == '*' && read_char('=')) return token_new(TK_MUL_ASSIGN); // *=
  if (c == '/' && read_char('=')) return token_new(TK_DIV_ASSIGN); // /=
  if (c == '%' && read_char('=')) return token_new(TK_MOD_ASSIGN); // %=
  if (c == '+' && read_char('=')) return token_new(TK_ADD_ASSIGN); // +=
  if (c == '-' && read_char('=')) return token_new(TK_SUB_ASSIGN); // -=

  if (c == '.' && read_char('.')) { // ...
    expect_char('.');
    return token_new(TK_ELLIPSIS);
  }

  for (int i = 0; i < sizeof(tk_chars) / sizeof(char*); i++) {
    if (c == tk_chars[i][0]) {
      return token_new(c);
    }
  }

  error(token_new(-1), "failed to tokenize.");
}

Vector *tokenize(char *filename) {
  // read the input file
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    perror(filename);
    exit(1);
  }

  String *file = string_new();
  char buffer[4096];
  while (1) {
    int n = fread(buffer, 1, sizeof(buffer), fp);
    if (n == 0) break;
    for (int i = 0; i < n; i++) {
      string_push(file, buffer[i]);
    }
  }
  string_push(file, '\0');

  fclose(fp);

  // replace '\r\n' with '\n'
  for (int i = 0, j = 0; i < file->length; i++, j++) {
    char c = file->buffer[i];
    if (c == '\r' && i + 1 < file->length && file->buffer[i + 1] == '\n') {
      c = '\n';
      i++;
    } else {
    }
    file->buffer[j] = c;
  }

  // initialization
  src = file->buffer;
  pos = 0;

  line_ptr = src;
  lineno = 1;
  column = 1;

  token_filename = filename;

  // tokenize
  Vector *pp_tokens = vector_new();
  while (1) {
    Token *token = next_token();

    // concat consecutive white spaces
    if (token->tk_type == TK_SPACE) {
      token->text = " ";
      vector_push(pp_tokens, token);

      while (token->tk_type == TK_SPACE) {
        token = next_token();
      }
    }

    vector_push(pp_tokens, token);
    if (token->tk_type == TK_EOF) break;
  }

  // replace '\n' with '\0' for debugging
  // we can display the line by printf("%s\n", token->line_ptr);
  for (int i = 0; src[i]; i++) {
    if (src[i] == '\n') {
      src[i] = '\0';
    }
  }

  return pp_tokens;
}
