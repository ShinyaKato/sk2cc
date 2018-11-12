#include "as.h"

Token *token_new(char *file, int lineno, int column, char *line) {
  Token *token = (Token *) calloc(1, sizeof(Token));
  token->file = file;
  token->lineno = lineno;
  token->column = column;
  token->line = line;
  return token;
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
        char *regs[16] = {
          "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
          "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
        };
        int reg = 0;
        for (; reg < 16; reg++) {
          if (strcmp(text->buffer, regs[reg]) == 0) break;
        }
        if (reg == 16) {
          ERROR(token, "invalid register: %s.\n", text->buffer);
        }
        token->type = TOK_REG;
        token->reg = reg;
      } else if (c == '+' || c == '-' || isdigit(c)) {
        int sign = 1;
        if (c == '-') sign = -1;
        int disp = isdigit(c) ? c - '0' : 0;
        while (isdigit(line[column])) {
          disp = disp * 10 + (line[column++] - '0');
        }
        token->type = TOK_DISP;
        token->disp = sign * disp;
      } else if (c == '$') {
        int imm = 0;
        if (!isdigit(line[column])) {
          ERROR(token, "invalid immediate.\n");
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
        ERROR(token, "invalid character: %c\n", c);
      }

      vector_push(tokens, token);
    }

    vector_push(lines, tokens);
  }

  return lines;
}
