#include "sk2cc.h"

Vector *literals;      // Vector<String*>
Vector *symbol_scopes; // Vector<Map<SymbolType*>*>

// symbol table and scopes

void put_symbol(char *identifier, Symbol *symbol) {
  Map *map = symbol_scopes->buffer[symbol_scopes->length - 1];

  symbol->prev = map_lookup(map, identifier);
  if (symbol->prev && symbol->prev->sy_type != symbol->sy_type) {
    error(symbol->token, "invalid redeclaration: %s.", identifier);
  }

  map_put(map, identifier, symbol);
}

Symbol *lookup_symbol(char *identifier) {
  for (int i = symbol_scopes->length - 1; i >= 0; i--) {
    Map *map = symbol_scopes->buffer[i];
    Symbol *symbol = map_lookup(map, identifier);
    if (symbol) return symbol;
  }

  return NULL;
}

// check declaration specifiers

bool check_storage_class_specifier() {
  if (check_token(TK_TYPEDEF)) return true;
  if (check_token(TK_EXTERN)) return true;

  return false;
}

bool check_typedef_name() {
  if (check_token(TK_IDENTIFIER)) {
    Symbol *symbol = lookup_symbol(peek_token()->identifier);
    return symbol && symbol->sy_type == SY_TYPE;
  }

  return false;
}

bool check_type_specifier() {
  if (check_token(TK_VOID)) return true;
  if (check_token(TK_CHAR)) return true;
  if (check_token(TK_SHORT)) return true;
  if (check_token(TK_INT)) return true;
  if (check_token(TK_LONG)) return true;
  if (check_token(TK_SIGNED)) return true;
  if (check_token(TK_UNSIGNED)) return true;
  if (check_token(TK_BOOL)) return true;
  if (check_token(TK_STRUCT)) return true;
  if (check_token(TK_ENUM)) return true;
  if (check_typedef_name()) return true;

  return false;
}

bool check_function_specifier() {
  if (check_token(TK_NORETURN)) return true;

  return false;
}

bool check_declaration_specifier() {
  if (check_storage_class_specifier()) return true;
  if (check_type_specifier()) return true;
  if (check_function_specifier()) return true;

  return false;
}

// parse expression

Expr *cast_expression();
Expr *assignment_expression();
Expr *expression();
TypeName *type_name();

// primary-expression :
//   identifier
//   integer-constant
//   string-literal
//   '(' expression ')'
Expr *primary_expression() {
  Token *token = peek_token();

  if (!check_typedef_name() && read_token(TK_IDENTIFIER)) {
    Symbol *symbol = lookup_symbol(token->identifier);
    if (symbol && symbol->sy_type == SY_CONST) {
      return expr_enum_const(token->identifier, symbol, token);
    } else {
      return expr_identifier(token->identifier, symbol, token);
    }
  }

  if (read_token(TK_INTEGER_CONST)) {
    return expr_integer(token->int_value, token->int_u, token->int_l, token->int_ll, token);
  }

  if (read_token(TK_CHAR_CONST)) {
    return expr_integer(token->char_value, false, false, false, token);
  }

  if (read_token(TK_STRING_LITERAL)) {
    int string_label = literals->length;
    vector_push(literals, token->string_literal);
    return expr_string(token->string_literal, string_label, token);
  }

  if (read_token('(')) {
    Expr *expr = expression();
    expect_token(')');
    return expr;
  }

  error(token, "invalid primary expression.");
}

// postfix-expression :
//   primary-expression
//   postfix-expression '[' expression ']'
//   postfix-expression '(' (assignment-expression (',' assignment-expression)*)? ')'
//   postfix-expression '.' identifier
//   postfix-expression '->' identifier
//   postfix-expression '++'
//   postfix-expression '--'
Expr *postfix_expression(Expr *expr) {
  if (!expr) {
    expr = primary_expression();
  }

  while (1) {
    Token *token = peek_token();
    if (read_token('[')) {
      Expr *index = expression();
      expect_token(']');
      expr = expr_subscription(expr, index, token);
    } else if (read_token('(')) {
      Vector *args = vector_new();
      if (!check_token(')')) {
        do {
          vector_push(args, assignment_expression());
        } while (read_token(','));
      }
      expect_token(')');
      expr = expr_call(expr, args, token);
    } else if (read_token('.')) {
      char *member = expect_token(TK_IDENTIFIER)->identifier;
      expr = expr_dot(expr, member, token);
    } else if (read_token(TK_ARROW)) {
      char *member = expect_token(TK_IDENTIFIER)->identifier;
      expr = expr_arrow(expr, member, token);
    } else if (read_token(TK_INC)) {
      expr = expr_unary(ND_POST_INC, expr, token);
    } else if (read_token(TK_DEC)) {
      expr = expr_unary(ND_POST_DEC, expr, token);
    } else {
      break;
    }
  }

  return expr;
}

// unary-expression :
//   postfix-expression
//   '++' unary-expression
//   '--' unary-expression
//   unary-operator cast-expression
//   cast-expression
//   'sizeof' unary-expression
//   'sizeof' '(' type-name ')'
//   '_Alignof' '(' type-name ')'
// unary-operator :
//   '&' | '*' | '+' | '-' | '~' | '!'
Expr *unary_expression() {
  Token *token = peek_token();

  if (read_token(TK_INC))
    return expr_unary(ND_PRE_INC, unary_expression(), token);
  if (read_token(TK_DEC))
    return expr_unary(ND_PRE_DEC, unary_expression(), token);

  if (read_token('&'))
    return expr_unary(ND_ADDRESS, unary_expression(), token);
  if (read_token('*'))
    return expr_unary(ND_INDIRECT, unary_expression(), token);
  if (read_token('+'))
    return expr_unary(ND_UPLUS, unary_expression(), token);
  if (read_token('-'))
    return expr_unary(ND_UMINUS, unary_expression(), token);
  if (read_token('~'))
    return expr_unary(ND_NOT, unary_expression(), token);
  if (read_token('!'))
    return expr_unary(ND_LNOT, unary_expression(), token);

  if (read_token(TK_SIZEOF)) {
    Expr *expr = NULL;
    TypeName *type = NULL;

    // If '(' follows 'sizeof', it is 'sizeof' type-name
    // or 'sizeof' '(' expression ')'.
    // Otherwise, it is 'sizeof' unary-expression.
    if (read_token('(')) {
      if (check_type_specifier()) {
        type = type_name();
      } else {
        expr = expression();
      }
      expect_token(')');
    } else {
      expr = unary_expression();
    }

    return expr_sizeof(expr, type, token);
  }

  if (read_token(TK_ALIGNOF)) {
    expect_token('(');
    TypeName *type = type_name();
    expect_token(')');
    return expr_alignof(type, token);
  }

  return postfix_expression(NULL);
}

// cast-expression :
//   unary-expression
//   '(' type-name ')' cast-expression
Expr *cast_expression() {
  Token *token = peek_token();

  // If type-specifier follows '(', it is cast-expression.
  // Otherwise, it is postfix-expression like '(' expression ')' '++'.
  if (read_token('(')) {
    if (check_type_specifier()) {
      TypeName *type = type_name();
      expect_token(')');
      return expr_cast(type, cast_expression(), token);
    }

    Expr *expr = expression();
    expect_token(')');
    return postfix_expression(expr);
  }

  return unary_expression();
}

// multiplicative-expression :
//   cast-expression (('*' | '/' | '%') cast-expression)*
Expr *multiplicative_expression(Expr *expr) {
  if (!expr) {
    expr = cast_expression();
  }

  while (1) {
    Token *token = peek_token();
    if (read_token('*')) {
      expr = expr_binary(ND_MUL, expr, cast_expression(), token);
    } else if (read_token('/')) {
      expr = expr_binary(ND_DIV, expr, cast_expression(), token);
    } else if (read_token('%')) {
      expr = expr_binary(ND_MOD, expr, cast_expression(), token);
    } else {
      break;
    }
  }

  return expr;
}

// additive-expression :
//   multiplicative-expression (('+' | '-') multiplicative-expression)*
Expr *additive_expression(Expr *expr) {
  expr = multiplicative_expression(expr);

  while (1) {
    Token *token = peek_token();
    if (read_token('+')) {
      expr = expr_binary(ND_ADD, expr, multiplicative_expression(NULL), token);
    } else if (read_token('-')) {
      expr = expr_binary(ND_SUB, expr, multiplicative_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// shift-expression :
//   additive-expression (('<<' | '>>') additive-expression)*
Expr *shift_expression(Expr *expr) {
  expr = additive_expression(expr);

  while (1) {
    Token *token = peek_token();
    if (read_token(TK_LSHIFT)) {
      expr = expr_binary(ND_LSHIFT, expr, additive_expression(NULL), token);
    } else if (read_token(TK_RSHIFT)) {
      expr = expr_binary(ND_RSHIFT, expr, additive_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// relational-expression :
//   shift-expression (('<' | '>' | '<=' | '>=') shift-expression)*
Expr *relational_expression(Expr *expr) {
  expr = shift_expression(expr);

  while (1) {
    Token *token = peek_token();
    if (read_token('<')) {
      expr = expr_binary(ND_LT, expr, shift_expression(NULL), token);
    } else if (read_token('>')) {
      expr = expr_binary(ND_GT, expr, shift_expression(NULL), token);
    } else if (read_token(TK_LTE)) {
      expr = expr_binary(ND_LTE, expr, shift_expression(NULL), token);
    } else if (read_token(TK_GTE)) {
      expr = expr_binary(ND_GTE, expr, shift_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// equality-expression :
//   relational-expression (('=' | '!=') equality-expression)*
Expr *equality_expression(Expr *expr) {
  expr = relational_expression(expr);

  while (1) {
    Token *token = peek_token();
    if (read_token(TK_EQ)) {
      expr = expr_binary(ND_EQ, expr, relational_expression(NULL), token);
    } else if (read_token(TK_NEQ)) {
      expr = expr_binary(ND_NEQ, expr, relational_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// and-expression :
//   equality-expression ('&' equality-expression)*
Expr *and_expression(Expr *expr) {
  expr = equality_expression(expr);

  while (1) {
    Token *token = peek_token();
    if (read_token('&')) {
      expr = expr_binary(ND_AND, expr, equality_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// xor-expression :
//   and-expression ('^' and-expression)*
Expr *xor_expression(Expr *expr) {
  expr = and_expression(expr);

  while (1) {
    Token *token = peek_token();
    if (read_token('^')) {
      expr = expr_binary(ND_XOR, expr, and_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// or-expression :
//   xor-expression ('^' xor-expression)*
Expr *or_expression(Expr *expr) {
  expr = xor_expression(expr);

  while (1) {
    Token *token = peek_token();
    if (read_token('|')) {
      expr = expr_binary(ND_OR, expr, xor_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// logical-and-expression :
//   or-expression ('&&' or-expression)*
Expr *logical_and_expression(Expr *expr) {
  expr = or_expression(expr);

  while (1) {
    Token *token = peek_token();
    if (read_token(TK_AND)) {
      expr = expr_binary(ND_LAND, expr, or_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// logical-or-expression :
//   logical-and-expression ('||' logical-and-expression)*
Expr *logical_or_expression(Expr *expr) {
  expr = logical_and_expression(expr);

  while (1) {
    Token *token = peek_token();
    if (read_token(TK_OR)) {
      expr = expr_binary(ND_LOR, expr, logical_and_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// conditional-expression :
//   logical-or-expression
//   logical-or-expression '?' expression ':' conditional-expression
Expr *conditional_expression(Expr *expr) {
  expr = logical_or_expression(expr);

  Token *token = peek_token();
  if (read_token('?')) {
    Expr *lhs = expression();
    expect_token(':');
    Expr *rhs = conditional_expression(NULL);

    return expr_condition(expr, lhs, rhs, token);
  }

  return expr;
}

// assignment-expression :
//   conditional-expression
//   unary-expression assignment-operator assignment-expression
// assignment-operator :
//   '=' | '*=' | '/=' | '%=' | '+=' | '-='
Expr *assignment_expression() {
  Expr *expr = cast_expression();

  Token *token = peek_token();
  if (read_token('='))
    return expr_binary(ND_ASSIGN, expr, assignment_expression(), token);
  if (read_token(TK_MUL_ASSIGN))
    return expr_binary(ND_MUL_ASSIGN, expr, assignment_expression(), token);
  if (read_token(TK_DIV_ASSIGN))
    return expr_binary(ND_DIV_ASSIGN, expr, assignment_expression(), token);
  if (read_token(TK_MOD_ASSIGN))
    return expr_binary(ND_MOD_ASSIGN, expr, assignment_expression(), token);
  if (read_token(TK_ADD_ASSIGN))
    return expr_binary(ND_ADD_ASSIGN, expr, assignment_expression(), token);
  if (read_token(TK_SUB_ASSIGN))
    return expr_binary(ND_SUB_ASSIGN, expr, assignment_expression(), token);

  return conditional_expression(expr);
}

// expression :
//   assignment-expression (',' assignment-expression)*
Expr *expression() {
  Expr *expr = assignment_expression();

  while (1) {
    Token *token = peek_token();
    if (read_token(',')) {
      expr = expr_binary(ND_COMMA, expr, assignment_expression(), token);
    } else {
      break;
    }
  }

  return expr;
}

// parse declaration

bool check_typedef(Vector *specs) {
  bool sp_typedef = false;
  for (int i = 0; i < specs->length; i++) {
    Specifier *spec = specs->buffer[i];
    if (spec->sp_type == SP_TYPEDEF) {
      if (sp_typedef) {
        error(spec->token, "duplicated typedef.");
      }
      sp_typedef = true;
    }
  }

  return sp_typedef;
}

Vector *declaration_specifiers();
Vector *specifier_qualifier_list();
Specifier *storage_class_specifier();
Specifier *type_specifier();
Specifier *struct_or_union_specifier();
Decl *struct_declaration();
Specifier *enum_specifier();
Symbol *enumerator();
Specifier *typedef_name();
Specifier *function_specifier();
Symbol *init_declarator(bool sp_typedef);
Symbol *declarator(bool sp_typedef, Declarator *decl);
Symbol *direct_declarator(bool sp_typedef, Declarator *decl);
Decl *parameter_declaration();
TypeName *type_name();
Declarator *abstract_declarator(Declarator *decl);
Initializer *initializer();

// declaration :
//   declaration-specifiers (init-declarator)* ';'
Decl *declaration() {
  Token *token = peek_token();

  Vector *specs = declaration_specifiers();
  bool sp_typedef = check_typedef(specs);

  Vector *symbols = vector_new();
  if (!check_token(';')) {
    do {
      Symbol *symbol = init_declarator(sp_typedef);
      vector_push(symbols, symbol);
      put_symbol(symbol->identifier, symbol);
    } while (read_token(','));
  }
  expect_token(';');

  return decl_new(specs, symbols, token);
}

// declaration-specifiers :
//   (storage-class-specifier | type-specifier | function-specifier)*
Vector *declaration_specifiers() {
  Vector *specs = vector_new();
  while (1) {
    if (check_storage_class_specifier()) {
      vector_push(specs, storage_class_specifier());
    } else if (check_type_specifier()) {
      vector_push(specs, type_specifier());
    } else if (check_function_specifier()) {
      vector_push(specs, function_specifier());
    } else {
      break;
    }
  }

  return specs;
}

// specifier-qualifier-list :
//   (type-specifier | type-qualifier)*
Vector *specifier_qualifier_list() {
  Vector *specs = vector_new();
  while (1) {
    if (check_storage_class_specifier()) {
      vector_push(specs, storage_class_specifier());
    } else if (check_type_specifier()) {
      vector_push(specs, type_specifier());
    } else {
      break;
    }
  }

  return specs;
}

// storage-class-specifier :
//   'typedef'
//   'extern'
Specifier *storage_class_specifier() {
  Token *token = peek_token();

  if (read_token(TK_TYPEDEF))
    return specifier_new(SP_TYPEDEF, token);
  if (read_token(TK_EXTERN))
    return specifier_new(SP_EXTERN, token);

  error(token, "invalid storage-class-specifier.");
}

// type-specifier :
//   'void'
//   'char'
//   'short'
//   'int'
//   'unsigned'
//   '_Bool'
//   struct-or-union-specifier
//   enum-specifier
//   typedef-name
Specifier *type_specifier() {
  Token *token = peek_token();

  if (read_token(TK_VOID))
    return specifier_new(SP_VOID, token);
  if (read_token(TK_CHAR))
    return specifier_new(SP_CHAR, token);
  if (read_token(TK_SHORT))
    return specifier_new(SP_SHORT, token);
  if (read_token(TK_INT))
    return specifier_new(SP_INT, token);
  if (read_token(TK_LONG))
    return specifier_new(SP_LONG, token);
  if (read_token(TK_SIGNED))
    return specifier_new(SP_SIGNED, token);
  if (read_token(TK_UNSIGNED))
    return specifier_new(SP_UNSIGNED, token);
  if (read_token(TK_BOOL))
    return specifier_new(SP_BOOL, token);

  if (check_token(TK_STRUCT))
    return struct_or_union_specifier();
  if (check_token(TK_ENUM))
    return enum_specifier();

  if (check_typedef_name())
    return typedef_name();

  error(token, "invalid type-specifier.");
}

// struct-or-union-specifier :
//   'struct' identifier? '{' struct-declaration (',' struct-declaration)* '}'
//   'struct' identifier
Specifier *struct_or_union_specifier() {
  Token *token = expect_token(TK_STRUCT);

  if (check_token(TK_IDENTIFIER)) {
    char *tag = expect_token(TK_IDENTIFIER)->identifier;

    if (read_token('{')) {
      Vector *decls = vector_new();
      do {
        vector_push(decls, struct_declaration());
      } while (!check_token('}') && !check_token(TK_EOF));
      expect_token('}');

      return specifier_struct(tag, decls, token);
    }

    return specifier_struct(tag, NULL, token);
  }

  Vector *decls = vector_new();
  expect_token('{');
  do {
    vector_push(decls, struct_declaration());
  } while (!check_token('}') && !check_token(TK_EOF));
  expect_token('}');

  return specifier_struct(NULL, decls, token);
}

// struct-declaration :
//   specifier-qualifier-list (declarator (',' declarator)*)? ';'
Decl *struct_declaration() {
  Token *token = peek_token();

  Vector *specs = specifier_qualifier_list();
  Vector *symbols = vector_new();
  if (!check_token(';')) {
    do {
      vector_push(symbols, declarator(false, NULL));
    } while (read_token(','));
  }
  expect_token(';');

  return decl_struct(specs, symbols, token);
}

// enum-specifier :
//   'enum' identifier? '{' (enumerator (',' enumerator)*) '}'
//   'enum' identifier
Specifier *enum_specifier() {
  Token *token = expect_token(TK_ENUM);

  if (check_token(TK_IDENTIFIER)) {
    char *tag = expect_token(TK_IDENTIFIER)->identifier;

    if (read_token('{')) {
      Vector *enums = vector_new();
      do {
        Symbol *symbol = enumerator();
        vector_push(enums, symbol);
        put_symbol(symbol->identifier, symbol);
      } while (read_token(','));
      expect_token('}');

      return specifier_enum(tag, enums, token);
    }

    return specifier_enum(tag, NULL, token);
  }

  Vector *enums = vector_new();
  expect_token('{');
  do {
    Symbol *symbol = enumerator();
    vector_push(enums, symbol);
    put_symbol(symbol->identifier, symbol);
  } while (read_token(','));
  expect_token('}');

  return specifier_enum(NULL, enums, token);
}

// enumerator :
//   enumeration-constant
//   enumeration-constant '=' conditional-expression
Symbol *enumerator() {
  Token *token = expect_token(TK_IDENTIFIER);
  Expr *const_expr = read_token('=') ? conditional_expression(NULL) : NULL;

  return symbol_const(token->identifier, const_expr, token);
}

// typedef-name :
//   identifier
Specifier *typedef_name() {
  Token *token = expect_token(TK_IDENTIFIER);
  Symbol *symbol = lookup_symbol(token->identifier);

  return specifier_typedef_name(token->identifier, symbol, token);
}

// function-specifier :
//   '_Noreturn'
Specifier *function_specifier() {
  Token *token = peek_token();

  if (read_token(TK_NORETURN))
    return specifier_new(SP_NORETURN, token);

  error(token, "invalid function-specifier.");
}

// init-declarator :
//   declarator
//   declarator '=' initializer
Symbol *init_declarator(bool sp_typedef) {
  Symbol *symbol = declarator(sp_typedef, NULL);
  symbol->init = read_token('=') ? initializer() : NULL;
  return symbol;
}

// declarator :
//   '*'* direct-declarator
Symbol *declarator(bool sp_typedef, Declarator *decl) {
  Token *token = peek_token();

  if (read_token('*')) {
    return declarator(sp_typedef, declarator_new(DECL_POINTER, decl, token));
  }

  return direct_declarator(sp_typedef, decl);
}

// direct-declarator :
//   identifier
//   direct-declarator '[' assignment-expression? ']'
//   direct-declarator '(' (parameter-declaration (',' parameter-declaration)* (',' ...)?)? ')'
Symbol *direct_declarator(bool sp_typedef, Declarator *decl) {
  Token *token = expect_token(TK_IDENTIFIER);

  Symbol *symbol;
  if (sp_typedef) {
    symbol = symbol_type(token->identifier, token);
  } else {
    symbol = symbol_variable(token->identifier, token);
  }

  Declarator **decl_ptr = &symbol->decl;
  while (1) {
    Token *token = peek_token();

    Declarator *decl;
    if (read_token('[')) {
      Expr *size = !check_token(']') ? assignment_expression() : NULL;
      expect_token(']');

      decl = declarator_new(DECL_ARRAY, NULL, token);
      decl->size = size;
    } else if (read_token('(')) {
      Map *proto_scope = map_new(); // begin prototype scope
      vector_push(symbol_scopes, proto_scope);

      Vector *params = vector_new();
      bool ellipsis = false;
      if (!check_token(')')) {
        vector_push(params, parameter_declaration());
        while (read_token(',')) {
          if (read_token(TK_ELLIPSIS)) {
            ellipsis = true;
            break;
          }
          vector_push(params, parameter_declaration());
        }
      }
      expect_token(')');

      vector_pop(symbol_scopes); // end prototype scope

      decl = declarator_new(DECL_FUNCTION, NULL, token);
      decl->params = params;
      decl->ellipsis = ellipsis;
      decl->proto_scope = proto_scope;
    } else {
      break;
    }

    *decl_ptr = decl;
    decl_ptr = &decl->decl;
  }

  *decl_ptr = decl;

  return symbol;
}

// parameter-declaration :
//   specifier-qualifier-list declarator
Decl *parameter_declaration() {
  Token *token = peek_token();

  Vector *specs = specifier_qualifier_list();
  Symbol *symbol = declarator(false, NULL);

  put_symbol(symbol->identifier, symbol);

  return decl_param(specs, symbol, token);
}

// type-name :
//   specifier-qualifier-list abstract-declarator?
TypeName *type_name() {
  Token *token = peek_token();

  Vector *specs = specifier_qualifier_list();
  Declarator *decl = check_token('*') ? abstract_declarator(NULL) : NULL;

  return type_name_new(specs, decl, token);
}

// abstract-declarator :
//   '*' '*'*
Declarator *abstract_declarator(Declarator *decl) {
  Token *token = expect_token('*');

  if (check_token('*')) {
    return abstract_declarator(declarator_new(DECL_POINTER, decl, token));
  }

  return declarator_new(DECL_POINTER, decl, token);
}

// initializer :
//   assignment-expression
//   '{' initializer (',' initializer)* '}'
Initializer *initializer() {
  Token *token = peek_token();

  if (read_token('{')) {
    Vector *list = vector_new();
    if (!check_token('}')) {
      do {
        vector_push(list, initializer());
      } while (read_token(','));
    }
    expect_token('}');

    return initializer_list(list, token);
  }

  return initializer_expr(assignment_expression(), token);
}

// parse statement

Stmt *statement();

// compound-statement :
//   '{' (declaration | statement)* '}'
Stmt *compound_statement() {
  Token *token = expect_token('{');
  Vector *block_items = vector_new();
  while (!check_token('}') && !check_token(TK_EOF)) {
    if (check_declaration_specifier()) {
      vector_push(block_items, declaration());
    } else {
      vector_push(block_items, statement());
    }
  }
  expect_token('}');

  Stmt *stmt = stmt_new(ND_COMP, token);
  stmt->block_items = block_items;
  return stmt;
}

// expression-statement :
//   expression? ';'
Stmt *expression_statement() {
  Token *token = peek_token();

  Expr *expr = !check_token(';') ? expression() : NULL;
  expect_token(';');

  Stmt *stmt = stmt_new(ND_EXPR, token);
  stmt->expr = expr;
  return stmt;
}

// if-statement :
//   'if' '(' expression ')' statement
//   'if' '(' expression ')' statement 'else' statement
Stmt *if_statement() {
  Token *token = expect_token(TK_IF);
  expect_token('(');
  Expr *if_cond = expression();
  expect_token(')');
  Stmt *then_body = statement();
  Stmt *else_body = read_token(TK_ELSE) ? statement() : NULL;

  Stmt *stmt = stmt_new(ND_IF, token);
  stmt->if_cond = if_cond;
  stmt->then_body = then_body;
  stmt->else_body = else_body;
  return stmt;
}

// while-statement :
//   'while' '(' expression ')' statement
Stmt *while_statement() {
  Token *token = expect_token(TK_WHILE);
  expect_token('(');
  Expr *while_cond = expression();
  expect_token(')');
  Stmt *while_body = statement();

  Stmt *stmt = stmt_new(ND_WHILE, token);
  stmt->while_cond = while_cond;
  stmt->while_body = while_body;
  return stmt;
}

// do-statement :
//   'do' statement 'while' '(' expression ')' ';'
Stmt *do_statement() {
  Token *token = expect_token(TK_DO);
  Stmt *do_body = statement();
  expect_token(TK_WHILE);
  expect_token('(');
  Expr *do_cond = expression();
  expect_token(')');
  expect_token(';');

  Stmt *stmt = stmt_new(ND_DO, token);
  stmt->do_cond = do_cond;
  stmt->do_body = do_body;
  return stmt;
}

// for-statement :
//   'for' '(' expression? ';' expression? ';' expression? ')' statement
//   'for' '(' declaration expression? ';' expression? ')' statement
Stmt *for_statement() {
  vector_push(symbol_scopes, map_new()); // begin for-statement scope

  Token *token = expect_token(TK_FOR);
  expect_token('(');
  Node *for_init;
  if (check_declaration_specifier()) {
    Decl *decl = declaration();
    if (check_typedef(decl->specs)) {
      error(decl->token, "typedef is not allowed in for-statement.");
    }
    for_init = (Node *) decl;
  } else {
    for_init = (Node *) (!check_token(';') ? expression() : NULL);
    expect_token(';');
  }
  Expr *for_cond = !check_token(';') ? expression() : NULL;
  expect_token(';');
  Expr *for_after = !check_token(')') ? expression() : NULL;
  expect_token(')');
  Stmt *for_body = statement();

  vector_pop(symbol_scopes); // end for-statement scope

  Stmt *stmt = stmt_new(ND_FOR, token);
  stmt->for_init = for_init;
  stmt->for_cond = for_cond;
  stmt->for_after = for_after;
  stmt->for_body = for_body;
  return stmt;
}

// continue-statement :
//   'continue' ';'
Stmt *continue_statement() {
  Token *token = expect_token(TK_CONTINUE);
  expect_token(';');

  return stmt_new(ND_CONTINUE, token);
}

// break-statement :
//   'break' ';'
Stmt *break_statement() {
  Token *token = expect_token(TK_BREAK);
  expect_token(';');

  return stmt_new(ND_BREAK, token);
}

// return-statement :
//   'return' expression? ';'
Stmt *return_statement() {
  Token *token = expect_token(TK_RETURN);
  Expr *ret = !check_token(';') ? expression() : NULL;
  expect_token(';');

  Stmt *stmt = stmt_new(ND_RETURN, token);
  stmt->ret = ret;
  return stmt;
}

// statement :
//   compound-statement
//   expression-statement
//   if-statement
//   while-statement
//   do-statement
//   continue-statement
//   break-statement
//   return-statement
Stmt *statement() {
  if (check_token(TK_IF))
    return if_statement();
  if (check_token(TK_WHILE))
    return while_statement();
  if (check_token(TK_DO))
    return do_statement();
  if (check_token(TK_FOR))
    return for_statement();
  if (check_token(TK_CONTINUE))
    return continue_statement();
  if (check_token(TK_BREAK))
    return break_statement();
  if (check_token(TK_RETURN))
    return return_statement();

  if (check_token('{')) {
    vector_push(symbol_scopes, map_new()); // begin block scope
    Stmt *stmt = compound_statement();
    vector_pop(symbol_scopes); // end block scope
    return stmt;
  }

  return expression_statement();
}

// parse translation-unit

// external-declaration :
//   function-definition
//   declaration
// function-definition :
//   declaration-specifiers declarator compound-statement
Node *external_declaration() {
  Token *token = peek_token();

  Vector *specs = declaration_specifiers();
  bool sp_typedef = check_typedef(specs);

  // If ';' follows declaration-specifiers,
  // this is declaration without declarators.
  if (read_token(';')) {
    return (Node *) decl_new(specs, vector_new(), token);
  }

  Symbol *symbol = declarator(sp_typedef, NULL);

  // If '{' does not follow the first declarator,
  // this is declaration with init-declarators.
  if (!check_token('{')) {
    Vector *symbols = vector_new();

    symbol->init = read_token('=') ? initializer() : NULL;
    vector_push(symbols, symbol);
    put_symbol(symbol->identifier, symbol);

    while (read_token(',')) {
      Symbol *symbol = init_declarator(sp_typedef);
      vector_push(symbols, symbol);
      put_symbol(symbol->identifier, symbol);
    }
    expect_token(';');

    return (Node *) decl_new(specs, symbols, token);
  }

  // Otherwise, this is function definition.
  if (sp_typedef) {
    error(token, "typedef is not allowed in function definition.");
  }
  if (!symbol->decl || symbol->decl->decl_type != DECL_FUNCTION) {
    error(symbol->token, "function definition should have function type.");
  }

  put_symbol(symbol->identifier, symbol);

  vector_push(symbol_scopes, symbol->decl->proto_scope);
  Stmt *body = compound_statement();
  vector_pop(symbol_scopes);

  return (Node *) func_new(specs, symbol, body, token);
}

// translation-unit :
//   external-declaration external-declaration*
TransUnit *translation_unit() {
  vector_push(symbol_scopes, map_new()); // begin file scope

  // __builtin_va_list
  Symbol *symbol_va_list = symbol_type("__builtin_va_list", NULL);
  put_symbol(symbol_va_list->identifier, symbol_va_list);

  Vector *decls = vector_new();
  do {
    vector_push(decls, external_declaration());
  } while (!check_token(TK_EOF));

  vector_pop(symbol_scopes); // end file scope

  return trans_unit_new(literals, decls);
}

// parser

// remove newlines and white-spaces
// inspect pp-numbers
Vector *inspect_tokens(Vector *tokens) {
  Vector *parse_tokens = vector_new();

  for (int i = 0; i < tokens->length; i++) {
    Token *token = tokens->buffer[i];

    if (token->tk_type == TK_SPACE) continue;
    if (token->tk_type == TK_NEWLINE) continue;

    if (token->tk_type == TK_PP_NUMBER) {
      vector_push(parse_tokens, inspect_pp_number(token));
    } else {
      vector_push(parse_tokens, token);
    }
  }

  return parse_tokens;
}

TransUnit *parse(Vector *tokens) {
  tokens = inspect_tokens(tokens);
  scanner_init(tokens);

  literals = vector_new();
  symbol_scopes = vector_new();

  return translation_unit();
}
