#include "string.h"
#include "vector.h"
#include "map.h"

#define EOF (-1)

#define NULL ((void *) 0)

#define bool _Bool
#define false 0
#define true 1

#define noreturn _Noreturn

#define va_start __builtin_va_start
#define va_end __builtin_va_end
typedef struct {
  int gp_offset;
  int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
} va_list[1];

typedef int size_t;
typedef struct _IO_FILE FILE;

extern struct _IO_FILE *stdin;
extern struct _IO_FILE *stdout;
extern struct _IO_FILE *stderr;

int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);
int printf (char *format, ...);
int fprintf(FILE *stream, char *format, ...);
int vfprintf(FILE *s, char *format, va_list arg);
FILE *fopen(char *filename, char *modes);

void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void exit(int status);
double strtod(char *s, char *endpoint);

int strcmp(char *s1, char *s2);

int isdigit(int c);
int isalpha(int c);
int isalnum(int c);

typedef enum token_type {
  tVOID,
  tBOOL,
  tCHAR,
  tINT,
  tDOUBLE,
  tSTRUCT,
  tENUM,
  tTYPEDEF,
  tEXTERN,
  tNORETURN,
  tSIZEOF,
  tALIGNOF,
  tIF,
  tELSE,
  tWHILE,
  tDO,
  tFOR,
  tCONTINUE,
  tBREAK,
  tRETURN,
  tIDENTIFIER,
  tINT_CONST,
  tFLOAT_CONST,
  tSTRING_LITERAL,
  tLBRACKET,
  tRBRACKET,
  tLPAREN,
  tRPAREN,
  tRBRACE,
  tLBRACE,
  tDOT,
  tARROW,
  tINC,
  tDEC,
  tNOT,
  tLNOT,
  tMUL,
  tDIV,
  tMOD,
  tADD,
  tSUB,
  tLSHIFT,
  tRSHIFT,
  tLT,
  tGT,
  tLTE,
  tGTE,
  tEQ,
  tNEQ,
  tAND,
  tXOR,
  tOR,
  tLAND,
  tLOR,
  tQUESTION,
  tCOLON,
  tSEMICOLON,
  tELLIPSIS,
  tASSIGN,
  tADD_ASSIGN,
  tSUB_ASSIGN,
  tMUL_ASSIGN,
  tCOMMA,
  tHASH,
  tNEWLINE,
  tEND
} TokenType;

typedef enum type_type {
  VOID,
  BOOL,
  CHAR,
  INT,
  DOUBLE,
  POINTER,
  ARRAY,
  STRUCT,
  FUNCTION
} TypeType;

typedef enum node_type {
  INT_CONST,
  FLOAT_CONST,
  STRING_LITERAL,
  IDENTIFIER,
  FUNC_CALL,
  DOT,
  POST_INC,
  POST_DEC,
  PRE_INC,
  PRE_DEC,
  ADDRESS,
  INDIRECT,
  UPLUS,
  UMINUS,
  NOT,
  LNOT,
  SIZEOF,
  CAST,
  MUL,
  DIV,
  MOD,
  ADD,
  SUB,
  LSHIFT,
  RSHIFT,
  LT,
  GT,
  LTE,
  GTE,
  EQ,
  NEQ,
  AND,
  OR,
  XOR,
  LAND,
  LOR,
  CONDITION,
  ASSIGN,
  ADD_ASSIGN,
  SUB_ASSIGN,
  MUL_ASSIGN,
  VAR_INIT,
  VAR_ARRAY_INIT,
  VAR_INIT_DECL,
  VAR_DECL,
  COMP_STMT,
  EXPR_STMT,
  IF_STMT,
  WHILE_STMT,
  DO_WHILE_STMT,
  FOR_STMT,
  CONTINUE_STMT,
  BREAK_STMT,
  RETURN_STMT,
  FUNC_DEF,
  TLANS_UNIT
} NodeType;

typedef enum symbol_type {
  GLOBAL,
  LOCAL
} SymbolType;

typedef struct token {
  TokenType type;
  int int_value;
  double double_value;
  String *string_value;
  char *identifier;
  int lineno, column;
  char *source;
} Token;

typedef struct type {
  TypeType type;
  int size, align;
  struct type *pointer_to;
  struct type *array_of;
  int array_size;
  Map *members, *offsets;
  struct type *function_returning;
  Vector *params;
  bool ellipsis;
  int original_size;
  bool array_pointer;
  bool definition;
  bool external;
  bool incomplete;
} Type;

typedef struct symbol {
  SymbolType type;
  Token *token;
  char *identifier;
  Type *value_type;
  int offset;
  bool declaration;
} Symbol;

typedef struct node {
  enum node_type type;
  Token *token;
  Type *value_type;
  int int_value;
  double double_value;
  String *string_value;
  int string_label, float_label;
  char *identifier, *member;
  Symbol *symbol;
  Vector *args;
  int member_offset;
  struct node *left, *right, *init, *control, *afterthrough, *expr;
  struct node *initializer;
  Vector *array_elements;
  Vector *declarations;
  Vector *statements;
  struct node *if_body, *else_body, *loop_body, *function_body;
  int local_vars_size;
  Vector *string_literals, *definitions;
} Node;

extern noreturn void error(Token *token, char *format, ...);

extern char *scan(char *filename);

extern Vector *tokenize(char *buffer);

extern Vector *preprocess(Vector *pp_tokens);

extern Type *type_new();
extern Type *type_void();
extern Type *type_bool();
extern Type *type_char();
extern Type *type_int();
extern Type *type_double();
extern Type *type_pointer_to(Type *type);
extern Type *type_array_of(Type *type, int array_size);
extern Type *type_struct(Vector *identifiers, Map *members);
extern Type *type_function_returning(Type *returning, Vector *params, bool ellipsis);
extern Type *type_convert(Type *type);
extern void type_copy(Type *dest, Type *src);
extern bool type_integer(Type *type);
extern bool type_pointer(Type *type);
extern bool type_scalar(Type *type);
extern bool type_same(Type *type1, Type *type2);

extern int get_local_vars_size();
extern Symbol *symbol_lookup(char *identifier);
extern void symbol_put(char *identifier, Symbol *symbol);
extern void begin_scope();
extern void end_scope();
extern void begin_function_scope(Symbol *symbol);
extern void begin_global_scope();

extern Node *node_new();
extern Node *node_int_const(int int_value, Token *token);
extern Node *node_float_const(double double_value, Token *token);
extern Node *node_string_literal(String *string_value, int string_label, Token *token);
extern Node *node_identifier(char *identifier, Symbol *symbol, Token *token);
extern Node *node_func_call(Node *expr, Vector *args, Token *token);
extern Node *node_dot(Node *expr, char *identifier, Token *token);
extern Node *node_sizeof(Node *expr, Token *token);
extern Node *node_unary_expr(NodeType type, Node *expr, Token *token);
extern Node *node_cast(Type *value_type, Node *expr, Token *token);
extern Node *node_binary_expr(NodeType type, Node *left, Node *right, Token *token);
extern Node *node_condition(Node *control, Node *left, Node *right, Token *token);
extern Node *node_assign(NodeType type, Node *left, Node *right, Token *token);
extern Node *node_comp_stmt(Vector *statements);
extern Node *node_expr_stmt(Node *expr);
extern Node *node_if_stmt(Node *control, Node *if_body, Node *else_body);
extern Node *node_while_stmt(Node *control, Node *loop_body);
extern Node *node_do_while_stmt(Node *control, Node *loop_body);
extern Node *node_for_stmt(Node *init, Node *control, Node *afterthrough, Node *loop_body);
extern Node *node_continue_stmt(int continue_level, Token *token);
extern Node *node_break_stmt(int break_level, Token *token);
extern Node *node_return_stmt(Node *node);
extern Node *node_func_def(Symbol *symbol, Node *function_body, int local_vars_size, Token *token);
extern Node *node_trans_unit(Vector *string_literals, Vector *definitions);

extern Node *parse(Vector *tokens);

extern void analyze(Node *node);

extern void gen(Node *node);
