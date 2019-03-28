// The preprocessor is not sufficiently supported
// enough to read the header file of the system.

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;

typedef unsigned long long size_t;

// stdbool.h
#define bool _Bool
#define false 0
#define true 1

// stdnoreturn.h
#define noreturn _Noreturn

// stdarg.h
#define va_start __builtin_va_start
#define va_arg __builtin_va_arg
#define va_end __builtin_va_end

typedef __builtin_va_list va_list;

// stdint.h
typedef long int intptr_t;

// stdio.h
#define EOF (-1)
#define NULL ((void *) 0)

typedef struct _IO_FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int printf(char *format, ...);
int fprintf(FILE *stream, char *format, ...);
int vfprintf(FILE *s, char *format, va_list arg);

FILE *fopen(char *filename, char *modes);
size_t fread(void *ptr, size_t size, size_t n, FILE *stream);
size_t fwrite(void *ptr, size_t size, size_t n, FILE *stream);
int fclose(FILE *stream);

int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);

void perror(char *s);

// stdlib.h
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void exit(int status);

// strcmp.h
int strcmp(char *s1, char *s2);

// ctype.h
int isprint(int c);
int isalpha(int c);
int isalnum(int c);
int isdigit(int c);
int isspace(int c);
int isxdigit(int c);
int tolower(int c);

// elf.h
#define ELF64_R_INFO(sym, type) ((((Elf64_Xword) (sym)) << 32) + (type))

#define R_X86_64_64 1
#define R_X86_64_PC32 2

#define ELF64_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))

#define STB_LOCAL 0
#define STB_GLOBAL 1

#define STV_DEFAULT 0

#define STT_NOTYPE 0
#define STT_SECTION 3

#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4

#define SHF_ALLOC (1 << 1)
#define SHF_EXECINSTR (1 << 2)
#define SHF_INFO_LINK (1 << 6)

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_REL 1
#define ELFOSABI_NONE 0
#define EM_X86_64 62

typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Section;

typedef struct {
  unsigned char e_ident[16];
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;
  Elf64_Off e_phoff;
  Elf64_Off e_shoff;
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
} Elf64_Ehdr;

typedef struct {
  Elf64_Word sh_name;
  Elf64_Word sh_type;
  Elf64_Xword sh_flags;
  Elf64_Addr sh_addr;
  Elf64_Off sh_offset;
  Elf64_Xword sh_size;
  Elf64_Word sh_link;
  Elf64_Word sh_info;
  Elf64_Xword sh_addralign;
  Elf64_Xword sh_entsize;
} Elf64_Shdr;

typedef struct {
  Elf64_Word st_name;
  unsigned char st_info;
  unsigned char st_other;
  Elf64_Section st_shndx;
  Elf64_Addr st_value;
  Elf64_Xword st_size;
} Elf64_Sym;

typedef struct {
  Elf64_Addr r_offset;
  Elf64_Xword r_info;
  Elf64_Sxword r_addend;
} Elf64_Rela;

// string, vector, map, binary
#include "string.h"
#include "vector.h"
#include "map.h"
#include "binary.h"

// struct and enum declaration

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
  DIR_TEXT,
  DIR_DATA,
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
  Map *symbols;
} TransUnit;

// as_error.c
#define ERROR(token, ...) \
  do { \
    Token *t = (token); \
    as_error(t->file, t->lineno, t->column, t->line, __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)

extern noreturn void as_error(char *file, int lineno, int column, char *line, char *__file, int __lineno, char *format, ...);

// as_lex.c
extern Vector *as_tokenize(char *file);

// as_parse.c
extern Vector *as_parse(Vector *lines);

// as_encode.c
extern TransUnit *as_encode(Vector *stmts);

// as_gen.c
#define SHNUM 8
#define UNDEF 0
#define TEXT 1
#define RELA_TEXT 2
#define DATA 3
#define RELA_DATA 4
#define SYMTAB 5
#define STRTAB 6
#define SHSTRTAB 7

extern void gen_obj(TransUnit *trans_unit, char *output);
