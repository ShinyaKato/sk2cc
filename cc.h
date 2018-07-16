#ifndef _CC_H_
#define _CC_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

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

extern void error(char *message);

extern char peek_char();
extern char get_char();

typedef enum token_type {
  tIF,
  tELSE,
  tWHILE,
  tFOR,
  tIDENTIFIER,
  tINT,
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
extern void lex_init();

typedef struct symbol {
  int position;
} Symbol;

typedef enum node_type {
  CONST,
  IDENTIFIER,
  FUNC_CALL,
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
  COMP_STMT,
  EXPR_STMT,
  IF_STMT,
  IF_ELSE_STMT,
  WHILE_STMT,
  FOR_STMT,
  FUNC_DEF,
  TLANS_UNIT
} NodeType;

typedef struct node {
  enum node_type type;
  int int_value;
  char *identifier;
  Symbol *symbol;
  struct node *args[6];
  int args_count;
  struct node *left, *right, *init, *control, *afterthrough, *expression;
  Vector *statements;
  struct node *if_body, *else_body, *loop_body, *function_body;
  int vars_count, params_count;
  Vector *definitions;
} Node;

extern Node *parse();
extern void parse_init();

extern void gen(Node *node);

#endif
