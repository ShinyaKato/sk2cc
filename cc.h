#include "sk2cc.h"

// string, vector, map, binary
#include "string.h"
#include "vector.h"
#include "map.h"
#include "binary.h"

// struct declaration
typedef struct location Location;

typedef struct token Token;

typedef struct node Node;
typedef struct expr Expr;
typedef struct decl Decl;
typedef struct stmt Stmt;
typedef struct func Func;

typedef struct specifier Specifier;
typedef struct declarator Declarator;
typedef struct initializer Initializer;
typedef struct type_name TypeName;

typedef struct trans_unit TransUnit;

typedef struct type Type;
typedef struct member Member;

typedef struct symbol Symbol;

// Location
struct location {
  char *filename; // source file name
  char *line;     // pointer to the line head
  int lineno;     // 1-indexed
  int column;     // 1-indexed
};

// TokenType
// A one-character token is represented by it's ascii code.
typedef enum token_type {
  // white spaces (removed before syntax analysis)
  TK_NEWLINE = 128,
  TK_SPACE,

  // pp-number (converted before syntax analysis)
  TK_PP_NUMBER,

  // keywords for expressions
  TK_SIZEOF,
  TK_ALIGNOF,

  // keywords for declarations
  TK_TYPEDEF,
  TK_EXTERN,
  TK_STATIC,
  TK_VOID,
  TK_CHAR,
  TK_SHORT,
  TK_INT,
  TK_LONG,
  TK_SIGNED,
  TK_UNSIGNED,
  TK_BOOL,
  TK_STRUCT,
  TK_ENUM,
  TK_NORETURN,

  // keywords for statements
  TK_CASE,
  TK_DEFAULT,
  TK_IF,
  TK_ELSE,
  TK_SWITCH,
  TK_WHILE,
  TK_DO,
  TK_FOR,
  TK_GOTO,
  TK_CONTINUE,
  TK_BREAK,
  TK_RETURN,

  // identifiers, constants, string literals
  TK_IDENTIFIER,
  TK_INTEGER_CONST,
  TK_CHAR_CONST,
  TK_STRING_LITERAL,

  // punctuators
  TK_ARROW,      // ->
  TK_INC,        // ++
  TK_DEC,        // --
  TK_LSHIFT,     // <<
  TK_RSHIFT,     // >>
  TK_LTE,        // <=
  TK_GTE,        // >=
  TK_EQ,         // ==
  TK_NEQ,        // !=
  TK_AND,        // &&
  TK_OR,         // ||
  TK_MUL_ASSIGN, // *=
  TK_DIV_ASSIGN, // /=
  TK_MOD_ASSIGN, // %=
  TK_ADD_ASSIGN, // +=
  TK_SUB_ASSIGN, // -=
  TK_ELLIPSIS,   // ...

  // EOF (the end of the input source file)
  TK_EOF,
} TokenType;

// Token
// Token holds original location in the input source code.
// This information is used for error report.
struct token {
  TokenType tk_type;

  // pp-number
  char *pp_number;

  // identifier
  char *identifier;

  // integer-constant
  unsigned long long int_value;
  bool int_decimal;
  bool int_unsigned;
  bool int_long;

  // character-constant
  char char_value;

  // string-literal
  String *string_literal;

  // for token stringification
  char *text;

  // location information
  Location *loc;
};

// NodeType
typedef enum node_type {
  // built-in macros
  ND_VA_START,
  ND_VA_ARG,
  ND_VA_END,

  // expression
  ND_IDENTIFIER,
  ND_INTEGER,
  ND_ENUM_CONST,
  ND_STRING,
  ND_SUBSCRIPTION,
  ND_CALL,
  ND_DOT,
  ND_ARROW,
  ND_POST_INC,
  ND_POST_DEC,
  ND_PRE_INC,
  ND_PRE_DEC,
  ND_ADDRESS,
  ND_INDIRECT,
  ND_UPLUS,
  ND_UMINUS,
  ND_NOT,
  ND_LNOT,
  ND_SIZEOF,
  ND_ALIGNOF,
  ND_CAST,
  ND_MUL,
  ND_DIV,
  ND_MOD,
  ND_ADD,
  ND_SUB,
  ND_LSHIFT,
  ND_RSHIFT,
  ND_LT,
  ND_GT,
  ND_LTE,
  ND_GTE,
  ND_EQ,
  ND_NEQ,
  ND_AND,
  ND_XOR,
  ND_OR,
  ND_LAND,
  ND_LOR,
  ND_CONDITION,
  ND_ASSIGN,
  ND_MUL_ASSIGN,
  ND_DIV_ASSIGN,
  ND_MOD_ASSIGN,
  ND_ADD_ASSIGN,
  ND_SUB_ASSIGN,
  ND_COMMA,

  // declaration
  ND_DECL,

  // statement
  ND_LABEL,
  ND_CASE,
  ND_DEFAULT,
  ND_COMP,
  ND_EXPR,
  ND_IF,
  ND_SWITCH,
  ND_WHILE,
  ND_DO,
  ND_FOR,
  ND_GOTO,
  ND_CONTINUE,
  ND_BREAK,
  ND_RETURN,

  // function definition
  ND_FUNC,
} NodeType;

// RegSize
typedef enum {
  REG_BYTE,
  REG_WORD,
  REG_LONG,
  REG_QUAD,
} RegSize;

// RegCode
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

// Node (AST node)
// This struct is used for checking the node type.
// After checking, the pointer is casted to the pointer of each node type.
// There are 4 kinds of node: Expr, Decl, Stmt, Func.
struct node {
  NodeType nd_type;
};

// Expr (AST node for expression)
struct expr {
  NodeType nd_type;
  Type *type;

  // built-in macros
  Expr *macro_ap;
  char *macro_arg;
  TypeName *macro_type;

  // child expression node
  Expr *expr;      // for unary expr, postfix expr, cast expr
  Expr *lhs, *rhs; // for binary expr, conditional expr
  Expr *cond;      // for conditional expr

  // identifier
  char *identifier;
  Symbol *symbol;

  // integer constant
  unsigned long long int_value;
  bool int_decimal;
  bool int_unsigned;
  bool int_long;

  // string literal
  String *string_literal;
  int string_label;

  // subscription
  Expr *index;

  // call
  Vector *args; // Vector<Expr*>

  // dot, arrow
  char *member;
  int offset;

  // sizeof, alignof, cast
  TypeName *type_name;

  // allocated register
  RegCode reg;

  Token *token;
};

// Decl (AST node for declaration)
struct decl {
  NodeType nd_type;
  Vector *specs;   // Vector<Specifier*>
  Vector *symbols; // Vector<Symbol*>
  Symbol *symbol;
  Token *token;
};

// Stmt (AST node for statement)
struct stmt {
  NodeType nd_type;

  // labeled statement
  char *label_ident;
  Stmt *label_stmt;

  // case statement
  Expr *case_const;
  Stmt *case_stmt;

  // default statement
  Stmt *default_stmt;

  // compound statement (block)
  Vector *block_items; // Vector<Node*> (Decl* or Stmt*)

  // expression statement
  Expr *expr; // optional

  // if-else statement
  Expr *if_cond;
  Stmt *then_body;
  Stmt *else_body; // optional

  // switch statement
  Expr *switch_cond;
  Stmt *switch_body;
  Vector *switch_cases;

  // while statement
  Expr *while_cond;
  Stmt *while_body;

  // do-while statement
  Expr *do_cond;
  Stmt *do_body;

  // for statement
  Node *for_init;  // optional, Decl* or Expr*
  Expr *for_cond;  // optional
  Expr *for_after; // optional
  Stmt *for_body;

  // goto statement
  char *goto_ident;
  Stmt *goto_target;

  // continue statement
  Stmt *continue_target;

  // break statement
  Stmt *break_target;

  // return statement
  Expr *ret_expr; // optional
  Func *ret_func;

  // labels
  int label_no;       // for label, case, default
  int label_continue; // for while, do, for
  int label_break;    // for while, do, for, switch

  Token *token;
};

// Func (AST node for function definition)
struct func {
  NodeType nd_type;
  Vector *specs; // Vector<Specifier*>
  Symbol *symbol;
  Stmt *body;

  int stack_size;      // stack size for local variables
  Vector *label_stmts; // Vector<Stmt*>

  int label_return; // label

  Token *token;
};

// SpecifierType
typedef enum specifier_type {
  // storage-class-specifier
  SP_TYPEDEF,
  SP_EXTERN,
  SP_STATIC,

  // type-specifier
  SP_VOID,
  SP_CHAR,
  SP_SHORT,
  SP_INT,
  SP_LONG,
  SP_SIGNED,
  SP_UNSIGNED,
  SP_BOOL,
  SP_STRUCT,
  SP_ENUM,
  SP_TYPEDEF_NAME,

  // function-specifier
  SP_NORETURN,
} SpecifierType;

// Specifier
struct specifier {
  SpecifierType sp_type;

  // struct
  char *struct_tag;     // optional
  Vector *struct_decls; // optional, Vector<Decl*>

  // enum
  char *enum_tag; // optional
  Vector* enums;  // optional, Vector<Symbol*>

  // typedef-name
  char *typedef_name;
  Symbol *typedef_symbol;

  Token *token;
};

// DeclaratorType
typedef enum declarator_type {
  DECL_POINTER,
  DECL_ARRAY,
  DECL_FUNCTION,
} DeclaratorType;

// Declarator
struct declarator {
  DeclaratorType decl_type;
  Declarator *decl; // optional

  // array
  Expr *size; // optional

  // function
  Vector *params;   // Vector<Decl*>
  bool ellipsis;    // accepts variable length arguments
  Map *proto_scope; // function prototype scope

  Token *token;
};

// TypeName
struct type_name {
  Vector *specs;    // Vector<Specifier*>
  Declarator *decl; // optional
  Token *token;
};

// Initializer (declaration initializer)
struct initializer {
  Type *type;
  Expr *expr;
  Vector *list; // Vector<Initializer*>
  Token *token;
};

// TransUnit
struct trans_unit {
  Vector *literals; // Vector<String*>
  Vector *decls;    // Vector<Node*> (Decl* or Func*)
};

// TypeType
typedef enum type_type {
  TY_VOID,
  TY_BOOL,
  TY_CHAR,
  TY_UCHAR,
  TY_SHORT,
  TY_USHORT,
  TY_INT,
  TY_UINT,
  TY_LONG,
  TY_ULONG,
  TY_POINTER,
  TY_ARRAY,
  TY_FUNCTION,
  TY_STRUCT,
} TypeType;

// Type
struct type {
  TypeType ty_type;
  int size;
  int align;
  bool complete;

  // holds original type when converting array to pointer.
  // the original type is used for sizeof and alignof.
  Type *original;

  // pointer
  Type *pointer_to;

  // array
  Type *array_of;
  int length;

  // function
  Type *returning;
  Vector *params; // Vector<Symbol*>
  bool ellipsis;

  // struct
  Map *members; // Map<Member*>
};

// Member (struct member)
struct member {
  Type *type;
  int offset;
};

// SymbolType
typedef enum symbol_type {
  SY_VARIABLE, // variable
  SY_TYPE,     // typedef-name
  SY_CONST,    // enumeration-constant
} SymbolType;

// SymbolLink
typedef enum symbol_link {
  LN_EXTERNAL, // global variable
  LN_INTERNAL, // static variable
  LN_NONE,     // local variable
} SymbolLink;

// Symbol
struct symbol {
  SymbolType sy_type;
  char *identifier;
  Symbol *prev;

  // variable and typedef-name
  Initializer *init;
  Declarator *decl;
  Type *type;
  SymbolLink link;
  bool definition;
  int offset; // for local variable

  // enumeration-constant
  Expr *const_expr;
  int const_value;

  Token *token;
};

// error.c
#define ERROR(token, ...) \
  error((token)->loc, __FILE__, __LINE__, __VA_ARGS__);

extern noreturn void error(Location *loc, char *__file, int __lineno, char *format, ...);

// lex.c
extern Vector *tokenize(char *input_filename);

// cpp.c
extern Vector *preprocess(Vector *pp_tokens);

// parse.c
extern TransUnit *parse(Vector *tokens);

// sema.c
extern void sema(TransUnit *trans_unit);

// alloc.c
extern void alloc(TransUnit *trans_unit);

// gen.c
extern void gen(TransUnit *node);
