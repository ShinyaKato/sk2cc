#include "as.h"

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

static Token *token_new(char *file, int lineno, int column, char *line) {
  Token *token = (Token *) calloc(1, sizeof(Token));
  token->file = file;
  token->lineno = lineno;
  token->column = column;
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

static RegType regtype(char *reg, Token *token) {
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 4; j++) {
      if (strcmp(reg, regs[i][j]) == 0) return j;
    }
  }
  ERROR(token, "invalid register: %s.", reg);
}

static Reg regcode(char *reg, Token *token) {
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 4; j++) {
      if (strcmp(reg, regs[i][j]) == 0) return i;
    }
  }
  ERROR(token, "invalid register: %s.", reg);
}

Vector *as_tokenize(char *file) {
  Vector *source = read_file(file);

  Vector *lines = vector_new();

  for (int lineno = 0; lineno < source->length; lineno++) {
    Vector *tokens = vector_new();
    char *line = source->buffer[lineno];

    for (int column = 0; line[column] != '\0';) {
      while (line[column] == ' ' || line[column] == '\t') column++;
      if (line[column] == '\0') break;

      Token *token = token_new(file, lineno, column, line);
      char c = line[column++];
      if (c == '.' || c == '_' || isalpha(c)) {
        String *text = string_new();
        string_push(text, c);
        while (line[column] == '.' || line[column] == '_' || isalnum(line[column])) {
          string_push(text, line[column++]);
        }
        token->type = TOK_IDENT;
        token->ident = text->buffer;
      } else if (c == '%') {
        String *text = string_new();
        while (isalnum(line[column])) {
          string_push(text, line[column++]);
        }
        if (strcmp(text->buffer, "rip") == 0) {
          token->type = TOK_RIP;
        } else {
          token->type = TOK_REG;
          token->regtype = regtype(text->buffer, token);
          token->regcode = regcode(text->buffer, token);
        }
      } else if (c == '+' || c == '-' || isdigit(c)) {
        int sign = c == '-' ? -1 : 1;
        int num = isdigit(c) ? (c - '0') : 0;
        while (isdigit(line[column])) {
          num = num * 10 + (line[column++] - '0');
        }
        token->type = TOK_NUM;
        token->num = sign * num;
      } else if (c == '$') {
        unsigned int imm = 0;
        if (!isdigit(line[column])) {
          ERROR(token, "invalid immediate.");
        }
        while (isdigit(line[column])) {
          imm = imm * 10 + (line[column++] - '0');
        }
        token->type = TOK_IMM;
        token->imm = imm;
      } else if (c == '"') {
        String *text = string_new();
        while (1) {
          char c = line[column++];
          if (c == '"') break;
          if (c == '\\') {
            switch (line[column++]) {
              case '"':
                string_push(text, '"');
                break;
              case '\\':
                string_push(text, '\\');
                break;
              case 'n':
                string_push(text, '\n');
                break;
              case '0':
                string_push(text, '\0');
                break;
              default:
                ERROR(token, "invalid escape character.");
            }
          } else {
            string_push(text, c);
          }
        }
        token->type = TOK_STR;
        token->length = text->length;
        token->string = text->buffer;
      } else if (c == ',') {
        token->type = TOK_COMMA;
      } else if (c == '(') {
        token->type = TOK_LPAREN;
      } else if (c == ')') {
        token->type = TOK_RPAREN;
      } else if (c == ':') {
        token->type = TOK_SEMICOLON;
      } else {
        ERROR(token, "invalid character: %c.", c);
      }

      vector_push(tokens, token);
    }

    vector_push(lines, tokens);
  }

  return lines;
}
