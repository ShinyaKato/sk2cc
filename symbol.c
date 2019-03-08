#include "sk2cc.h"

Symbol *symbol_new(SymbolType sy_type, char *identifier, Token *token) {
  Symbol *symbol = calloc(1, sizeof(Symbol));
  symbol->sy_type = sy_type;
  symbol->identifier = identifier;
  symbol->token = token;
  return symbol;
}

Symbol *symbol_variable(char *identifier, Token *token) {
  return symbol_new(SY_VARIABLE, identifier, token);
}

Symbol *symbol_type(char *identifier, Token *token) {
  return symbol_new(SY_TYPE, identifier, token);
}

Symbol *symbol_const(char *identifier, Expr *expr, Token *token) {
  Symbol *symbol = symbol_new(SY_CONST, identifier, token);
  symbol->const_expr = expr;
  return symbol;
}
