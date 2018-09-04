#include "cc.h"

Vector *scopes;
int local_vars_size;

int get_local_vars_size() {
  return local_vars_size;
}

Symbol *symbol_lookup(char *identifier) {
  for (int i = scopes->length - 1; i >= 0; i--) {
    Map *map = scopes->array[i];
    Symbol *symbol = map_lookup(map, identifier);
    if (symbol) return symbol;
  }
  return NULL;
}

void symbol_put(char *identifier, Symbol *symbol) {
  Map *map = scopes->array[scopes->length - 1];
  Symbol *previous = map_lookup(map, identifier);
  if (previous && previous->defined) {
    error(symbol->token, "duplicated function or variable definition of '%s'.", identifier);
  }

  if (scopes->length == 1) {
    symbol->type = GLOBAL;
  } else {
    int size = symbol->value_type->size;
    if (size % 8 != 0) size = size / 8 * 8 + 8;
    local_vars_size += size;

    symbol->type = LOCAL;
    symbol->offset = local_vars_size;
  }

  map_put(map, identifier, symbol);
}

void begin_scope() {
  vector_push(scopes, map_new());
}

void end_scope() {
  vector_pop(scopes);
}

void begin_function_scope(Symbol *symbol) {
  symbol_put(symbol->identifier, symbol);

  local_vars_size = symbol->value_type->ellipsis ? 176 : 0;
  begin_scope();

  Vector *params = symbol->value_type->params;
  for (int i = 0; i < params->length; i++) {
    Symbol *param = params->array[i];
    symbol_put(param->identifier, param);
  }
}

void begin_global_scope() {
  scopes = vector_new();
  begin_scope();
}
