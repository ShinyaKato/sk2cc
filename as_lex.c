#include "as.h"

static char *regs[16][4] = {
  { "al", "ax", "eax", "rax" },
  { "cl", "cx", "ecx", "rcx" },
  { "dl", "dx", "edx", "rdx" },
  { "bl", "bx", "ebx", "rbx" },
  { "spl", "sp", "esp", "rsp" },
  { "bpl", "bp", "ebp", "rbp" },
  { "sil", "si", "esi", "rsi" },
  { "dil", "di", "edi", "rdi" },
  { "r8b", "r8w", "r8d", "r8" },
  { "r9b", "r9w", "r9d", "r9" },
  { "r10b", "r10w", "r10d", "r10" },
  { "r11b", "r11w", "r11d", "r11" },
  { "r12b", "r12w", "r12d", "r12" },
  { "r13b", "r13w", "r13d", "r13" },
  { "r14b", "r14w", "r14d", "r14" },
  { "r15b", "r15w", "r15d", "r15" },
};

static char *filename;

static char *src;
static int pos;

static char **cur_line;
static int lineno;
static int column;

Location *loc;

static char *read_file(void) {
  FILE *fp;
  if (strcmp(filename, "-") == 0) {
    filename = "stdin";
    fp = stdin;
  } else {
    fp = fopen(filename, "r");
    if (!fp) {
      perror(filename);
      exit(1);
    }
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

  String *src = string_new();
  for (int i = 0; i < file->length; i++) {
    char c = file->buffer[i];
    if (c == '\r' && i + 1 < file->length && file->buffer[i + 1] == '\n') {
      c = '\n';
      i++;
    }
    string_push(src, c);
  }

  return src->buffer;
}

static char **split_lines(char *orig_src) {
  String *src = string_new();
  string_write(src, orig_src);

  Vector *lines = vector_new();
  for (int i = 0; i < src->length; i++) {
    vector_push(lines, &src->buffer[i]);

    while (src->buffer[i] != '\n') i++;
    src->buffer[i] = '\0';
  }

  return (char **) lines->buffer;
}

static Location *create_location(void) {
  Location *loc = calloc(1, sizeof(Location));
  loc->filename = filename;
  loc->line = *cur_line;
  loc->lineno = lineno;
  loc->column = column;
  return loc;
}

static Token *create_token(TokenType type) {
  Token *token = (Token *) calloc(1, sizeof(Token));
  token->type = type;
  token->loc = loc;
  return token;
}

static char peek_char(void) {
  return src[pos];
}

static char get_char(void) {
  column++;
  if (src[pos] == '\n') {
    cur_line++;
    lineno++;
    column = 1;
  }

  return src[pos++];
}

static RegSize regtype(char *reg) {
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 4; j++) {
      if (strcmp(reg, regs[i][j]) == 0) return j;
    }
  }

  as_error(loc, __FILE__, __LINE__, "unknown register: %s.", reg);
}

static RegCode regcode(char *reg) {
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 4; j++) {
      if (strcmp(reg, regs[i][j]) == 0) return i;
    }
  }

  as_error(loc, __FILE__, __LINE__, "unknown register: %s.", reg);
}

static char escape_sequence(void) {
  switch (get_char()) {
    case '"': return '"';
    case '\\': return '\\';
    case 'n': return '\n';
    case '0': return '\0';
  }

  as_error(loc, __FILE__, __LINE__, "invalid escape sequence.");
}

static Token *next_token(void) {
  // skip white spaces
  while (isspace(peek_char()) && peek_char() != '\n') get_char();

  // store the start position of the next token
  loc = create_location();

  // get first character
  char c = get_char();

  // end of file
  if (c == '\0') {
    return create_token(TK_EOF);
  }

  // newline
  if (c == '\n') {
    return create_token(TK_NEWLINE);
  }

  // identifier
  if (c == '.' || c == '_' || isalpha(c)) {
    String *ident = string_new();
    string_push(ident, c);
    while (peek_char() == '.' || peek_char() == '_' || isalnum(peek_char())) {
      string_push(ident, get_char());
    }

    Token *token = create_token(TK_IDENT);
    token->ident = ident->buffer;
    return token;
  }

  // register
  if (c == '%') {
    String *reg = string_new();
    while (isalnum(peek_char())) {
      string_push(reg, get_char());
    }

    if (strcmp(reg->buffer, "rip") == 0) {
      return create_token(TK_RIP);
    }

    Token *token = create_token(TK_REG);
    token->regtype = regtype(reg->buffer);
    token->regcode = regcode(reg->buffer);
    return token;
  }

  // number
  if (c == '+' || c == '-' || isdigit(c)) {
    int sign = c == '-' ? -1 : 1;
    int num = isdigit(c) ? (c - '0') : 0;
    while (isdigit(peek_char())) {
      num = num * 10 + (get_char() - '0');
    }

    Token *token = create_token(TK_NUM);
    token->num = sign * num;
    return token;
  }

  // immediate
  if (c == '$') {
    unsigned int imm = 0;
    if (!isdigit(peek_char())) {
      as_error(loc, __FILE__, __LINE__, "invalid immediate.");
    }
    while (isdigit(peek_char())) {
      imm = imm * 10 + (get_char() - '0');
    }

    Token *token = create_token(TK_IMM);
    token->imm = imm;
    return token;
  }

  // string
  if (c == '"') {
    String *string = string_new();
    while (1) {
      char c = get_char();
      if (c == '"') break;
      if (c == '\\') c = escape_sequence();
      string_push(string, c);
    }

    Token *token = create_token(TK_STR);
    token->string = string;
    return token;
  }

  // punctuators
  if (c == ',') return create_token(TK_COMMA);
  if (c == '(') return create_token(TK_LPAREN);
  if (c == ')') return create_token(TK_RPAREN);
  if (c == ':') return create_token(TK_SEMICOLON);

  as_error(loc, __FILE__, __LINE__,  "failed to tokenize.");
}

Vector *as_tokenize(char *_filename) {
  filename = _filename;

  src = read_file();
  pos = 0;

  cur_line = split_lines(src);
  lineno = 1;
  column = 1;

  Vector *tokens = vector_new();
  while (1) {
    Token *token = next_token();
    vector_push(tokens, token);

    if (token->type == TK_EOF) break;
  }

  return tokens;
}
