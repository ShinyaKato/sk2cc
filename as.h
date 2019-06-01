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
  TK_NEWLINE,   // '\n'
  TK_EOF,       // end of file
} TokenType;

// source code location
// this informatino is used for error report.
typedef struct location {
  char *filename; // source file name
  char *line;     // pointer to the line head
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
  String *string;

  Location *loc; // location information
} Token;

// statement type
typedef enum {
  // label
  ST_LABEL,

  // directives
  ST_TEXT,
  ST_DATA,
  ST_SECTION,
  ST_GLOBAL,
  ST_ZERO,
  ST_LONG,
  ST_QUAD,
  ST_ASCII,

  // instructions
  ST_PUSH,
  ST_POP,
  ST_MOV,
  ST_MOVZB,
  ST_MOVZW,
  ST_MOVSB,
  ST_MOVSW,
  ST_MOVSL,
  ST_LEA,
  ST_NEG,
  ST_NOT,
  ST_ADD,
  ST_SUB,
  ST_MUL,
  ST_IMUL,
  ST_DIV,
  ST_IDIV,
  ST_AND,
  ST_XOR,
  ST_OR,
  ST_SAL,
  ST_SAR,
  ST_CMP,
  ST_SETE,
  ST_SETNE,
  ST_SETB,
  ST_SETL,
  ST_SETG,
  ST_SETBE,
  ST_SETLE,
  ST_SETGE,
  ST_JMP,
  ST_JE,
  ST_JNE,
  ST_CALL,
  ST_LEAVE,
  ST_RET,
} StmtType;

typedef struct {
  StmtType type;
} Stmt;

// label
typedef struct {
  StmtType type;
  char *ident;
  Token *token;
} Label;

// directive
typedef struct {
  StmtType type; // directive type
  char *ident;      // identifier
  int num;          // number
  String *string;   // string
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

// instructino suffix
typedef enum {
  INST_BYTE,
  INST_WORD,
  INST_LONG,
  INST_QUAD,
} InstSuffix;

// instruction
typedef struct {
  StmtType type;  // instruction type
  InstSuffix suffix; // instruction suffix

  // instruction with one operand
  Op *op;

  // instruction with two operands
  Op *src;
  Op *dest;

  Token *token;
} Inst;

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
extern Vector *as_parse(Vector *tokens);

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
