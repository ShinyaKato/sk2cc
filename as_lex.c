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
static char *line_ptr;
static int lineno;
static int column;

Location *loc;

static Vector *read_file(char *file) {
  Vector *source = vector_new();

  FILE *fp = fopen(file, "r");
  if (!fp) {
    perror(file);
    exit(1);
  }

  while (1) {
    char c = fgetc(fp);
    if (c == EOF) break;
    ungetc(c, fp);

    String *line = string_new();
    while (1) {
      char c = fgetc(fp);
      if (c == EOF || c == '\n') break;
      string_push(line, c);
    }
    vector_push(source, line->buffer);
  }

  fclose(fp);

  return source;
}

static Location *create_location(void) {
  Location *loc = calloc(1, sizeof(Location));
  loc->filename = filename;
  loc->line_ptr = line_ptr;
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
  switch (line_ptr[column++]) {
    case '"': return '"';
    case '\\': return '\\';
    case 'n': return '\n';
    case '0': return '\0';
  }

  as_error(loc, __FILE__, __LINE__, "invalid escape sequence.");
}

static Token *next_token(void) {
  char c = line_ptr[column++];

  if (c == '.' || c == '_' || isalpha(c)) {
    String *text = string_new();
    string_push(text, c);
    while (line_ptr[column] == '.' || line_ptr[column] == '_' || isalnum(line_ptr[column])) {
      string_push(text, line_ptr[column++]);
    }

    Token *token = create_token(TK_IDENT);
    token->ident = text->buffer;
    return token;
  }

  if (c == '%') {
    String *text = string_new();
    while (isalnum(line_ptr[column])) {
      string_push(text, line_ptr[column++]);
    }

    if (strcmp(text->buffer, "rip") == 0) {
      return create_token(TK_RIP);
    }

    Token *token = create_token(TK_REG);
    token->regtype = regtype(text->buffer);
    token->regcode = regcode(text->buffer);
    return token;
  }

  if (c == '+' || c == '-' || isdigit(c)) {
    int sign = c == '-' ? -1 : 1;
    int num = isdigit(c) ? (c - '0') : 0;
    while (isdigit(line_ptr[column])) {
      num = num * 10 + (line_ptr[column++] - '0');
    }

    Token *token = create_token(TK_NUM);
    token->num = sign * num;
    return token;
  }

  if (c == '$') {
    unsigned int imm = 0;
    if (!isdigit(line_ptr[column])) {
      as_error(loc, __FILE__, __LINE__, "invalid immediate.");
    }
    while (isdigit(line_ptr[column])) {
      imm = imm * 10 + (line_ptr[column++] - '0');
    }

    Token *token = create_token(TK_IMM);
    token->imm = imm;
    return token;
  }

  if (c == '"') {
    String *text = string_new();
    while (1) {
      char c = line_ptr[column++];
      if (c == '"') break;
      if (c == '\\') c = escape_sequence();
      string_push(text, c);
    }

    Token *token = create_token(TK_STR);
    token->string = text;
    return token;
  }

  if (c == ',') return create_token(TK_COMMA);
  if (c == '(') return create_token(TK_LPAREN);
  if (c == ')') return create_token(TK_RPAREN);
  if (c == ':') return create_token(TK_SEMICOLON);

  as_error(loc, __FILE__, __LINE__,  "failed to tokenize.");
}

static Vector *next_line(void) {
  Vector *tokens = vector_new();

  for (column = 0; line_ptr[column] != '\0';) {
    while (line_ptr[column] == ' ' || line_ptr[column] == '\t') column++;
    if (line_ptr[column] == '\0') break;

    loc = create_location();
    vector_push(tokens, next_token());
  }

  return tokens;
}

Vector *as_tokenize(char *_filename) {
  filename = _filename;

  Vector *source = read_file(filename);
  Vector *lines = vector_new();
  for (lineno = 0; lineno < source->length; lineno++) {
    line_ptr = source->buffer[lineno];
    vector_push(lines, next_line());
  }

  return lines;
}
