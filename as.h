#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdnoreturn.h>
#include <assert.h>
#include <elf.h>

typedef struct string {
  int size, length;
  char *buffer;
} String;

extern String *string_new();
extern void string_push(String *string, char c);
extern void string_write(String *string, char *buffer);

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

extern Binary *binary_new();
extern void binary_push(Binary *binary, Byte byte);
extern void binary_append(Binary *binary, int size, ...);
extern void binary_write(Binary *binary, void *buffer, int size);

typedef enum reg_type {
  REG8,
  REG16,
  REG32,
  REG64,
} RegType;

typedef enum reg {
  AX = 0,
  CX = 1,
  DX = 2,
  BX = 3,
  SP = 4,
  BP = 5,
  SI = 6,
  DI = 7,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11,
  R12 = 12,
  R13 = 13,
  R14 = 14,
  R15 = 15,
} Reg;

typedef enum token_type {
  TOK_IDENT,
  TOK_REG,
  TOK_NUM,
  TOK_IMM,
  TOK_COMMA,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_SEMICOLON,
} TokenType;

typedef struct token {
  TokenType type;
  char *ident;
  RegType regtype;
  Reg regcode;
  int num;
  unsigned int imm;
  char *file;
  int lineno;
  int column;
  char *line;
} Token;

typedef enum scale {
  SCALE1 = 0,
  SCALE2 = 1,
  SCALE4 = 2,
  SCALE8 = 3,
} Scale;

typedef enum op_type {
  OP_REG,
  OP_MEM,
  OP_SYM,
  OP_IMM,
} OpType;

typedef struct op {
  OpType type;
  RegType regtype;
  Reg regcode;
  bool sib;
  Scale scale;
  Reg index;
  Reg base;
  int disp;
  char *ident;
  unsigned int imm;
  Token *token;
} Op;

typedef enum inst_type {
  INST_PUSH,
  INST_POP,
  INST_MOV,
  INST_LEA,
  INST_ADD,
  INST_SUB,
  INST_MUL,
  INST_IMUL,
  INST_DIV,
  INST_IDIV,
  INST_JMP,
  INST_CALL,
  INST_LEAVE,
  INST_RET,
} InstType;

typedef enum inst_suffix {
  INST_BYTE,
  INST_WORD,
  INST_LONG,
  INST_QUAD,
} InstSuffix;

typedef struct inst {
  InstType type;
  InstSuffix suffix;
  Op *op;
  Op *src;
  Op *dest;
  Token *token;
} Inst;

typedef struct symbol {
  bool global;
  bool undef;
  int inst;
} Symbol;

typedef struct unit {
  Vector *insts;
  Map *symbols;
} Unit;

typedef struct section {
  Binary *text;
  Binary *symtab;
  String *strtab;
  Binary *rela_text;
} Section;

extern noreturn void errorf(char *file, int lineno, int column, char *line, char *__file, int __lineno, char *format, ...);

#define ERROR(token, ...) \
  do { \
    Token *t = (token); \
    errorf(t->file, t->lineno, t->column, t->line, __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)
