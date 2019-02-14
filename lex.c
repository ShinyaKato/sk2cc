#include "sk2cc.h"

char *filename;

char *src;
int pos;

char *start_ptr;
char *line_ptr;
int lineno;
int column;

Token *token_new(int tk_type) {
  Token *token = calloc(1, sizeof(Token));
  token->tk_type = tk_type;
  token->filename = filename;
  token->start_ptr = start_ptr;
  token->end_ptr = &src[pos];
  token->line_ptr = line_ptr;
  token->lineno = lineno;
  token->column = column;
  return token;
}

int peek_char() {
  return src[pos];
}

int get_char() {
  int c = peek_char();
  pos++;

  column++;
  if (c == '\n') {
    line_ptr = &src[pos];
    lineno++;
    column = 1;
  }

  return c;
}

int expect_char(int c) {
  if (peek_char() != c) {
    error(token_new(-1), "%c is expected.", c);
  }
  return get_char();
}

bool read_char(int c) {
  if (peek_char() == c) {
    get_char();
    return true;
  }
  return false;
}

int get_escape_sequence() {
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
  // store the start position of the next token.
  start_ptr = &src[pos];

  // white space
  if (read_char('\n')) return token_new(TK_NEWLINE);
  if (read_char(' ')) return token_new(TK_SPACE);

  // keyword or identifier
  if (isalpha(peek_char()) || peek_char() == '_') {
    String *string = string_new();
    do {
      string_push(string, get_char());
    } while (isalnum(peek_char()) || peek_char() == '_');
    char *identifier = string->buffer;

    // check keywords
    if (strcmp(identifier, "sizeof") == 0) {
      return token_new(TK_SIZEOF);
    }
    if (strcmp(identifier, "_Alignof") == 0) {
      return token_new(TK_ALIGNOF);
    }
    if (strcmp(identifier, "void") == 0) {
      return token_new(TK_VOID);
    }
    if (strcmp(identifier, "char") == 0) {
      return token_new(TK_CHAR);
    }
    if (strcmp(identifier, "short") == 0) {
      return token_new(TK_SHORT);
    }
    if (strcmp(identifier, "int") == 0) {
      return token_new(TK_INT);
    }
    if (strcmp(identifier, "unsigned") == 0) {
      return token_new(TK_UNSIGNED);
    }
    if (strcmp(identifier, "_Bool") == 0) {
      return token_new(TK_BOOL);
    }
    if (strcmp(identifier, "struct") == 0) {
      return token_new(TK_STRUCT);
    }
    if (strcmp(identifier, "enum") == 0) {
      return token_new(TK_ENUM);
    }
    if (strcmp(identifier, "typedef") == 0) {
      return token_new(TK_TYPEDEF);
    }
    if (strcmp(identifier, "extern") == 0) {
      return token_new(TK_EXTERN);
    }
    if (strcmp(identifier, "_Noreturn") == 0) {
      return token_new(TK_NORETURN);
    }
    if (strcmp(identifier, "if") == 0) {
      return token_new(TK_IF);
    }
    if (strcmp(identifier, "else") == 0) {
      return token_new(TK_ELSE);
    }
    if (strcmp(identifier, "while") == 0) {
      return token_new(TK_WHILE);
    }
    if (strcmp(identifier, "do") == 0) {
      return token_new(TK_DO);
    }
    if (strcmp(identifier, "for") == 0) {
      return token_new(TK_FOR);
    }
    if (strcmp(identifier, "continue") == 0) {
      return token_new(TK_CONTINUE);
    }
    if (strcmp(identifier, "break") == 0) {
      return token_new(TK_BREAK);
    }
    if (strcmp(identifier, "return") == 0) {
      return token_new(TK_RETURN);
    }

    Token *token = token_new(TK_IDENTIFIER);
    token->identifier = identifier;
    return token;
  }

  // integer constant
  if (isdigit(peek_char())) {
    int int_value = 0;
    do {
      int c = get_char();
      int_value = int_value * 10 + (c - '0');
    } while (isdigit(peek_char()));

    Token *token = token_new(TK_INTEGER_CONST);
    token->int_value = int_value;
    return token;
  }

  // character constant
  if (read_char('\'')) {
    int c = get_char();
    if (c == '\\') {
      c = get_escape_sequence();
    }
    expect_char('\'');

    Token *token = token_new(TK_INTEGER_CONST);
    token->int_value = c;
    return token;
  }

  // string literal
  if (read_char('"')) {
    String *string_literal = string_new();
    while (!(peek_char() == '"' || peek_char() == '\0')) {
      int c = get_char();
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
  if (read_char('[')) return token_new('[');
  if (read_char(']')) return token_new(']');
  if (read_char('(')) return token_new('(');
  if (read_char(')')) return token_new(')');
  if (read_char('{')) return token_new('{');
  if (read_char('}')) return token_new('}');
  if (read_char('*')) {
    if (read_char('=')) return token_new(TK_MUL_ASSIGN);
    return token_new('*');
  }
  if (read_char('/')) {
    if (read_char('/')) { // read '//' comment
      // skip until newline
      while (get_char() != '\n');
      return token_new(TK_SPACE);
    }
    if (read_char('*')) { // read '/*' comment
      // skip until '*/'
      while (1) {
        int c = get_char();
        if (c == '*' && read_char('/')) break;
      }
      return token_new(TK_SPACE);
    }
    if (read_char('=')) return token_new(TK_DIV_ASSIGN);
    return token_new('/');
  }
  if (read_char('%')) {
    if (read_char('=')) return token_new(TK_MOD_ASSIGN);
    return token_new('%');
  }
  if (read_char('+')) {
    if (read_char('+')) return token_new(TK_INC);
    if (read_char('=')) return token_new(TK_ADD_ASSIGN);
    return token_new('+');
  }
  if (read_char('-')) {
    if (read_char('-')) return token_new(TK_DEC);
    if (read_char('=')) return token_new(TK_SUB_ASSIGN);
    if (read_char('>')) return token_new(TK_ARROW);
    return token_new('-');
  }
  if (read_char('<')) {
    if (read_char('<')) return token_new(TK_LSHIFT);
    if (read_char('=')) return token_new(TK_LTE);
    return token_new('<');
  }
  if (read_char('>')) {
    if (read_char('>')) return token_new(TK_RSHIFT);
    if (read_char('=')) return token_new(TK_GTE);
    return token_new('>');
  }
  if (read_char('=')) {
    if (read_char('=')) return token_new(TK_EQ);
    return token_new('=');
  }
  if (read_char('!')) {
    if (read_char('=')) return token_new(TK_NEQ);
    return token_new('!');
  }
  if (read_char('&')) {
    if (read_char('&')) return token_new(TK_AND);
    return token_new('&');
  }
  if (read_char('^')) return token_new('^');
  if (read_char('|')) {
    if (read_char('|')) return token_new(TK_OR);
    return token_new('|');
  }
  if (read_char('~')) return token_new('~');
  if (read_char('?')) return token_new('?');
  if (read_char(':')) return token_new(':');
  if (read_char(';')) return token_new(';');
  if (read_char('.')) {
    if (read_char('.')) {
      expect_char('.');
      return token_new(TK_ELLIPSIS);
    }
    return token_new('.');
  }
  if (read_char(',')) return token_new(',');
  if (read_char('#')) return token_new('#');

  error(token_new(-1), "invalid token.");
}

Vector *tokenize(char *input_filename) {
  filename = input_filename;

  // read the input file
  String *file = string_new();
  char buffer[4096];
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    perror(filename);
    exit(1);
  }
  while (1) {
    int n = fread(buffer, 1, sizeof(buffer), fp);
    if (n == 0) break;
    for (int i = 0; i < n; i++) {
      string_push(file, buffer[i]);
    }
  }
  fclose(fp);

  src = file->buffer;
  pos = 0;

  line_ptr = src;
  lineno = 1;
  column = 1;

  Vector *pp_tokens = vector_new();
  while (src[pos]) {
    Token *pp_token = next_token();
    vector_push(pp_tokens, pp_token);
  }
  vector_push(pp_tokens, token_new(TK_EOF));

  return pp_tokens;
}
