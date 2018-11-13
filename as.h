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

typedef enum token_type {
  TOK_IDENT,
  TOK_REG,
  TOK_DISP,
  TOK_IMM,
  TOK_COMMA,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_SEMICOLON,
} TokenType;

typedef struct token {
  TokenType type;
  char *ident;
  int reg;
  int disp;
  int imm;
  char *file;
  int lineno;
  int column;
  char *line;
} Token;

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

typedef struct label {
  int inst;
} Label;

typedef struct reloc {
  int offset;
  char *sym;
  Token *token;
} Reloc;

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

extern noreturn void errorf(char *file, int lineno, int column, char *line, char *__file, int __lineno, char *format, ...);

#define ERROR(token, ...) \
  do { \
    Token *t = (token); \
    errorf(t->file, t->lineno, t->column, t->line, __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)
