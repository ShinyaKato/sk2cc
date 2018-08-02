#ifndef _CC_H_
#define _CC_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdnoreturn.h>

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

extern char peek_char();
extern char get_char();

typedef enum token_type {
  tINT,
  tCHAR,
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
  tLBRACKET,
  tRBRACKET,
  tLPAREN,
  tRPAREN,
  tRBRACE,
  tLBRACE,
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
  tASSIGN,
  tCOMMA,
  tEND
} TokenType;

typedef struct token {
  TokenType type;
  int int_value;
  char *identifier;
} Token;

extern Token *peek_token();
extern Token *get_token();
extern Token *expect_token(TokenType type);
extern bool read_token(TokenType type);
extern void lex_init();

typedef enum type_type {
  CHAR,
  INT,
  POINTER,
  ARRAY
} TypeType;

typedef struct type {
  TypeType type;
  struct type *pointer_to, *array_of;
  int array_size, size;
} Type;

extern Type *type_new();
extern Type *type_char();
extern Type *type_int();
extern Type *type_pointer_to(Type *type);
extern Type *type_array_of(Type *type, int array_size);
extern Type *type_convert(Type *type);
extern bool type_integer(Type *type);

typedef enum symbol_type {
  GLOBAL,
  LOCAL
} SymbolType;

typedef struct symbol {
  SymbolType type;
  char *identifier;
  Type *value_type;
  int offset;
} Symbol;

typedef enum node_type {
  CONST,
  IDENTIFIER,
  FUNC_CALL,
  ADDRESS,
  INDIRECT,
  UPLUS,
  UMINUS,
  NOT,
  LNOT,
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
  char *identifier;
  Symbol *symbol;
  Vector *args;
  struct node *left, *right, *init, *control, *afterthrough, *expr;
  Vector *var_symbols;
  Vector *statements;
  struct node *if_body, *else_body, *loop_body, *function_body;
  Vector *param_symbols;
  int local_vars_size;
  Vector *definitions;
} Node;

extern Node *parse();

extern void analyze(Node *node);

extern void gen(Node *node);

#endif
