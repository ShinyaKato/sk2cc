#include "sk2cc.h"

// string, vector, map, binary
#include "string.h"
#include "vector.h"
#include "map.h"
#include "binary.h"

// struct and enum declaration

// register size
typedef enum {
  REG_BYTE, // 8-bit
  REG_WORD, // 16-bit
  REG_LONG, // 32-bit
  REG_QUAD, // 64-bit
} RegSize;

// register code
// the values of constants are important
// because they are used in the instruction encoding.
typedef enum {
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
} RegCode;

// token type
typedef enum {
  TK_IDENT,     // identifier
  TK_RIP,       // RIP register
  TK_REG,       // register
  TK_NUM,       // number (displacement, scale, etc.)
  TK_IMM,       // immediate value
  TK_STR,       // string (.ascii directive, etc.)
  TK_COMMA,     // ','
  TK_LPAREN,    // '('
  TK_RPAREN,    // ')'
  TK_SEMICOLON, // ';'
} TokenType;

// source code location
// this informatino is used for error report.
typedef struct location {
  char *filename; // source file name
  char *line_ptr; // pointer to the line head
  int lineno;     // 0-indexed
  int column;     // 0-indexed
} Location;

// token
typedef struct {
  TokenType type; // token type

  // identifier
  char *ident;

  // register
  RegSize regtype;
  RegCode regcode;

  // number
  int num;

  // immediate value
  unsigned int imm;

  // string
  char *string;
  int length;

  Location *loc; // location information
} Token;

// label
typedef struct {
  char *ident;
  Token *token;
} Label;

// directive type
typedef enum {
  DIR_TEXT,    // .text
  DIR_DATA,    // .data
  DIR_SECTION, // .section
  DIR_GLOBAL,  // .global
  DIR_ZERO,    // .zero
  DIR_LONG,    // .long
  DIR_QUAD,    // .quad
  DIR_ASCII,   // .ascii
} DirType;

// directive
typedef struct {
  DirType type; // directive type
  char *ident;  // identifier
  int num;      // number
  char *string; // string
  int length;   // string length
  Token *token;
} Dir;

// scale of memory operand
// the values of constants are important
// because they are used in instruction encoding.
typedef enum {
  SCALE1,
  SCALE2,
  SCALE4,
  SCALE8,
} Scale;

typedef enum {
  OP_REG, // register operand
  OP_MEM, // memory operand
  OP_SYM, // symbol operand
  OP_IMM, // immediate operand
} OpType;

// operand
typedef struct {
  OpType type; // operand type

  // register operand
  RegSize regtype; // register type
  RegCode regcode;     // register code

  // memory operand
  bool rip;      // RIP-relative addressing?
  bool sib;      // needs scale-index-base byte?
  Scale scale;   // scale
  RegCode index; // index register code
  RegCode base;  // base register code
  int disp;      // address displacement

  // symbol operand
  char *ident; // identifier

  // immediate operand
  unsigned int imm;

  Token *token;
} Op;

// instruction type
typedef enum {
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

// instructino suffix
typedef enum {
  INST_BYTE,
  INST_WORD,
  INST_LONG,
  INST_QUAD,
} InstSuffix;

// instruction
typedef struct {
  InstType type;     // instruction type
  InstSuffix suffix; // instruction suffix

  // instruction with one operand
  Op *op;

  // instruction with two operands
  Op *src;
  Op *dest;

  Token *token;
} Inst;

// assembly statement type
// a statement is one of label, directive or instruction.
typedef enum {
  STMT_LABEL,
  STMT_DIR,
  STMT_INST,
} StmtType;

// assembly statement
typedef struct {
  StmtType type;
  Label *label;
  Dir *dir;
  Inst *inst;
} Stmt;

// relocation
// some relocs are resolved when generating object file.
// others are stored in the relocation table of object file and resolved by a linker.
typedef struct {
  int offset;
  char *ident;
  int type;
  int addend;
} Reloc;

// section
typedef struct {
  Binary *bin;
  Vector *relocs;
} Section;

// symbol
typedef struct {
  bool global; // a global symbol can be referenced by other object files
  int section; // section index
  int offset;  // offset int the section
} Symbol;

// translation unit
typedef struct {
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
