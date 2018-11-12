#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdnoreturn.h>
#include <elf.h>

typedef struct string {
  int size, length;
  char *buffer;
} String;

extern String *string_new();
extern void string_push(String *string, char c);

void string_write(String *string, char *buffer) {
  for (int i = 0; buffer[i]; i++) {
    string_push(string, buffer[i]);
  }
}

typedef struct vector {
  int size, length;
  void **array;
} Vector;

extern Vector *vector_new();
extern void vector_push(Vector *vector, void *value);

typedef struct map {
  int count;
  char *keys[1024];
  void *values[1024];
} Map;

extern Map *map_new();
extern bool map_put(Map *map, char *key, void *value);
extern void *map_lookup(Map *map, char *key);

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
  TOK_DISP,
  TOK_IMM,
  TOK_COMMA,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_SEMICOLON,
};

typedef struct token Token;
struct token {
  TokenType type;
  char *ident;
  int reg;
  int disp;
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

typedef enum op_type {
  OP_REG,
  OP_MEM,
  OP_SYM,
  OP_IMM,
} OpType;

typedef struct op {
  OpType type;
  int reg;
  int base;
  int disp;
  char *sym;
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

Op *op_mem(int base, int disp, Token *token) {
  Op *op = op_new(OP_MEM, token);
  op->base = base;
  op->disp = disp;
  return op;
}

Op *op_sym(char *sym, Token *token) {
  Op *op = op_new(OP_SYM, token);
  op->sym = sym;
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
  INST_CALL,
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

typedef struct label {
  int inst;
} Label;

Label *label_new(int inst) {
  Label *label = (Label *) calloc(1, sizeof(Label));
  label->inst = inst;
  return label;
}

typedef struct unit {
  Vector *insts;
  Map *labels;
  Binary *text;
  Vector *relocs;
  int *addrs;
  Binary *symtab;
  String *strtab;
  Map *gsyms;
  Binary *rela_text;
} Unit;

void parse(Unit *unit, Vector *lines) {
  Vector *insts = vector_new();
  Map *labels = map_new();

  for (int i = 0; i < lines->length; i++) {
    Vector *line = lines->array[i];
    if (line->length == 0) continue;

    Token **token = (Token **) line->array;
    Token *head = *token++;

    if (head->type != TOK_IDENT) {
      ERROR(head, "invalid instruction.\n");
    }

    if (*token && (*token)->type == TOK_SEMICOLON) {
      token++;
      if (*token) {
        ERROR(*token, "invalid symbol declaration.\n");
      }
      if (map_lookup(labels, head->ident)) {
        ERROR(head, "duplicated symbol declaration: %s.\n", head->ident);
      }
      map_put(labels, head->ident, label_new(insts->length));
      continue;
    }

    InstType type;
    if (strcmp(head->ident, "pushq") == 0) {
      type = INST_PUSH;
    } else if (strcmp(head->ident, "popq") == 0) {
      type = INST_POP;
    } else if (strcmp(head->ident, "movq") == 0) {
      type = INST_MOV;
    } else if (strcmp(head->ident, "call") == 0) {
      type = INST_CALL;
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
            token++;
            vector_push(ops, op_reg(reg, op_head));
          }
          break;

          case TOK_LPAREN: {
            token++;
            if ((*token)->type != TOK_REG) {
              ERROR(*token, "register is expected.\n");
            }
            int base = (*token)->reg;
            token++;
            if ((*token)->type != TOK_RPAREN) {
              ERROR(*token, "')' is expected.\n");
            }
            token++;
            vector_push(ops, op_mem(base, 0, op_head));
          }
          break;

          case TOK_DISP: {
            int disp = (*token)->disp;
            token++;
            if ((*token)->type != TOK_LPAREN) {
              ERROR(*token, "'(' is expected.\n");
            }
            token++;
            if ((*token)->type != TOK_REG) {
              ERROR(*token, "register is expected.\n");
            }
            int base = (*token)->reg;
            token++;
            if ((*token)->type != TOK_RPAREN) {
              ERROR(*token, "')' is expected.\n");
            }
            token++;
            vector_push(ops, op_mem(base, disp, op_head));
          }
          break;

          case TOK_IDENT: {
            char *sym = (*token)->ident;
            token++;
            vector_push(ops, op_sym(sym, op_head));
          }
          break;

          case TOK_IMM: {
            int imm = (*token)->imm;
            token++;
            vector_push(ops, op_imm(imm, op_head));
          }
          break;

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

  unit->insts = insts;
  unit->labels = labels;
}

typedef struct reloc {
  int offset;
  char *sym;
  Token *token;
} Reloc;

Reloc *reloc_new(int offset, char *sym, Token *token) {
  Reloc *reloc = (Reloc *) calloc(1, sizeof(Reloc));
  reloc->offset = offset;
  reloc->sym = sym;
  reloc->token = token;
  return reloc;
}

#define REXW_PRE(R, X, B) \
  (0x48 | (((R >> 3) & 1) << 2) | (((X >> 3) & 1) << 1) | ((B >> 3) & 1))

#define REX_PRE(R, X, B) \
  (0x40 | (((R >> 3) & 1) << 2) | (((X >> 3) & 1) << 1) | ((B >> 3) & 1))

#define MOD_RM(Mod, RegOpcode, RM) \
  (((Mod << 6) & 0xc0) | ((RegOpcode << 3) & 0x38) | (RM & 0x07))

#define MOD_DISP0 0
#define MOD_DISP8 1
#define MOD_DISP32 2
#define MOD_REG 3

#define MOD_MEM(disp) \
  (disp == 0 ? MOD_DISP0 : (-128 <= disp && disp < 128 ? MOD_DISP8 : MOD_DISP32))

#define OPCODE_REG(Opcode, Reg) \
  ((Opcode & 0xf8) | (Reg & 0x07))

#define IMM_ID0(id) (id & 0xff)
#define IMM_ID1(id) ((id >> 8) & 0xff)
#define IMM_ID2(id) ((id >> 16) & 0xff)
#define IMM_ID3(id) (id >> 24)

void gen_text(Unit *unit) {
  Vector *insts = unit->insts;
  Binary *text = binary_new();
  int *addrs = (int *) calloc(insts->length, sizeof(int));
  Vector *relocs = vector_new();

  for (int i = 0; i < insts->length; i++) {
    Inst *inst = insts->array[i];
    addrs[i] = text->length;

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
          int mod = MOD_MEM(dest->disp);
          Byte rex = REXW_PRE(src->reg, 0, dest->base);
          Byte opcode = 0x89;
          Byte mod_rm = MOD_RM(mod, src->reg, dest->base);
          if (mod == MOD_DISP0) {
            binary_append(text, 3, rex, opcode, mod_rm);
          } else if (mod == MOD_DISP8) {
            Byte disp = (signed char) dest->disp;
            binary_append(text, 4, rex, opcode, mod_rm, disp);
          } else if (mod == MOD_DISP32) {
            Byte disp0 = ((unsigned int) dest->disp) & 0xff;
            Byte disp1 = (((unsigned int) dest->disp) >> 8) & 0xff;
            Byte disp2 = (((unsigned int) dest->disp) >> 16) & 0xff;
            Byte disp3 = (((unsigned int) dest->disp) >> 24) & 0xff;
            binary_append(text, 7, rex, opcode, mod_rm, disp0, disp1, disp2, disp3);
          }
        } else if (src->type == OP_MEM && dest->type == OP_REG) {
          if (src->base == 4) {
            ERROR(src->token, "rsp is not supported.\n");
          }
          if (src->base == 5) {
            ERROR(src->token, "rbp is not supported.\n");
          }
          // REX.W + 8B /r
          int mod = MOD_MEM(src->disp);
          Byte rex = REXW_PRE(dest->reg, 0, src->base);
          Byte opcode = 0x8b;
          Byte mod_rm = MOD_RM(mod, dest->reg, src->base);
          if (mod == MOD_DISP0) {
            binary_append(text, 3, rex, opcode, mod_rm);
          } else if (mod == MOD_DISP8) {
            Byte disp = (signed char) src->disp;
            binary_append(text, 4, rex, opcode, mod_rm, disp);
          } else if (mod == MOD_DISP32) {
            Byte disp0 = ((unsigned int) src->disp) & 0xff;
            Byte disp1 = (((unsigned int) src->disp) >> 8) & 0xff;
            Byte disp2 = (((unsigned int) src->disp) >> 16) & 0xff;
            Byte disp3 = (((unsigned int) src->disp) >> 24) & 0xff;
            binary_append(text, 7, rex, opcode, mod_rm, disp0, disp1, disp2, disp3);
          }
        } else {
          ERROR(src->token, "invalid operand types.\n");
        }
      }
      break;

      case INST_CALL: {
        if (inst->ops->length != 1) {
          ERROR(inst->token, "'call' expects 1 operand.\n");
        }
        Op *op = inst->ops->array[0];
        if (op->type == OP_SYM) {
          // E8 cd
          Byte opcode = 0xe8;
          binary_append(text, 5, opcode, 0, 0, 0, 0);
          vector_push(relocs, reloc_new(text->length - 4, op->sym, op->token));
        } else {
          ERROR(inst->token, "invalid operand type.\n");
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

  unit->text = text;
  unit->relocs = relocs;
  unit->addrs = addrs;
}

void gen_symtab(Unit *unit) {
  Map *labels = unit->labels;
  int *addrs = unit->addrs;
  Binary *symtab = binary_new();
  String *strtab = string_new();
  Map *gsyms = map_new();

  binary_write(symtab, calloc(1, sizeof(Elf64_Sym)), sizeof(Elf64_Sym));
  string_push(strtab, '\0');

  for (int i = 0; i < labels->count; i++) {
    char *key = labels->keys[i];
    Label *val = labels->values[i];

    Elf64_Sym *sym = (Elf64_Sym *) calloc(1, sizeof(Elf64_Sym));
    sym->st_name = strtab->length;
    sym->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
    sym->st_other = STV_DEFAULT;
    sym->st_shndx = 1;
    sym->st_value = addrs[val->inst];

    binary_write(symtab, sym, sizeof(Elf64_Sym));
    string_write(strtab, key);
    string_push(strtab, '\0');
    map_put(gsyms, key, (void *) (intptr_t) (i + 1));
  }

  unit->symtab = symtab;
  unit->strtab = strtab;
  unit->gsyms = gsyms;
}

void gen_rela_text(Unit *unit) {
  Vector *relocs = unit->relocs;
  Map *gsyms = unit->gsyms;
  Binary *rela_text = binary_new();

  for (int i = 0; i < relocs->length; i++) {
    Reloc *reloc = relocs->array[i];
    int index = (int) (intptr_t) map_lookup(gsyms, reloc->sym);
    if (index == 0) {
      ERROR(reloc->token, "undefined symbol: %s.\n", reloc->sym);
    }

    Elf64_Rela *rela = (Elf64_Rela *) calloc(1, sizeof(Elf64_Rela));
    rela->r_offset = reloc->offset;
    rela->r_info = ELF64_R_INFO(index, R_X86_64_PC32);
    rela->r_addend = -4;

    binary_write(rela_text, rela, sizeof(Elf64_Rela));
  }

  unit->rela_text = rela_text;
}

#define SHNUM 6
#define TEXT 1
#define SYMTAB 2
#define STRTAB 3
#define RELA_TEXT 4
#define SHSTRTAB 5

void gen_elf(Unit *unit, char *output) {
  Binary *text = unit->text;
  Binary *symtab = unit->symtab;
  String *strtab = unit->strtab;
  Binary *rela_text = unit->rela_text;

  // .shstrtab
  String *shstrtab = string_new();
  int names[SHNUM];
  char *sections[SHNUM] = {
    "", ".text", ".symtab", ".strtab", ".rela.text", ".shstrtab"
  };
  for (int i = 0; i < SHNUM; i++) {
    names[i] = shstrtab->length;
    string_write(shstrtab, sections[i]);
    string_push(shstrtab, '\0');
  }

  // section header table
  int offset = sizeof(Elf64_Ehdr);
  Elf64_Shdr *shdrtab = (Elf64_Shdr *) calloc(SHNUM, sizeof(Elf64_Shdr));

  // section header for .text
  shdrtab[TEXT].sh_name = names[TEXT];
  shdrtab[TEXT].sh_type = SHT_PROGBITS;
  shdrtab[TEXT].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
  shdrtab[TEXT].sh_offset = offset;
  shdrtab[TEXT].sh_size = text->length;
  offset += text->length;

  // section header for .symtab
  shdrtab[SYMTAB].sh_name = names[SYMTAB];
  shdrtab[SYMTAB].sh_type = SHT_SYMTAB;
  shdrtab[SYMTAB].sh_offset = offset;
  shdrtab[SYMTAB].sh_size = symtab->length;
  shdrtab[SYMTAB].sh_link = STRTAB;
  shdrtab[SYMTAB].sh_info = 1;
  shdrtab[SYMTAB].sh_entsize = sizeof(Elf64_Sym);
  offset += symtab->length;

  // section header for .strtab
  shdrtab[STRTAB].sh_name = names[STRTAB];
  shdrtab[STRTAB].sh_type = SHT_STRTAB;
  shdrtab[STRTAB].sh_offset = offset;
  shdrtab[STRTAB].sh_size = strtab->length;
  offset += strtab->length;

  // section header for .rela.text
  if (rela_text->length > 0) {
    shdrtab[RELA_TEXT].sh_name = names[RELA_TEXT];
    shdrtab[RELA_TEXT].sh_type = SHT_RELA;
    shdrtab[RELA_TEXT].sh_flags = SHF_INFO_LINK;
    shdrtab[RELA_TEXT].sh_offset = offset;
    shdrtab[RELA_TEXT].sh_size = rela_text->length;
    shdrtab[RELA_TEXT].sh_link = SYMTAB;
    shdrtab[RELA_TEXT].sh_info = TEXT;
    shdrtab[RELA_TEXT].sh_entsize = sizeof(Elf64_Rela);
    offset += rela_text->length;
  }

  // section header for .shstrtab
  shdrtab[SHSTRTAB].sh_name = names[SHSTRTAB];
  shdrtab[SHSTRTAB].sh_type = SHT_STRTAB;
  shdrtab[SHSTRTAB].sh_offset = offset;
  shdrtab[SHSTRTAB].sh_size = shstrtab->length;
  offset += shstrtab->length;

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
  ehdr->e_shoff = offset;
  ehdr->e_ehsize = sizeof(Elf64_Ehdr);
  ehdr->e_shentsize = sizeof(Elf64_Shdr);
  ehdr->e_shnum = SHNUM;
  ehdr->e_shstrndx = SHSTRTAB;

  // generate ELF file
  FILE *out = fopen(output, "wb");
  if (!out) {
    perror(output);
    exit(1);
  }
  fwrite(ehdr, sizeof(Elf64_Ehdr), 1, out);
  fwrite(text->buffer, text->length, 1, out);
  fwrite(symtab->buffer, symtab->length, 1, out);
  fwrite(strtab->buffer, strtab->length, 1, out);
  fwrite(rela_text->buffer, rela_text->length, 1, out);
  fwrite(shstrtab->buffer, shstrtab->length, 1, out);
  fwrite(shdrtab, sizeof(Elf64_Shdr), SHNUM, out);
  fclose(out);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s [input file] [output file]\n", argv[0]);
    exit(1);
  }
  char *input = argv[1];
  char *output = argv[2];

  Vector *source = scan(input);
  Vector *lines = tokenize(input, source);

  Unit *unit = (Unit *) calloc(1, sizeof(Unit));
  parse(unit, lines);

  // .text
  gen_text(unit);

  // .symtab, .strtab
  gen_symtab(unit);

  // .rela.text
  gen_rela_text(unit);

  // gen ELF file
  gen_elf(unit, output);

  return 0;
}
