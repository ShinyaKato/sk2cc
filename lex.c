#include "sk2cc.h"

char *src;
int pos;

char *line_ptr;
int lineno;
int column;

String *lex_text;
char *lex_filename;
char *lex_line_ptr;
int lex_lineno;
int lex_column;

Map *keywords;

String *read_file(char *filename) {
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

  fclose(fp);

  return file;
}

// replace '\r\n' with '\n'
String *replace_newline(String *file) {
  String *src = string_new();
  for (int i = 0; i < file->length; i++) {
    char c = file->buffer[i];
    if (c == '\r' && i + 1 < file->length && file->buffer[i + 1] == '\n') {
      c = '\n';
      i++;
    }
    string_push(src, c);
  }

  return src;
}

Token *create_token(TokenType tk_type) {
  return token_new(tk_type, lex_text->buffer, lex_filename, lex_line_ptr, lex_lineno, lex_column);
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
  string_push(lex_text, c);

  column++;
  if (c == '\n') next_line();

  return c;
}

char expect_char(char c) {
  if (peek_char() != c) {
    error(create_token(-1), "%c is expected.", c);
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

Map *create_keywords() {
  Map *map = map_new();

  map_puti(map, "sizeof", TK_SIZEOF);
  map_puti(map, "_Alignof", TK_ALIGNOF);
  map_puti(map, "typedef", TK_TYPEDEF);
  map_puti(map, "extern", TK_EXTERN);
  map_puti(map, "static", TK_STATIC);
  map_puti(map, "void", TK_VOID);
  map_puti(map, "char", TK_CHAR);
  map_puti(map, "short", TK_SHORT);
  map_puti(map, "int", TK_INT);
  map_puti(map, "long", TK_LONG);
  map_puti(map, "signed", TK_SIGNED);
  map_puti(map, "unsigned", TK_UNSIGNED);
  map_puti(map, "_Bool", TK_BOOL);
  map_puti(map, "struct", TK_STRUCT);
  map_puti(map, "enum", TK_ENUM);
  map_puti(map, "_Noreturn", TK_NORETURN);
  map_puti(map, "case", TK_CASE);
  map_puti(map, "default", TK_DEFAULT);
  map_puti(map, "if", TK_IF);
  map_puti(map, "else", TK_ELSE);
  map_puti(map, "switch", TK_SWITCH);
  map_puti(map, "while", TK_WHILE);
  map_puti(map, "do", TK_DO);
  map_puti(map, "for", TK_FOR);
  map_puti(map, "goto", TK_GOTO);
  map_puti(map, "continue", TK_CONTINUE);
  map_puti(map, "break", TK_BREAK);
  map_puti(map, "return", TK_RETURN);

  return map;
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

  error(create_token(-1), "invalid escape sequence.");
}

Token *next_token() {
  skip_backslash_newline();

  // store the start position of the next token.
  lex_text = string_new();
  lex_line_ptr = line_ptr;
  lex_lineno = lineno;
  lex_column = column;

  // check EOF
  if (peek_char() == '\0') {
    return create_token(TK_EOF);
  }

  // get first character
  char c = get_char();

  // nwe-line and white-space
  if (c == '\n') {
    return create_token(TK_NEWLINE);
  }
  if (isspace(c)) {
    while (isspace(peek_char()) && peek_char() != '\n') {
      get_char();
    }
    return create_token(TK_SPACE);
  }

  // comment
  if (c == '/' && read_char('/')) { // line comment
    while (peek_char() != '\n') {
      get_char();
    }
    return create_token(TK_SPACE);
  }
  if (c == '/' && read_char('*')) { // block comment
    while (1) {
      char c = get_char();
      if (c == '*' && read_char('/')) break;
    }
    return create_token(TK_SPACE);
  }

  // keyword or identifier
  if (isalpha(c) || c == '_') {
    String *string = string_new();
    string_push(string, c);
    while (isalnum(peek_char()) || peek_char() == '_') {
      string_push(string, get_char());
    }

    TokenType tk_type = map_lookupi(keywords, string->buffer);
    if (tk_type) return create_token(tk_type);

    Token *token = create_token(TK_IDENTIFIER);
    token->identifier = string->buffer;
    return token;
  }

  // pp-number
  if (isdigit(c)) {
    String *pp_number = string_new();
    string_push(pp_number, c);
    while (isalnum(peek_char()) || peek_char() == '_') {
      string_push(pp_number, get_char());
    }

    Token *token = create_token(TK_PP_NUMBER);
    token->pp_number = pp_number->buffer;
    return token;
  }

  // character-constant
  if (c == '\'') {
    char char_value = get_char();
    if (char_value == '\\') {
      char_value = get_escape_sequence();
    }
    expect_char('\'');

    Token *token = create_token(TK_CHAR_CONST);
    token->char_value = char_value;
    return token;
  }

  // string-literal
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

    Token *token = create_token(TK_STRING_LITERAL);
    token->string_literal = string_literal;
    return token;
  }

  // punctuators
  if (c == '-' && read_char('>')) return create_token(TK_ARROW);      // ->
  if (c == '+' && read_char('+')) return create_token(TK_INC);        // ++
  if (c == '-' && read_char('-')) return create_token(TK_DEC);        // --
  if (c == '<' && read_char('<')) return create_token(TK_LSHIFT);     // <<
  if (c == '>' && read_char('>')) return create_token(TK_RSHIFT);     // >>
  if (c == '<' && read_char('=')) return create_token(TK_LTE);        // <=
  if (c == '>' && read_char('=')) return create_token(TK_GTE);        // >=
  if (c == '=' && read_char('=')) return create_token(TK_EQ);         // ==
  if (c == '!' && read_char('=')) return create_token(TK_NEQ);        // !=
  if (c == '&' && read_char('&')) return create_token(TK_AND);        // &&
  if (c == '|' && read_char('|')) return create_token(TK_OR);         // ||
  if (c == '*' && read_char('=')) return create_token(TK_MUL_ASSIGN); // *=
  if (c == '/' && read_char('=')) return create_token(TK_DIV_ASSIGN); // /=
  if (c == '%' && read_char('=')) return create_token(TK_MOD_ASSIGN); // %=
  if (c == '+' && read_char('=')) return create_token(TK_ADD_ASSIGN); // +=
  if (c == '-' && read_char('=')) return create_token(TK_SUB_ASSIGN); // -=

  if (c == '.' && read_char('.')) { // ...
    expect_char('.');
    return create_token(TK_ELLIPSIS);
  }

  if (check_char_token(c)) {
    return create_token(c);
  }

  error(create_token(-1), "failed to tokenize.");
}

Vector *tokenize(char *filename) {
  // read the input file
  String *file = read_file(filename);

  // initialization
  src = replace_newline(file)->buffer;
  pos = 0;

  line_ptr = src;
  lineno = 1;
  column = 1;

  lex_filename = filename;

  if (!keywords) {
    keywords = create_keywords();
  }

  // tokenize
  Vector *pp_tokens = vector_new();
  while (1) {
    Token *token = next_token();

    // concat consecutive white spaces
    if (token->tk_type == TK_SPACE) {
      Token *space = token;
      String *text = string_new();
      while (token->tk_type == TK_SPACE) {
        string_write(text, token->text);
        token = next_token();
      }
      space->text = text->buffer;
      vector_push(pp_tokens, space);
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
