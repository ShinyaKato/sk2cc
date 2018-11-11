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
  TOK_IDENT,
  TOK_REG,
  TOK_IMM,
  TOK_COMMA,
  TOK_LPAREN,
  TOK_RPAREN,
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
      } else {
        ERROR(token, "invalid character: %c\n", c);
      }

      vector_push(tokens, token);
    }

    vector_push(lines, tokens);
  }

  return lines;
}

typedef enum op_type {
  OP_REG,
  OP_MEM,
  OP_IMM,
} OpType;

typedef struct op {
  OpType type;
  int reg;
  int base;
  int imm;
  Token *token;
} Op;

Op *op_new(OpType type, Token *token) {
  Op *op = (Op *) calloc(1, sizeof(Op));
  op->type = type;
  op->token = token;
  return op;
}

Op *op_reg(int reg, Token *token) {
  Op *op = op_new(OP_REG, token);
  op->reg = reg;
  return op;
}

Op *op_mem(int base, Token *token) {
  Op *op = op_new(OP_MEM, token);
  op->base = base;
  return op;
}

Op *op_imm(int imm, Token *token) {
  Op *op = op_new(OP_IMM, token);
  op->imm = imm;
  return op;
}

typedef enum inst_type {
  INST_PUSH,
  INST_POP,
  INST_MOV,
  INST_LEAVE,
  INST_RET,
} InstType;

typedef struct inst {
  InstType type;
  Vector *ops;
  Token *token;
} Inst;

Inst *inst_new(InstType type, Vector *ops, Token *token) {
  Inst *inst = (Inst *) calloc(1, sizeof(Inst));
  inst->type = type;
  inst->ops = ops;
  return inst;
}

Vector *parse(Vector *lines) {
  Vector *insts = vector_new();

  for (int i = 0; i < lines->length; i++) {
    Vector *line = lines->array[i];
    if (line->length == 0) continue;

    Token **token = (Token **) line->array;
    Token *head = *token++;

    if (head->type != TOK_IDENT) {
      ERROR(head, "invalid instruction.\n");
    }

    InstType type;
    if (strcmp(head->ident, "pushq") == 0) {
      type = INST_PUSH;
    } else if (strcmp(head->ident, "popq") == 0) {
      type = INST_POP;
    } else if (strcmp(head->ident, "movq") == 0) {
      type = INST_MOV;
    } else if (strcmp(head->ident, "leave") == 0) {
      type = INST_LEAVE;
    } else if (strcmp(head->ident, "ret") == 0) {
      type = INST_RET;
    } else {
      ERROR(head, "invalide instruction.\n");
    }

    Vector *ops = vector_new();
    if (*token) {
      while (1) {
        Token *op_head = *token;
        switch ((*token)->type) {
          case TOK_REG: {
            int reg = (*token)->reg;
            vector_push(ops, op_reg(reg, op_head));
            token++;
            break;
          }
          case TOK_LPAREN: {
            token++;
            if ((*token)->type != TOK_REG) {
              ERROR(*token, "register is expected.\n");
            }
            int base = (*token)->reg;
            vector_push(ops, op_mem(base, op_head));
            token++;
            if ((*token)->type != TOK_RPAREN) {
              ERROR(*token, "')' is expected.\n");
            }
            token++;
            break;
          }
          case TOK_IMM: {
            int imm = (*token)->imm;
            vector_push(ops, op_imm(imm, op_head));
            token++;
            break;
          }
          default: {
            ERROR(*token, "invalid operand.\n");
          }
        }

        if (!*token) {
          break;
        }
        if ((*token)->type != TOK_COMMA) {
          ERROR(*token, "',' is expected.\n");
        }
        token++;
        if (!*token) {
          ERROR(*token, "invalid operand.\n");
        }
      }
    }

    vector_push(insts, inst_new(type, ops, head));
  }

  return insts;
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

void binary_append(Binary *binary, int size, ...) {
  va_list ap;
  va_start(ap, size);
  for (int i = 0; i < size; i++) {
    Byte byte = va_arg(ap, int);
    binary_push(binary, byte);
  }
  va_end(ap);
}

void binary_write(Binary *binary, void *buffer, int size) {
  for (int i = 0; i < size; i++) {
    binary_push(binary, ((Byte *) buffer)[i]);
  }
}

#define REXW_PRE(R, X, B) \
  (0x48 | (((R >> 3) & 1) << 2) | (((X >> 3) & 1) << 1) | ((B >> 3) & 1))

#define REX_PRE(R, X, B) \
  (0x40 | (((R >> 3) & 1) << 2) | (((X >> 3) & 1) << 1) | ((B >> 3) & 1))

#define MOD_RM(Mod, RegOpcode, RM) \
  (((Mod << 6) & 0xc0) | ((RegOpcode << 3) & 0x38) | (RM & 0x07))

#define MOD_MEM 0
#define MOD_REG 3

#define OPCODE_REG(Opcode, Reg) \
  ((Opcode & 0xf8) | (Reg & 0x07))

#define IMM_ID0(id) (id & 0xff)
#define IMM_ID1(id) ((id >> 8) & 0xff)
#define IMM_ID2(id) ((id >> 16) & 0xff)
#define IMM_ID3(id) (id >> 24)

Binary *gen_text(Vector *insts) {
  Binary *text = binary_new();

  for (int i = 0; i < insts->length; i++) {
    Inst *inst = insts->array[i];

    switch (inst->type) {
      case INST_PUSH: {
        if (inst->ops->length != 1) {
          ERROR(inst->token, "'pushq' expects 1 operand.\n");
        }
        Op *op = inst->ops->array[0];

        if (op->type == OP_REG) {
          // 50 +rd
          Byte rex = REX_PRE(0, 0, op->reg);
          Byte opcode = OPCODE_REG(0x50, op->reg);
          if (op->reg < 8) {
            binary_append(text, 1, opcode);
          } else {
            binary_append(text, 2, rex, opcode);
          }
        } else {
          ERROR(op->token, "invalid operand type.\n");
        }
      }
      break;

      case INST_POP: {
        if (inst->ops->length != 1) {
          ERROR(inst->token, "'popq' expects 1 operand.\n");
        }
        Op *op = inst->ops->array[0];

        if (op->type == OP_REG) {
          // 58 +rd
          Byte rex = REX_PRE(0, 0, op->reg);
          Byte opcode = OPCODE_REG(0x58, op->reg);
          if (op->reg < 8) {
            binary_append(text, 1, opcode);
          } else {
            binary_append(text, 2, rex, opcode);
          }
        } else {
          ERROR(op->token, "invalid operand type.\n");
        }
      }
      break;

      case INST_MOV: {
        if (inst->ops->length != 2) {
          ERROR(inst->token, "'movq' expects 2 operands.\n");
        }
        Op *src = inst->ops->array[0];
        Op *dest = inst->ops->array[1];

        if (src->type == OP_IMM && dest->type == OP_REG) {
          // REX.W + C7 /0 id
          Byte rex = REXW_PRE(0, 0, dest->reg);
          Byte opcode = 0xc7;
          Byte mod_rm = MOD_RM(MOD_REG, 0, dest->reg);
          Byte imm0 = IMM_ID0(src->imm);
          Byte imm1 = IMM_ID1(src->imm);
          Byte imm2 = IMM_ID2(src->imm);
          Byte imm3 = IMM_ID3(src->imm);
          binary_append(text, 7, rex, opcode, mod_rm, imm0, imm1, imm2, imm3);
        } else if (src->type == OP_REG && dest->type == OP_REG) {
          // REX.W + 8B /r
          Byte rex = REXW_PRE(dest->reg, 0, src->reg);
          Byte opcode = 0x8b;
          Byte mod_rm = MOD_RM(MOD_REG, dest->reg, src->reg);
          binary_append(text, 3, rex, opcode, mod_rm);
        } else if (src->type == OP_REG && dest->type == OP_MEM) {
          if (dest->base == 4) {
            ERROR(dest->token, "rsp is not supported.\n");
          }
          if (dest->base == 5) {
            ERROR(dest->token, "rbp is not supported.\n");
          }
          // REX.W + 89 /r
          Byte rex = REXW_PRE(src->reg, 0, dest->base);
          Byte opcode = 0x89;
          Byte mod_rm = MOD_RM(MOD_MEM, src->reg, dest->base);
          binary_append(text, 3, rex, opcode, mod_rm);
        } else if (src->type == OP_MEM && dest->type == OP_REG) {
          if (src->base == 4) {
            ERROR(src->token, "rsp is not supported.\n");
          }
          if (src->base == 5) {
            ERROR(src->token, "rbp is not supported.\n");
          }
          // REX.W + 8B /r
          Byte rex = REXW_PRE(dest->reg, 0, src->base);
          Byte opcode = 0x8b;
          Byte mod_rm = MOD_RM(MOD_MEM, dest->reg, src->base);
          binary_append(text, 3, rex, opcode, mod_rm);
        } else {
          ERROR(src->token, "invalid operand types.\n");
        }
      }
      break;

      case INST_LEAVE: {
        if (inst->ops->length != 0) {
          ERROR(inst->token, "'leave' expects no operand.\n");
        }

        // C9
        Byte opcode = 0xc9;
        binary_append(text, 1, opcode);
      }
      break;

      case INST_RET: {
        if (inst->ops->length != 0) {
          ERROR(inst->token, "'ret' expects no operand.\n");
        }

        // C3
        Byte opcode = 0xc3;
        binary_append(text, 1, opcode);
      }
      break;

      default: {
        ERROR(inst->token, "unknown instruction.\n");
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
  Vector *insts = parse(lines);

  // .text
  Binary *text = gen_text(insts);

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
