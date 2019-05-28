#include "sk2cc.h"

// string, vector, map, binary
#include "string.h"
#include "vector.h"
#include "map.h"
#include "binary.h"

// struct and enum declaration
typedef enum reg_type {
  REG_BYTE,
  REG_WORD,
  REG_LONG,
  REG_QUAD,
} RegType;

typedef enum reg {
  REG_AX,
  REG_CX,
  REG_DX,
  REG_BX,
  REG_SP,
  REG_BP,
  REG_SI,
  REG_DI,
  REG_R8,
  REG_R9,
  REG_R10,
  REG_R11,
  REG_R12,
  REG_R13,
  REG_R14,
  REG_R15,
} Reg;

typedef enum token_type {
  TK_IDENT,
  TK_RIP,
  TK_REG,
  TK_NUM,
  TK_IMM,
  TK_STR,
  TK_COMMA,
  TK_LPAREN,
  TK_RPAREN,
  TK_SEMICOLON,
} TokenType;

typedef struct location {
  char *filename; // source file name
  char *line_ptr; // pointer to the line head
  int lineno;     // 1-indexed
  int column;     // 1-indexed
} Location;

typedef struct token {
  TokenType type;
  char *ident;
  RegType regtype;
  Reg regcode;
  int num;
  unsigned int imm;
  char *string;
  int length;
  Location *loc;
} Token;

typedef struct label {
  char *ident;
  Token *token;
} Label;

typedef enum dir_type {
  DIR_TEXT,
  DIR_DATA,
  DIR_SECTION,
  DIR_GLOBAL,
  DIR_ZERO,
  DIR_LONG,
  DIR_QUAD,
  DIR_ASCII,
} DirType;

typedef struct dir {
  DirType type;
  char *ident;
  int num;
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
  INST_SETB,
  INST_SETL,
  INST_SETG,
  INST_SETBE,
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
  int type;
  int addend;
} Reloc;

typedef struct section {
  Binary *bin;
  Vector *relocs;
} Section;

typedef struct symbol {
  bool global;
  int section;
  int offset;
} Symbol;

typedef struct trans_unit {
  Section *text;
  Section *data;
  Section *rodata;
  Map *symbols;
} TransUnit;

// as_error.c
#define ERROR(token, ...) \
  do { \
    as_error((token)->loc,  __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)

extern noreturn void as_error(Location *loc, char *file, int lineno, char *format, ...);

// as_lex.c
extern Vector *as_tokenize(char *file);

// as_parse.c
extern Vector *as_parse(Vector *lines);

// as_encode.c
extern TransUnit *as_encode(Vector *stmts);

// as_gen.c
#define SHNUM 10
#define UNDEF 0
#define TEXT 1
#define RELA_TEXT 2
#define DATA 3
#define RELA_DATA 4
#define RODATA 5
#define RELA_RODATA 6
#define SYMTAB 7
#define STRTAB 8
#define SHSTRTAB 9

extern void gen_obj(TransUnit *trans_unit, char *output);
