#include "as.h"

static char *r32[16] = {
  "eax",
  "ecx",
  "edx",
  "ebx",
  "esp",
  "ebp",
  "esi",
  "edi",
  "r8d",
  "r9d",
  "r10d",
  "r11d",
  "r12d",
  "r13d",
  "r14d",
  "r15d",
};

static char *r64[16] = {
  "rax",
  "rcx",
  "rdx",
  "rbx",
  "rsp",
  "rbp",
  "rsi",
  "rdi",
  "r8",
  "r9",
  "r10",
  "r11",
  "r12",
  "r13",
  "r14",
  "r15",
};

Token *token_new(char *file, int lineno, int column, char *line) {
  Token *token = (Token *) calloc(1, sizeof(Token));
  token->file = file;
  token->lineno = lineno;
  token->column = column;
  token->line = line;
  return token;
}

static RegType regtype(char *reg, Token *token) {
  for (int i = 0; i < 16; i++) {
    if (strcmp(reg, r32[i]) == 0) {
      return R32;
    }
  }
  for (int i = 0; i < 16; i++) {
    if (strcmp(reg, r64[i]) == 0) {
      return R64;
    }
  }

  ERROR(token, "invalid register: %s.", reg);
}

static Reg regcode(char *reg, Token *token) {
  for (int i = 0; i < 16; i++) {
    if (strcmp(reg, r32[i]) == 0) {
      return i;
    }
  }
  for (int i = 0; i < 16; i++) {
    if (strcmp(reg, r64[i]) == 0) {
      return i;
    }
  }

  ERROR(token, "invalid register: %s.", reg);
}

Vector *tokenize(char *file, Vector *source) {
  Vector *lines = vector_new();

  for (int lineno = 0; lineno < source->length; lineno++) {
    Vector *tokens = vector_new();
    char *line = source->array[lineno];

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
        token->type = TOK_REG;
        token->regtype = regtype(text->buffer, token);
        token->regcode = regcode(text->buffer, token);
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