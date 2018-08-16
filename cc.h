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

int strcmp(char *s1, char *s2);

int isdigit(int c);
int isalpha(int c);
int isalnum(int c);

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
extern void *vector_pop(Vector *vector);

typedef struct map {
  int count;
  char *keys[1024];
  void *values[1024];
} Map;

extern Map *map_new();
extern int map_count(Map *map);
extern bool map_put(Map *map, char *key, void *value);
extern void *map_lookup(Map *map, char *key);
extern void map_clear();

extern noreturn void error(char *format, ...);

extern char *read_source(char *file);

typedef enum token_type {
  tVOID,
  tBOOL,
  tCHAR,
  tINT,
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

typedef struct token {
  TokenType type;
  int int_value;
  String *string_value;
  char *identifier;
} Token;

extern Vector *lexical_analyze(char *source_buffer);

extern Vector *preprocess(Vector *token_vector);

typedef enum type_type {
  VOID,
  BOOL,
  CHAR,
  INT,
  POINTER,
  ARRAY,
  STRUCT,
  FUNCTION
} TypeType;

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

extern Type *type_new();
extern Type *type_void();
extern Type *type_bool();
extern Type *type_char();
extern Type *type_int();
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

typedef enum symbol_type {
  GLOBAL,
  LOCAL
} SymbolType;

typedef struct symbol {
  SymbolType type;
  char *identifier;
  Type *value_type;
  int offset;
  bool declaration;
} Symbol;

typedef enum node_type {
  CONST,
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
  IF_ELSE_STMT,
  WHILE_STMT,
  DO_WHILE_STMT,
  FOR_STMT,
  CONTINUE_STMT,
  BREAK_STMT,
  RETURN_STMT,
  FUNC_DEF,
  TLANS_UNIT
} NodeType;

typedef struct node {
  enum node_type type;
  Type *value_type;
  int int_value;
  String *string_value;
  int string_label;
  char *identifier;
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

extern Node *node_new();
extern Node *parse(Vector *token_vector);

extern void analyze(Node *node);

extern void gen(Node *node);
