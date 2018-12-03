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
  TOK_RIP,
  TOK_REG,
  TOK_NUM,
  TOK_IMM,
  TOK_STR,
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
  char *string;
  int length;
  char *file;
  int lineno;
  int column;
  char *line;
} Token;

typedef struct label {
  char *ident;
  Token *token;
} Label;

typedef enum dir_type {
  DIR_GLOBAL,
  DIR_ASCII,
} DirType;

typedef struct dir {
  DirType type;
  char *ident;
  char *string;
  int length;
  Token *token;
} Dir;

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
  bool rip;
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
  INST_MOVZB,
  INST_MOVZW,
  INST_MOVSB,
  INST_MOVSW,
  INST_MOVSL,
  INST_LEA,
  INST_NEG,
  INST_NOT,
  INST_ADD,
  INST_SUB,
  INST_MUL,
  INST_IMUL,
  INST_DIV,
  INST_IDIV,
  INST_AND,
  INST_XOR,
  INST_OR,
  INST_SAL,
  INST_SAR,
  INST_CMP,
  INST_SETE,
  INST_SETNE,
  INST_SETL,
  INST_SETG,
  INST_SETLE,
  INST_SETGE,
  INST_JMP,
  INST_JE,
  INST_JNE,
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

typedef enum stmt_type {
  STMT_LABEL,
  STMT_DIR,
  STMT_INST,
} StmtType;

typedef struct stmt {
  StmtType type;
  Label *label;
  Dir *dir;
  Inst *inst;
} Stmt;

typedef struct reloc {
  int offset;
  char *ident;
} Reloc;

typedef struct symbol {
  bool global;
  int section;
  int offset;
} Symbol;

typedef struct trans_unit {
  Binary *text;
  Vector *relocs;
  Map *symbols;
} TransUnit;

extern noreturn void errorf(char *file, int lineno, int column, char *line, char *__file, int __lineno, char *format, ...);

extern Vector *scan(char *file);
extern Vector *tokenize(char *file, Vector *source);
extern Vector *parse(Vector *lines);
extern TransUnit *encode(Vector *stmts);
extern void gen_obj(TransUnit *trans_unit, char *output);

#define SHNUM 6
#define UNDEF 0
#define TEXT 1
#define SYMTAB 2
#define STRTAB 3
#define RELA_TEXT 4
#define SHSTRTAB 5

#define ERROR(token, ...) \
  do { \
    Token *t = (token); \
    errorf(t->file, t->lineno, t->column, t->line, __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)
