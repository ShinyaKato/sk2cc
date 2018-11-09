#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdnoreturn.h>
#include <elf.h>

typedef struct string {
  int size, length;
  char *buffer;
} String;

extern String *string_new();
extern void string_push(String *string, char c);

typedef struct vector {
  int size, length;
  void **array;
} Vector;

extern Vector *vector_new();
extern void vector_push(Vector *vector, void *value);

#define ERROR(token, ...) \
  errorf((token)->file, (token)->lineno, (token)->column, (token)->line, __VA_ARGS__)

noreturn void errorf(char *file, int lineno, int column, char *line, char *format, ...) {
  fprintf(stderr, "%s:%d:%d: ", file, lineno + 1, column + 1);

  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);

  fprintf(stderr, " %s\n", line);

  for (int i = 0; i < column + 1; i++) {
    fprintf(stderr, " ");
  }
  fprintf(stderr, "^\n");

  exit(1);
}

Vector *scan(char *file) {
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

typedef enum token_type TokenType;
enum token_type {
  COMMA = ',',
  IDENT = 256,
  REG,
  IMM,
};

typedef struct token Token;
struct token {
  TokenType type;
  char *ident;
  int reg;
  int imm;
  char *file;
  int lineno;
  int column;
  char *line;
};

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
        token->type = IDENT;
        token->ident = text->buffer;
      } else if (c == '%') {
        String *text = string_new();
        while (isalnum(line[column])) {
          string_push(text, line[column++]);
        }
        char *regs[8] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi" };
        int reg = 0;
        for (; reg < 8; reg++) {
          if (strcmp(text->buffer, regs[reg]) == 0) break;
        }
        if (reg == 8) {
          ERROR(token, "invalid register: %s.\n", text->buffer);
        }
        token->type = REG;
        token->reg = reg;
      } else if (c == '$') {
        int imm = 0;
        if (!isdigit(line[column])) {
          ERROR(token, "invalid immediate.\n");
        }
        while (isdigit(line[column])) {
          imm = imm * 10 + (line[column++] - '0');
        }
        token->type = IMM;
        token->imm = imm;
      } else if (c == ',') {
        token->type = c;
      } else {
        ERROR(token, "invalid character: %c\n", c);
      }

      vector_push(tokens, token);
    }

    vector_push(lines, tokens);
  }

  return lines;
}

typedef unsigned char Byte;
typedef struct {
  int length, capacity;
  Byte *buffer;
} Binary;

Binary *binary_new() {
  int capacity = 0x100;
  Binary *binary = (Binary *) calloc(1, sizeof(Binary));
  binary->capacity = capacity;
  binary->buffer = (Byte *) calloc(capacity, sizeof(Byte));
  return binary;
}

void binary_push(Binary *binary, Byte byte) {
  if (binary->length >= binary->capacity) {
    binary->capacity *= 2;
    binary->buffer = realloc(binary->buffer, binary->capacity);
  }
  binary->buffer[binary->length++] = byte;
}

void binary_write(Binary *binary, void *buffer, int size) {
  for (int i = 0; i < size; i++) {
    binary_push(binary, ((Byte *) buffer)[i]);
  }
}

#define MOD_RM(Mod, RegOpcode, RM) \
  (((Mod << 6) & 0xc0) | ((RegOpcode << 3) & 0x38) | (RM & 0x07))

#define MOD_REG 3

Binary *gen_text(Vector *lines) {
  Binary *text = binary_new();

  for (int i = 0; i < lines->length; i++) {
    Vector *line = lines->array[i];
    Token **tokens = (Token **) line->array;
    int length = line->length;
    if (length == 0) continue;

    switch (tokens[0]->type) {
      case IDENT: {
        if (strcmp(tokens[0]->ident, "movq") == 0) {
          if (length < 4) {
            ERROR(tokens[0], "'movq' expects 2 operands.\n");
          }
          if (tokens[1]->type != IMM) {
            ERROR(tokens[1], "'movq' expects first operand to be immediate.\n");
          }
          if (tokens[2]->type != ',') {
            ERROR(tokens[2], "'movq' expects ',' between operands.\n");
          }
          if (tokens[3]->type != REG) {
            ERROR(tokens[3], "'movq' expects second operand to be 'rax'.\n");
          }
          if (length > 4) {
            ERROR(tokens[4], "'movq' expects 2 operands.\n");
          }

          int imm = tokens[1]->imm;
          int imm0 = imm & 0xff;
          int imm1 = (imm >> 8) & 0xff;
          int imm2 = (imm >> 16) & 0xff;
          int imm3 = imm >> 24;
          int reg = tokens[3]->reg;

          // REX.W + C7 /0 id
          Byte byte[7] = { 0x48, 0xc7, MOD_RM(MOD_REG, 0, reg), imm0, imm1, imm2, imm3 };

          binary_write(text, byte, 7);
        } else if (strcmp(tokens[0]->ident, "ret") == 0) {
          if (length > 1) {
            ERROR(tokens[1], "'ret' expects no operand.\n");
          }
          binary_push(text, 0xc3);
        } else {
          ERROR(tokens[0], "invalid instruction.\n");
        }
        break;
      }
      default: {
        ERROR(tokens[0], "invalid instruction.\n");
      }
    }
  }

  return text;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: ./as [input file] [output file]\n");
    exit(1);
  }
  char *input = argv[1];
  char *output = argv[2];

  Vector *source = scan(input);
  Vector *lines = tokenize(input, source);

  // .text
  Binary *text = gen_text(lines);

  // .symtab
  Elf64_Sym *symtab = (Elf64_Sym *) calloc(2, sizeof(Elf64_Sym));
  symtab[1].st_name = 1;
  symtab[1].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
  symtab[1].st_other = STV_DEFAULT;
  symtab[1].st_shndx = 1;
  symtab[1].st_value = 0;

  // .strtab
  char strtab[6] = "\0main\0";

  // .shstrtab
  char shstrtab[33] = "\0.text\0.symtab\0.strtab\0.shstrtab\0";

  // section header table
  Elf64_Shdr *shdrtab = (Elf64_Shdr *) calloc(5, sizeof(Elf64_Shdr));

  // section header for .text
  shdrtab[1].sh_name = 1;
  shdrtab[1].sh_type = SHT_PROGBITS;
  shdrtab[1].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
  shdrtab[1].sh_offset = sizeof(Elf64_Ehdr);
  shdrtab[1].sh_size = text->length;

  // section header for .symtab
  shdrtab[2].sh_name = 7;
  shdrtab[2].sh_type = SHT_SYMTAB;
  shdrtab[2].sh_offset = sizeof(Elf64_Ehdr) + text->length;
  shdrtab[2].sh_size = sizeof(Elf64_Sym) * 2;
  shdrtab[2].sh_link = 3;
  shdrtab[2].sh_info = 1;
  shdrtab[2].sh_entsize = sizeof(Elf64_Sym);

  // section header for .strtab
  shdrtab[3].sh_name = 15;
  shdrtab[3].sh_type = SHT_STRTAB;
  shdrtab[3].sh_offset = sizeof(Elf64_Ehdr) + text->length + sizeof(Elf64_Sym) * 2;
  shdrtab[3].sh_size = sizeof(strtab);

  // section header for .shstrtab
  shdrtab[4].sh_name = 23;
  shdrtab[4].sh_type = SHT_STRTAB;
  shdrtab[4].sh_offset = sizeof(Elf64_Ehdr) + text->length + sizeof(Elf64_Sym) * 2 + sizeof(strtab);
  shdrtab[4].sh_size = sizeof(shstrtab);

  // ELF header
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *) calloc(1, sizeof(Elf64_Ehdr));
  ehdr->e_ident[0] = 0x7f;
  ehdr->e_ident[1] = 'E';
  ehdr->e_ident[2] = 'L';
  ehdr->e_ident[3] = 'F';
  ehdr->e_ident[4] = ELFCLASS64;
  ehdr->e_ident[5] = ELFDATA2LSB;
  ehdr->e_ident[6] = EV_CURRENT;
  ehdr->e_ident[7] = ELFOSABI_NONE;
  ehdr->e_ident[8] = 0;
  ehdr->e_type = ET_REL;
  ehdr->e_machine = EM_X86_64;
  ehdr->e_version = EV_CURRENT;
  ehdr->e_shoff = sizeof(Elf64_Ehdr) + text->length + sizeof(Elf64_Sym) * 2 + sizeof(strtab) + sizeof(shstrtab);
  ehdr->e_ehsize = sizeof(Elf64_Ehdr);
  ehdr->e_shentsize = sizeof(Elf64_Shdr);
  ehdr->e_shnum = 5;
  ehdr->e_shstrndx = 4;

  // generate ELF file
  FILE *out = fopen(output, "wb");
  if (!out) {
    perror(output);
    exit(1);
  }
  fwrite(ehdr, sizeof(Elf64_Ehdr), 1, out);
  fwrite(text->buffer, text->length, 1, out);
  fwrite(symtab, sizeof(Elf64_Sym), 2, out);
  fwrite(strtab, sizeof(strtab), 1, out);
  fwrite(shstrtab, sizeof(shstrtab), 1, out);
  fwrite(shdrtab, sizeof(Elf64_Shdr), 5, out);
  fclose(out);

  return 0;
}
