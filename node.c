#include "sk2cc.h"

Expr *expr_new(NodeType nd_type, Token *token) {
  Expr *node = calloc(1, sizeof(Expr));
  node->nd_type = nd_type;
  node->token = token;
  return node;
}

Expr *expr_identifier(char *identifier, Symbol *symbol, Token *token) {
  Expr *node = expr_new(ND_IDENTIFIER, token);
  node->identifier = identifier;
  node->symbol = symbol;
  return node;
}

Expr *expr_integer(unsigned long long int_value, bool int_u, bool int_l, bool int_ll, Token *token) {
  Expr *node = expr_new(ND_INTEGER, token);
  node->int_value = int_value;
  node->int_u = int_u;
  node->int_l = int_l;
  node->int_ll = int_ll;
  return node;
}

Expr *expr_enum_const(char *identifier, Symbol *symbol, Token *token) {
  Expr *node = expr_new(ND_ENUM_CONST, token);
  node->identifier = identifier;
  node->symbol = symbol;
  return node;
}

Expr *expr_string(String *string_literal, int string_label, Token *token) {
  Expr *node = expr_new(ND_STRING, token);
  node->string_literal = string_literal;
  node->string_label = string_label;
  return node;
}

Expr *expr_subscription(Expr *expr, Expr *index, Token *token) {
  Expr *node = expr_new(ND_SUBSCRIPTION, token);
  node->expr = expr;
  node->index = index;
  return node;
}

Expr *expr_call(Expr *expr, Vector *args, Token *token) {
  Expr *node = expr_new(ND_CALL, token);
  node->expr = expr;
  node->args = args;
  return node;
}

Expr *expr_dot(Expr *expr, char *member, Token *token) {
  Expr *node = expr_new(ND_DOT, token);
  node->expr = expr;
  node->member = member;
  return node;
}

Expr *expr_arrow(Expr *expr, char *member, Token *token) {
  Expr *node = expr_new(ND_ARROW, token);
  node->expr = expr;
  node->member = member;
  return node;
}

Expr *expr_sizeof(Expr *expr, TypeName *type_name, Token *token) {
  Expr *node = expr_new(ND_SIZEOF, token);
  node->expr = expr;
  node->type_name = type_name;
  return node;
}

Expr *expr_alignof(TypeName *type_name, Token *token) {
  Expr *node = expr_new(ND_ALIGNOF, token);
  node->type_name = type_name;
  return node;
}

Expr *expr_cast(TypeName *type_name, Expr *expr, Token *token) {
  Expr *node = expr_new(ND_CAST, token);
  node->expr = expr;
  node->type_name = type_name;
  return node;
}

Expr *expr_unary(NodeType nd_type, Expr *expr, Token *token) {
  Expr *node = expr_new(nd_type, token);
  node->expr = expr;
  return node;
}

Expr *expr_binary(NodeType nd_type, Expr *lhs, Expr *rhs, Token *token) {
  Expr *node = expr_new(nd_type, token);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Expr *expr_condition(Expr *cond, Expr *lhs, Expr *rhs, Token *token) {
  Expr *node = expr_new(ND_CONDITION, token);
  node->lhs = lhs;
  node->rhs = rhs;
  node->cond = cond;
  return node;
}

Decl *decl_new(Vector *specs, Vector *symbols, Token *token) {
  Decl *decl = calloc(1, sizeof(Decl));
  decl->nd_type = ND_DECL;
  decl->specs = specs;
  decl->symbols = symbols;
  decl->token = token;
  return decl;
}

Decl *decl_struct(Vector *specs, Vector *symbols, Token *token) {
  Decl *decl = calloc(1, sizeof(Decl));
  decl->nd_type = ND_DECL;
  decl->specs = specs;
  decl->symbols = symbols;
  decl->token = token;
  return decl;
}

Decl *decl_param(Vector *specs, Symbol *symbol, Token *token) {
  Decl *decl = calloc(1, sizeof(Decl));
  decl->nd_type = ND_DECL;
  decl->specs = specs;
  decl->symbol = symbol;
  decl->token = token;
  return decl;
}

Specifier *specifier_new(SpecifierType sp_type, Token *token) {
  Specifier *spec = calloc(1, sizeof(Specifier));
  spec->sp_type = sp_type;
  spec->token = token;
  return spec;
}

Specifier *specifier_struct(char *tag, Vector *decls, Token *token) {
  Specifier *spec = specifier_new(SP_STRUCT, token);
  spec->struct_tag = tag;
  spec->struct_decls = decls;
  return spec;
}

Specifier *specifier_enum(char *tag, Vector *enums, Token *token) {
  Specifier *spec = specifier_new(SP_ENUM, token);
  spec->enum_tag = tag;
  spec->enums = enums;
  return spec;
}

Specifier *specifier_typedef_name(char *identifier, Symbol *symbol, Token *token) {
  Specifier *spec = specifier_new(SP_TYPEDEF_NAME, token);
  spec->typedef_name = identifier;
  spec->typedef_symbol = symbol;
  return spec;
}

Declarator *declarator_new(DeclaratorType decl_type, Declarator *decl, Token *token) {
  Declarator *declarator = calloc(1, sizeof(Declarator));
  declarator->decl_type = decl_type;
  declarator->decl = decl;
  declarator->token = token;
  return declarator;
}

TypeName *type_name_new(Vector *specs, Declarator *decl, Token *token) {
  TypeName *type_name = calloc(1, sizeof(TypeName));
  type_name->specs = specs;
  type_name->decl = decl;
  type_name->token = token;
  return type_name;
}

Initializer *initializer_expr(Expr *expr, Token *token) {
  Initializer *init = calloc(1, sizeof(Initializer));
  init->expr = expr;
  init->token = token;
  return init;
}

Initializer *initializer_list(Vector *list, Token *token) {
  Initializer *init = calloc(1, sizeof(Initializer));
  init->list = list;
  init->token = token;
  return init;
}

Stmt *stmt_new(NodeType nd_type, Token *token) {
  Stmt *node = calloc(1, sizeof(Stmt));
  node->nd_type = nd_type;
  node->token = token;
  return node;
}

Func *func_new(Vector *specs, Symbol *symbol, Stmt *body, Token *token) {
  Func *func = calloc(1, sizeof(Func));
  func->nd_type = ND_FUNC;
  func->specs = specs;
  func->symbol = symbol;
  func->body = body;
  func->token = token;
  return func;
}

TransUnit *trans_unit_new(Vector *literals, Vector *decls) {
  TransUnit *unit = calloc(1, sizeof(TransUnit));
  unit->literals = literals;
  unit->decls = decls;
  return unit;
}
