#include "as.h"

#define LEX_ERROR(...) \
  do { \
    as_error(file, token_lineno, token_column, line, __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)

static char *file;
static int lineno;
static int column;
static char *line;

static int token_lineno;
static int token_column;

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

static Token *create_token(TokenType type) {
  Token *token = (Token *) calloc(1, sizeof(Token));
  token->type = type;
  token->file = file;
  token->lineno = token_lineno;
  token->column = token_column;
  token->line = line;
  return token;
}

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

static RegType regtype(char *reg) {
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 4; j++) {
      if (strcmp(reg, regs[i][j]) == 0) return j;
    }
  }

  LEX_ERROR("invalid register: %s.", reg);
}

static Reg regcode(char *reg) {
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 4; j++) {
      if (strcmp(reg, regs[i][j]) == 0) return i;
    }
  }

  LEX_ERROR("invalid register: %s.", reg);
}

char escape_sequence(void) {
  switch (line[column++]) {
    case '"': return '"';
    case '\\': return '\\';
    case 'n': return '\n';
    case '0': return '\0';
  }

  LEX_ERROR("invalid escape sequence.");
}

Token *next_token(void) {
  char c = line[column++];
  token_lineno = lineno;
  token_column = column;

  if (c == '.' || c == '_' || isalpha(c)) {
    String *text = string_new();
    string_push(text, c);
    while (line[column] == '.' || line[column] == '_' || isalnum(line[column])) {
      string_push(text, line[column++]);
    }

    Token *token = create_token(TOK_IDENT);
    token->ident = text->buffer;
    return token;
  }

  if (c == '%') {
    String *text = string_new();
    while (isalnum(line[column])) {
      string_push(text, line[column++]);
    }

    if (strcmp(text->buffer, "rip") == 0) {
      return create_token(TOK_RIP);
    }

    Token *token = create_token(TOK_REG);
    token->regtype = regtype(text->buffer);
    token->regcode = regcode(text->buffer);
    return token;
  }

  if (c == '+' || c == '-' || isdigit(c)) {
    int sign = c == '-' ? -1 : 1;
    int num = isdigit(c) ? (c - '0') : 0;
    while (isdigit(line[column])) {
      num = num * 10 + (line[column++] - '0');
    }

    Token *token = create_token(TOK_NUM);
    token->num = sign * num;
    return token;
  }

  if (c == '$') {
    unsigned int imm = 0;
    if (!isdigit(line[column])) {
      LEX_ERROR("invalid immediate.");
    }
    while (isdigit(line[column])) {
      imm = imm * 10 + (line[column++] - '0');
    }

    Token *token = create_token(TOK_IMM);
    token->imm = imm;
    return token;
  }

  if (c == '"') {
    String *text = string_new();
    while (1) {
      char c = line[column++];
      if (c == '"') break;
      if (c == '\\') c = escape_sequence();
      string_push(text, c);
    }

    Token *token = create_token(TOK_STR);
    token->length = text->length;
    token->string = text->buffer;
    return token;
  }

  if (c == ',') {
    return create_token(TOK_COMMA);
  }

  if (c == '(') {
    return create_token(TOK_LPAREN);
  }

  if (c == ')') {
    return create_token(TOK_RPAREN);
  }

  if (c == ':') {
    return create_token(TOK_SEMICOLON);
  }

  LEX_ERROR("unexpected character: %c.", c);
}

Vector *next_line(void) {
  Vector *tokens = vector_new();

  for (column = 0; line[column] != '\0';) {
    while (line[column] == ' ' || line[column] == '\t') column++;
    if (line[column] == '\0') break;

    vector_push(tokens, next_token());
  }

  return tokens;
}

Vector *as_tokenize(char *_file) {
  file = _file;

  Vector *source = read_file(file);
  Vector *lines = vector_new();
  for (lineno = 0; lineno < source->length; lineno++) {
    line = source->buffer[lineno];
    vector_push(lines, next_line());
  }

  return lines;
}
